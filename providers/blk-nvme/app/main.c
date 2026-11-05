/* blk-nvme acceptance guest — QEMU NVMe controller + namespace driver.
 *
 * Lane: donor-c. This is real driver authorship, not composition:
 * Unikraft has virtio-blk only; there is no NVMe driver in-tree or in
 * the lib-* catalog. Provenance: written against the NVMe 1.4 spec and
 * the QEMU nvme device model (vendor 0x1b36, device 0x0010); the PCI
 * registration shape follows Unikraft's ukbus/pci driver pattern
 * (drivers/ukbus/pci @ the unikraft-fork pin in origins.lock, BSD-3).
 *
 * The acceptance contract (providers/blk-nvme/entry.md): QEMU presents
 * -device nvme,serial=korudesktop backed by a host image file. We
 * identify the controller + namespace, prove the serial is
 * 'korudesktop', read the host-planted pattern at LBA 8, write a
 * second pattern at LBA 16, flush, and say so on serial — the host
 * then dd/cmp's the image bytes, which the guest cannot fake.
 *
 * Deliberate scope for tier:qemu: polled completion queues (MSI-X is
 * the irq-msix rung later), PRP1-only payloads (one 4K page covers the
 * 512-byte-sector IO the acceptance contract needs), namespace 1 only.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include <uk/bus/pci.h>
#include <uk/plat/time.h>
#include <uk/paging.h>
#include <uk/arch/util.h>

/* ---- NVMe register + command vocabulary ------------------------------ */
#define NVME_CAP   0x00
#define NVME_VS    0x08
#define NVME_CC    0x14
#define NVME_CSTS  0x1c
#define NVME_AQA   0x24
#define NVME_ASQ   0x28
#define NVME_ACQ   0x30
#define NVME_DBBL  0x1000   /* doorbell base; stride = CAP.DSTRD*4 = 4 here */

#define NVME_CC_EN      (1u << 0)
#define NVME_CSTS_RDY   (1u << 0)
#define NVME_CSTS_CFS   (1u << 1)

#define NVME_OPC_FLUSH      0x00
#define NVME_OPC_WRITE      0x01
#define NVME_OPC_READ       0x02
#define NVME_OPC_IDENTIFY   0x06
#define NVME_OPC_CREATE_IO_CQ 0x05
#define NVME_OPC_CREATE_IO_SQ 0x01

#define ADMIN_Q 0
#define IO_Q    1
#define QDEPTH  16

struct nvme_sqe {
	uint8_t  opc;
	uint8_t  fuse;
	uint16_t cid;
	uint32_t nsid;
	uint64_t rsvd;
	uint64_t mptr;
	uint64_t prp1;
	uint64_t prp2;
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
};

struct nvme_cqe {
	uint32_t result;   /* DW0 command-specific */
	uint32_t rsvd;     /* DW1 */
	uint16_t head;     /* DW2 low  - SQ head pointer */
	uint16_t sqid;     /* DW2 high - SQ identifier */
	uint16_t cid;      /* DW3 low  - command id */
	uint16_t status;   /* DW3 high - SF<<1 | phase */
};

struct nvme_queue {
	struct nvme_sqe *sq;
	struct nvme_cqe *cq;
	uint32_t sq_tail;
	uint32_t cq_head;
	uint8_t  cq_phase;
	uint16_t cid;
	uint32_t qid;
	volatile uint8_t *bar0;
};

static volatile uint8_t *g_bar0;
static uint16_t g_block_shift = 9;   /* 512B default until Identify NS says */
static uint32_t g_sq_db;
static uint32_t g_cq_db;

static void nvme_delay(uint32_t us)
{
	__nsec end = ukplat_monotonic_clock() + us * 1000ULL;
	while (ukplat_monotonic_clock() < end) { }
}

static uint32_t r32(uint32_t off) { return *(volatile uint32_t *)(g_bar0 + off); }
static void w32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(g_bar0 + off) = v; }
static void w64(uint32_t off, uint64_t v) {
	w32(off, (uint32_t)v);
	w32(off + 4, (uint32_t)(v >> 32));
}

static void db_sq(struct nvme_queue *q)
{
	w32(g_sq_db + q->qid * 8, q->sq_tail);
}
static void db_cq(struct nvme_queue *q)
{
	w32(g_cq_db + q->qid * 8, q->cq_head);
}

