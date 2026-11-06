/* virtio-input provider — modern-transport paravirt input driver.
 * Probes PCI 1af4:1052 (virtio input, modern-only: QEMU
 * virtio-keyboard-pci / virtio-tablet-pci with disable-legacy=on),
 * walks the virtio 1.0 PCI capability list, negotiates VERSION_1,
 * sets up split vrings via the common-config region, arms the eventq
 * with device-writable buffers, and decodes evdev-class events to
 * serial so the host oracle diffs them against QMP-injected input.
 *
 * Provenance: PCI cap-walk + config-space helpers derived from this
 * repo's providers/virtio-pci-modern/app/main.c (same author,
 * BSD-3-Clause, original work); vring/queue protocol from virtio 1.0
 * spec §2.7 (split virtqueues) + §5.10 (input device).
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include <uk/paging.h>
#include <uk/bus/pci.h>

static inline void outl(uint16_t p, uint32_t v)
{ __asm__ __volatile__("outl %0,%1" :: "a"(v), "dN"(p)); }
static inline uint32_t inl(uint16_t p)
{ uint32_t v; __asm__ __volatile__("inl %1,%0" : "=a"(v) : "dN"(p)); return v; }
static inline uint8_t inb(uint16_t p)
{ uint8_t v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "dN"(p)); return v; }

static uint32_t cfg_addr(struct pci_device *d, int reg)
{
	return 0x80000000u | ((uint32_t)d->addr.bus << 16) |
	    ((uint32_t)d->addr.devid << 11) |
	    ((uint32_t)d->addr.function << 8) | (reg & ~3u);
}
static uint32_t cfg32(struct pci_device *d, int reg)
{
	outl(0xcf8, cfg_addr(d, reg));
	return (reg & 2) ? (inl(0xcfc) >> 16) : inl(0xcfc);
}
static uint8_t cfg8(struct pci_device *d, int reg)
{
	outl(0xcf8, cfg_addr(d, reg & ~3u));
	return (inl(0xcfc) >> ((reg & 3) * 8)) & 0xff;
}

struct virtio_cap { uint8_t cfg_type, bar; uint32_t offset, length, notify_mul; };

static uint64_t bar_base(struct pci_device *dev, int barno)
{
	uint32_t lo = cfg32(dev, 0x10 + barno * 4);
	uint64_t b = lo & ~0xfu;
	if ((lo & 6) == 4)
		b |= (uint64_t)cfg32(dev, 0x10 + barno * 4 + 4) << 32;
	return b;
}

static uint64_t devmap(uint64_t phys, int pages)
{
	__vaddr_t va = uk_paging_page_kmap(uk_paging_pt_get_active(),
					 (__paddr_t)(phys & ~0xfffULL), pages, 0);
	return va == UK_PAL_VADDR_INV ? 0 : va + (phys & 0xfffULL);
}
static void mmio_w(uint64_t a, int w, uint32_t v)
{
	if (w == 4) *(volatile uint32_t *)(uintptr_t)a = v;
	else if (w == 2) *(volatile uint16_t *)(uintptr_t)a = (uint16_t)v;
	else *(volatile uint8_t *)(uintptr_t)a = (uint8_t)v;
}
static uint32_t mmio_r(uint64_t a, int w)
{
	if (w == 4) return *(volatile uint32_t *)(uintptr_t)a;
	if (w == 2) return *(volatile uint16_t *)(uintptr_t)a;
	return *(volatile uint8_t *)(uintptr_t)a;
}
static uint64_t virt_to_phys(void *p)
{ return uk_paging_virt_to_phys((__vaddr_t)p); }

/* ---- split virtqueue (virtio 1.0 §2.7) ------------------------------- */
struct vring_desc  { uint64_t addr; uint32_t len; uint16_t flags, next; };
struct vring_used_elem { uint32_t id, len; };
struct vq {
	struct vring_desc  *desc;
	volatile uint16_t  *avail;      /* avail.idx at +2, ring at +4 */
	volatile uint16_t  *used;       /* used.idx at +2, ring at +6 */
	uint64_t            notify_addr;
	uint16_t            num, last_used;
};

#define NEVBUF 8
#define NDEVS  4
struct virtio_input_event { uint16_t type, code; uint32_t value; };
static int nvqs;
struct vinput_dev {
	struct vq vqs[2];
	struct virtio_input_event evbuf[NEVBUF];
	uint64_t cbase, notify;
	uint32_t nmul;
	char name[64];
};
static struct vinput_dev devs[NDEVS];
static int ndevs;