/* Submit one command, poll its completion, return 0 / -EIO. */
static int nvme_cmd(struct nvme_queue *q, struct nvme_sqe *e)
{
	uint32_t timeout = 5000000;   /* ~5s of busy polling */
	struct nvme_cqe *c;

	e->cid = q->cid++;
	e->mptr = 0;
	e->prp2 = 0;
	q->sq[q->sq_tail] = *e;
	__sync_synchronize();
	q->sq_tail = (q->sq_tail + 1) % QDEPTH;
	db_sq(q);

	for (;;) {
		__sync_synchronize();
		c = &q->cq[q->cq_head];
		if ((c->status & 1) == q->cq_phase) {
			uint16_t sc = c->status >> 1;
			if (sc)
				printf("nvme: cmd opc=%x failed status=%x cid=%x sqid=%x res=%x head=%u phase=%u\n",
				       e->opc, sc, c->cid, c->sqid, c->result,
				       q->cq_head, q->cq_phase);
			q->cq_head++;
			if (q->cq_head == QDEPTH) {
				q->cq_head = 0;
				q->cq_phase ^= 1;
			}
			db_cq(q);
			return sc ? -5 : 0;
		}
		if (--timeout == 0) {
			printf("nvme: cmd timeout opc=%x tail=%u head=%u cqe0={res=%x st=%x} sq0={opc=%x cid=%x} csts=%x\n",
			       q->sq[(q->sq_tail ? q->sq_tail : QDEPTH) - 1].opc,
			       q->sq_tail, q->cq_head,
			       q->cq[0].result, q->cq[0].status,
			       q->sq[0].opc, q->sq[0].cid, r32(NVME_CSTS));
			return -5;
		}
		nvme_delay(1);
	}
}

static int nvme_io(struct nvme_queue *q, uint8_t op, uint32_t lba,
		   void *buf)
{
	struct nvme_sqe e;
	memset(&e, 0, sizeof e);
	e.opc = op;
	e.nsid = 1;
	e.prp1 = (uint64_t)uk_paging_virt_to_phys((__vaddr_t)buf);
	e.cdw10 = lba;
	e.cdw11 = 0;          /* LBA high */
	e.cdw12 = 0;          /* 1 block */
	return nvme_cmd(q, &e);
}

static struct uk_alloc *a;
static struct nvme_queue admin, io;
static void *id_buf, *io_buf;

static int nvme_add_dev(struct pci_device *dev);
static int nvme_pci_init(struct uk_alloc *al);

static const struct pci_device_id nvme_ids[] = {
	{ PCI_DEVICE_ID(0x1b36, 0x0010) },
	{ PCI_ANY_DEVICE_ID },
};

static struct pci_driver nvme_drv = {
	.device_ids = nvme_ids,
	.init = nvme_pci_init,
	.add_dev = nvme_add_dev,
};
_PCI_REGISTER_DRIVER(appblknvme, &nvme_drv);

static int nvme_pci_init(struct uk_alloc *al)
{
	a = al;
	return 0;
}

static int nvme_add_dev(struct pci_device *dev)
{
	uint64_t cap;
	uint32_t cc;
	uint64_t t;

	/* dev->base is only the LOW 16 bits of BAR0 — the x86 enumerator
	 * does PCI_CONF_READ(__u16, &dev->base, IOBAR). QEMU's nvme BAR is
	 * a 64-bit MMIO region, so read the full BAR ourselves: config
	 * addr = 0x80000000|bus<<16|dev<<11|func<<8, BAR0 at offset 0x10
	 * (+0x14 for the high dword on a 64-bit BAR), flags in bits 0-3.
	 */
	{
		uint32_t caddr = 0x80000000u
			| ((uint32_t)dev->addr.bus << 16)
			| ((uint32_t)dev->addr.devid << 11)
			| ((uint32_t)dev->addr.function << 8);
		uint64_t bar_phys;
		uk_arch_x86_64_outl(PCI_CONFIG_ADDR, caddr | 0x10);
		bar_phys = uk_arch_x86_64_inl(PCI_CONFIG_DATA);
		if ((bar_phys & 0x6) == 0x4) {          /* 64-bit BAR */
			uk_arch_x86_64_outl(PCI_CONFIG_ADDR, caddr | 0x14);
			bar_phys |= (uint64_t)uk_arch_x86_64_inl(PCI_CONFIG_DATA) << 32;
		}
		bar_phys &= ~0xfULL;
		printf("nvme: dev %02x:%02x.%x bar0 phys=%lx\n",
		       dev->addr.bus, dev->addr.devid, dev->addr.function,
		       (unsigned long)bar_phys);
		g_bar0 = (volatile uint8_t *)uk_paging_page_kmap(
			uk_paging_pt_get_active(), (__paddr_t)bar_phys, 4, 0);
		if (g_bar0 == (volatile uint8_t *)UK_PAL_VADDR_INV) {
			printf("nvme: refused: cannot map BAR0 phys %lx\n",
			       (unsigned long)bar_phys);
			return -19;
		}
	}
	cap = r32(NVME_CAP) | ((uint64_t)r32(NVME_CAP + 4) << 32);
	g_sq_db = NVME_DBBL;
	g_cq_db = NVME_DBBL + 4 * ((cap >> 32 & 0xf) + 1);
	printf("nvme: dev %02x:%02x.%x bar0=%p cap=%llx vs=%08x\n",
	       dev->addr.bus, dev->addr.devid, dev->addr.function,
	       g_bar0, (unsigned long long)cap, r32(NVME_VS));

	/* Reset: EN=0, wait RDY=0 */
	w32(NVME_CC, r32(NVME_CC) & ~NVME_CC_EN);
	for (t = 0; t < 5000 && (r32(NVME_CSTS) & NVME_CSTS_RDY); t++)
		nvme_delay(1000);

	/* Admin queues: one page each, zeroed CQs */
	admin.sq = uk_palloc(a, 1);
	admin.cq = uk_palloc(a, 1);
	io.sq = uk_palloc(a, 1);
	io.cq = uk_palloc(a, 1);
	id_buf = uk_palloc(a, 1);
	io_buf = uk_palloc(a, 1);
	if (!admin.sq || !admin.cq || !io.sq || !io.cq || !id_buf || !io_buf)
		return -12;
	memset(admin.cq, 0, 4096);
	memset(io.cq, 0, 4096);
	admin.qid = ADMIN_Q;
	admin.cq_phase = 1;
	io.qid = IO_Q;
	io.cq_phase = 1;
	w32(NVME_AQA, (QDEPTH - 1) | ((QDEPTH - 1) << 16));
	printf("nvme: asq va=%p phys=%lx acq va=%p phys=%lx\n",
	       admin.sq, (unsigned long)uk_paging_virt_to_phys((__vaddr_t)admin.sq),
	       admin.cq, (unsigned long)uk_paging_virt_to_phys((__vaddr_t)admin.cq));
	w64(NVME_ASQ, (uint64_t)uk_paging_virt_to_phys((__vaddr_t)admin.sq));
	w64(NVME_ACQ, (uint64_t)uk_paging_virt_to_phys((__vaddr_t)admin.cq));
	printf("nvme: asq reg=%llx acq reg=%llx\n",
	       (unsigned long long)(r32(NVME_ASQ) | ((uint64_t)r32(NVME_ASQ + 4) << 32)),
	       (unsigned long long)(r32(NVME_ACQ) | ((uint64_t)r32(NVME_ACQ + 4) << 32)));

	/* Enable: CC.EN=1, wait RDY */
	/* CC: EN + IOSQES=6 (64B entries) + IOCQES=4 (16B) — leaving the
	 * entry-size fields at 0 makes the controller read 16-byte SQEs
	 * and every admin command fails. */
	cc = NVME_CC_EN | (6u << 16) | (4u << 20);
	w32(NVME_CC, cc);
	for (t = 0; t < 5000 && !(r32(NVME_CSTS) & NVME_CSTS_RDY); t++)
		nvme_delay(1000);
	if (!(r32(NVME_CSTS) & NVME_CSTS_RDY) || (r32(NVME_CSTS) & NVME_CSTS_CFS)) {
		printf("nvme: refused: controller never ready (csts=%08x)\n", r32(NVME_CSTS));
		return -19;
	}
	printf("nvme: controller ready\n");

	/* Identify Controller — serial is the contract's first proof. */
	{
		struct nvme_sqe e;
		char serial[21];
		memset(&e, 0, sizeof e);
		e.opc = NVME_OPC_IDENTIFY;
		e.prp1 = (uint64_t)uk_paging_virt_to_phys((__vaddr_t)id_buf);
		e.cdw10 = 1;              /* CNS=1: controller */
		if (nvme_cmd(&admin, &e)) {
			printf("nvme: refused: identify controller failed\n");
			return -5;
		}
		memcpy(serial, (char *)id_buf + 4, 20);
		serial[20] = 0;
		/* QEMU pads serial with spaces; trim. */
		{
			int n = 20;
			while (n && serial[n - 1] == ' ') serial[--n] = 0;
		}
		printf("nvme: controller serial '%s'\n", serial);
		if (strcmp(serial, "korudesktop") != 0) {
			printf("nvme: refused: serial '%s' != 'korudesktop'\n", serial);
			return -19;
		}
	}

	/* Identify Namespace 1 — block size. */
	{
		struct nvme_sqe e;
		uint8_t *d = id_buf;
		memset(&e, 0, sizeof e);
		e.opc = NVME_OPC_IDENTIFY;
		e.nsid = 1;
		e.prp1 = (uint64_t)uk_paging_virt_to_phys((__vaddr_t)id_buf);
		e.cdw10 = 0;              /* CNS=0: namespace */
		if (nvme_cmd(&admin, &e)) {
			printf("nvme: refused: identify namespace failed\n");
			return -5;
		}
		{
			uint8_t flbas = d[26] & 0xf;
			uint8_t lbads = d[128 + flbas * 4 + 2];
			if (lbads)
				g_block_shift = lbads;
			printf("nvme: namespace 1 identified, %u-byte blocks, nsze=%llu\n",
			       1u << g_block_shift,
			       (unsigned long long)(*(uint64_t *)d));
		}
	}

	/* IO queues: CQ then SQ. */
	{
		struct nvme_sqe e;
		memset(&e, 0, sizeof e);
		e.opc = NVME_OPC_CREATE_IO_CQ;
		e.prp1 = (uint64_t)uk_paging_virt_to_phys((__vaddr_t)io.cq);
		e.cdw10 = IO_Q | ((QDEPTH - 1) << 16);
		e.cdw11 = 1;             /* contiguous, IRQs off (polled) */
		if (nvme_cmd(&admin, &e))
			return -5;
		memset(&e, 0, sizeof e);
		e.opc = NVME_OPC_CREATE_IO_SQ;
		e.prp1 = (uint64_t)uk_paging_virt_to_phys((__vaddr_t)io.sq);
		e.cdw10 = IO_Q | ((QDEPTH - 1) << 16);
		e.cdw11 = (IO_Q << 16) | 1;
		if (nvme_cmd(&admin, &e))
			return -5;
	}
	printf("nvme: io queues up\n");
	return 0;
}