static int vq_init(struct vinput_dev *vd, uint64_t cbase, uint64_t nbase,
		   uint32_t nmul, int q)
{
	struct vq *v = &vd->vqs[q];
	void *mem = uk_palloc(uk_alloc_get_default(), 1);
	if (!mem) return -1;
	memset(mem, 0, 4096);
	uint64_t phys = virt_to_phys(mem);
	uint16_t num = 16;
	v->desc = mem;
	v->avail = (uint16_t *)((char *)mem + 16 * sizeof *v->desc);
	v->used  = (uint16_t *)((char *)mem + 16 * sizeof *v->desc +
				(((6 + 2 * num) + 3) & ~3));
	v->num = num; v->last_used = 0;

	mmio_w(cbase + 0x16, 2, q);          /* queue_select */
	uint16_t hwmax = mmio_r(cbase + 0x18, 2);
	if (hwmax < num) { printf("vinput: q%d hwmax %u < %u\n", q, hwmax, num); return -2; }
	mmio_w(cbase + 0x18, 2, num);        /* queue_size */
	/* modern cfg: queue_desc(0x20) queue_avail(0x28) queue_used(0x30) are 64b */
	mmio_w(cbase + 0x20, 4, (uint32_t)phys); mmio_w(cbase + 0x24, 4, phys >> 32);
	mmio_w(cbase + 0x28, 4, (uint32_t)(phys + 16 * sizeof *v->desc));
	mmio_w(cbase + 0x2c, 4, 0);
	uint64_t used_off = (char *)v->used - (char *)mem;
	mmio_w(cbase + 0x30, 4, (uint32_t)(phys + used_off));
	mmio_w(cbase + 0x34, 4, 0);
	uint16_t noff = mmio_r(cbase + 0x1e, 2);      /* queue_notify_off */
	v->notify_addr = nbase + (uint64_t)noff * nmul;
	mmio_w(cbase + 0x1c, 2, 1);          /* queue_enable */
	{   /* readback: does the device hold the addresses we wrote? */
		uint64_t rd = (uint64_t)mmio_r(cbase + 0x24, 4) << 32 | mmio_r(cbase + 0x20, 4);
		uint64_t ra = (uint64_t)mmio_r(cbase + 0x2c, 4) << 32 | mmio_r(cbase + 0x28, 4);
		uint64_t ru = (uint64_t)mmio_r(cbase + 0x34, 4) << 32 | mmio_r(cbase + 0x30, 4);
		printf("vinput: q%d desc=%llx avail=%llx used=%llx en=%u\n", q, rd, ra, ru,
		       mmio_r(cbase + 0x1c, 2));
	}
	return 0;
}

static void vq_kick(struct vinput_dev *vd, int q)
{
	__sync_synchronize();
	mmio_w(vd->vqs[q].notify_addr, 2, q);
}

static int vq_used(struct vq *v)
{
	__sync_synchronize();
	if (v->last_used == v->used[1]) return -1;
	uint16_t slot = v->last_used++ % v->num;
	struct vring_used_elem *e =
		(struct vring_used_elem *)((char *)v->used + 4) + slot;
	return e->id;
}

static int found;

static void read_devcfg_name(struct vinput_dev *vd, uint64_t dcfg)
{
	memset(vd->name, 0, sizeof vd->name);
	if (!dcfg) { printf("vinput: no device cfg cap\n"); return; }
	mmio_w(dcfg + 0, 1, 1);                 /* select = ID_NAME */
	mmio_w(dcfg + 1, 1, 0);                 /* subsel = 0 */
	uint8_t sz = mmio_r(dcfg + 2, 1);
	if (sz > sizeof vd->name - 1) sz = sizeof vd->name - 1;
	for (int i = 0; i < sz; i++) vd->name[i] = mmio_r(dcfg + 8 + i, 1);
	printf("vinput: device name '%s'\n", vd->name);
}