int main(void)
{
	int r;
	char expect[64];
	const char *want_a = "KORUDESKTOP-HOST-PATTERN-A";

	printf("blk-nvme: boot\n");

	/* Read the host-planted pattern at LBA 8. */
	memset(io_buf, 0, 4096);
	r = nvme_io(&io, NVME_OPC_READ, 8, io_buf);
	printf("nvme: read lba8 rc=%d\n", r);
	if (r == 0) {
		memcpy(expect, io_buf, 26);
		expect[26] = 0;
		printf("nvme: lba8 starts '%s'\n", expect);
		if (memcmp(io_buf, want_a, 26) == 0)
			printf("nvme: host pattern A verified at lba8\n");
		else
			printf("nvme: refused: lba8 payload is not pattern A\n");
	}

	/* Write the second pattern at LBA 16, flush, confirm. */
	memset(io_buf, 0, 512);
	memcpy(io_buf, "KORUDESKTOP-GUEST-PATTERN-B", 27);
	r = nvme_io(&io, NVME_OPC_WRITE, 16, io_buf);
	printf("nvme: write lba16 rc=%d\n", r);
	{
		struct nvme_sqe e;
		memset(&e, 0, sizeof e);
		e.opc = NVME_OPC_FLUSH;
		e.nsid = 1;
		r = nvme_cmd(&io, &e);   /* NVM command set — io queue, not admin */
	}
	printf("nvme: flush rc=%d\n", r);

	/* Read it back in-guest as a sanity line (host dd/cmp is the oracle). */
	memset(io_buf, 0, 512);
	r = nvme_io(&io, NVME_OPC_READ, 16, io_buf);
	printf("nvme: readback lba16 rc=%d '%s'\n", r, r ? "?" : (char *)io_buf);
	printf("blk-nvme: done\n");
	return 0;
}