static int add(struct pci_device *dev)
{
	struct virtio_cap caps[8]; int ncaps = 0, hops = 0;
	struct virtio_cap *common = NULL, *notify = NULL, *isr = NULL, *dc = NULL;

	found = 1;
	printf("vinput: virtio-input %04x:%04x\n",
	       dev->id.vendor_id, dev->id.device_id);
	uint8_t cp = cfg8(dev, 0x34);
	while (cp && cp >= 0x40 && hops++ < 32 && ncaps < 8) {
		uint8_t id = cfg8(dev, cp), nxt = cfg8(dev, cp + 1);
		uint8_t len = cfg8(dev, cp + 2);
		if (id == 0x09 && len >= 16) {
			struct virtio_cap *c = &caps[ncaps];
			c->cfg_type = cfg8(dev, cp + 3); c->bar = cfg8(dev, cp + 4);
			c->offset = cfg32(dev, cp + 8); c->length = cfg32(dev, cp + 12);
			c->notify_mul = (len >= 20) ? cfg32(dev, cp + 16) : 0;
			ncaps++;
		}
		cp = nxt;
	}
	for (int i = 0; i < ncaps; i++) {
		if (caps[i].cfg_type == 1) common = &caps[i];
		if (caps[i].cfg_type == 2) notify = &caps[i];
		if (caps[i].cfg_type == 3) isr = &caps[i];
		if (caps[i].cfg_type == 4) dc = &caps[i];
	}
	if (!common || !notify) { printf("vinput: refused: no common/notify cap\n"); return -3; }
	if (ndevs >= NDEVS) { printf("vinput: refused: too many devices\n"); return -3; }
	struct vinput_dev *vd = &devs[ndevs];
	uint64_t cbase = devmap(bar_base(dev, common->bar) + common->offset, 2);
	uint64_t nbase = devmap(bar_base(dev, notify->bar) + notify->offset, 2);
	vd->nmul = notify->notify_mul;
	uint64_t dcfg = dc ? devmap(bar_base(dev, dc->bar) + dc->offset, 1) : 0;
	if (!cbase || !nbase) { printf("vinput: refused: bar map failed\n"); return -4; }
	vd->cbase = cbase; vd->notify = nbase;
	printf("vinput: common=%lx notify=%lx mul=%u\n",
	       (unsigned long)cbase, (unsigned long)nbase, vd->nmul);

	/* device_status negotiation: reset → ACK → DRIVER → FEATURES_OK */
	mmio_w(cbase + 0x14, 1, 0);
	mmio_w(cbase + 0x14, 1, 1);
	mmio_w(cbase + 0x14, 1, 1 | 2);
	mmio_w(cbase + 0x08, 4, 0); mmio_w(cbase + 0x0c, 4, 0);
	mmio_w(cbase + 0x08, 4, 1); mmio_w(cbase + 0x0c, 4, 1);  /* VERSION_1 */
	uint8_t st = mmio_r(cbase + 0x14, 1);
	mmio_w(cbase + 0x14, 1, st | 8);
	if (!(mmio_r(cbase + 0x14, 1) & 8)) { printf("vinput: refused: FEATURES_OK\n"); return -5; }

	read_devcfg_name(vd, dcfg);
	nvqs = mmio_r(cbase + 0x12, 2);
	printf("vinput: queues=%u\n", nvqs);
	if (vq_init(vd, cbase, nbase, vd->nmul, 0) ||
	    vq_init(vd, cbase, nbase, vd->nmul, 1)) {
		printf("vinput: refused: vq setup\n"); return -6;
	}
	/* arm eventq: NEVBUF device-writable descriptors */
	for (int i = 0; i < NEVBUF; i++) {
		vd->vqs[0].desc[i].addr = virt_to_phys(&vd->evbuf[i]);
		vd->vqs[0].desc[i].len = sizeof vd->evbuf[i];
		vd->vqs[0].desc[i].flags = 2;           /* write */
	}
	for (int i = 0; i < NEVBUF; i++)
		vd->vqs[0].avail[2 + i] = i;
	__sync_synchronize();
	vd->vqs[0].avail[1] = NEVBUF;               /* avail.idx */
	mmio_w(cbase + 0x14, 1, mmio_r(cbase + 0x14, 1) | 4);   /* DRIVER_OK */
	vq_kick(vd, 0);
	printf("vinput: DRIVER_OK, eventq armed (%s)\n", vd->name);
	ndevs++;
	return 0;
}

static const struct pci_device_id vin_ids[] = {
	{ PCI_DEVICE_ID(0x1af4, 0x1052) },   /* virtio-input modern */
	{ PCI_ANY_DEVICE_ID },
};
static struct pci_driver vin_drv = { .device_ids = vin_ids, .add_dev = add };
PCI_REGISTER_DRIVER(&vin_drv);

static const char *evkey(uint16_t code)
{
	switch (code) { case 30: return "a"; case 48: return "b"; }
	return "key";
}

int main(void)
{
	printf("virtio-input: guest up\n");
	if (!found) { printf("virtio-input: no devices present\n"); return 0; }
	for (;;) {
		for (int d = 0; d < ndevs; d++) {
			struct vinput_dev *vd = &devs[d];
			struct vq *vq = &vd->vqs[0];
			int id = vq_used(vq);
			if (id < 0) continue;
			struct virtio_input_event *e = &vd->evbuf[id];
			printf("virtio-input: ev type=%u code=%u val=%u",
			       e->type, e->code, e->value);
			if (e->type == 1)
				printf(" %s=%s", evkey(e->code), e->value ? "down" : "up");
			else if (e->type == 3)
				printf(" abs%u=%u", e->code, e->value);
			else if (e->type == 2)
				printf(" rel%u=%d", e->code, (int)e->value);
			printf("\n");
			/* recycle the descriptor onto the device's eventq */
			vq->avail[2 + (vq->avail[1] % vq->num)] = id;
			__sync_synchronize();
			vq->avail[1]++;
			vq_kick(vd, 0);
		}
	}
	return 0;
}
