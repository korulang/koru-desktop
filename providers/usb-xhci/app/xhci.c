/* usb-xhci: minimal xHCI driver — controller init, command/event/EP0
 * transfer rings, port scan, EnableSlot, AddressDevice, GET_DESCRIPTOR.
 * Provenance: written from the xHCI 1.2 spec (Intel rev 1.2) for this
 * slot; no code lifted. The register/ring vocabulary is the spec's.
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>

#include <uk/paging.h>
#include <uk/bus/pci.h>

#define DBG(...) printf(__VA_ARGS__)

/* PCI config space via CF8/CFC — the ukbus x86_64 bus populates
 * dev->base from the legacy 16-bit IOBAR only (Solo5 heritage); MMIO
 * BARs are read here directly. */
static inline void outl(uint16_t p, uint32_t v)
{
	__asm__ __volatile__("outl %0,%1" :: "a"(v), "dN"(p));
}
static inline uint32_t inl(uint16_t p)
{
	uint32_t v;
	__asm__ __volatile__("inl %1,%0" : "=a"(v) : "dN"(p));
	return v;
}
static uint32_t cfg32(struct pci_device *d, int reg)
{
	uint32_t a = 0x80000000u | ((uint32_t)d->addr.bus << 16) |
	    ((uint32_t)d->addr.devid << 11) |
	    ((uint32_t)d->addr.function << 8) | (reg & ~3u);
	outl(0xcf8, a);
	uint32_t v = inl(0xcfc);
	return (reg & 2) ? (v >> 16) : v;
}

/* ---- capability regs (off 0) ---- */
#define CAP_CAPLENGTH(v)	((v) & 0xff)
#define CAP_HCIVERSION(v)	(((v) >> 16) & 0xffff)
#define CAP_HCSPARAMS1_MAXSLOTS(v) ((v) & 0xff)
#define CAP_HCSPARAMS1_MAXINTS(v) (((v) >> 8) & 0x7ff)
#define CAP_HCSPARAMS1_MAXPORTS(v) (((v) >> 24) & 0xff)
#define CAP_HCSPARAMS2_SCRATCH(v)  (((v) >> 27) & 0x1f)
#define CAP_DBOFF(v)		((v) & ~3u)
#define CAP_RTSOFF(v)		((v) & ~0x1fu)
/* ---- operational regs (off CAPLENGTH) ---- */
#define OP_USBCMD	0x00
#define OP_USBSTS	0x04
#define OP_DNCTRL	0x14
#define OP_CRCR		0x18
#define OP_DCBAAP	0x30
#define OP_CONFIG	0x38
#define OP_PORTSC(i)	(0x400 + 0x10 * (i))
#define USBCMD_RS	(1u << 0)
#define USBCMD_HCRST	(1u << 1)
#define USBCMD_INTE	(1u << 2)
#define USBSTS_HCH	(1u << 0)
#define USBSTS_CNR	(1u << 11)
/* PORTSC bits */
#define PSC_CCS		(1u << 0)
#define PSC_PED		(1u << 1)
#define PSC_PR		(1u << 4)
#define PSC_SPEED(v)	(((v) >> 10) & 0xf)
#define PSC_WRC		((1u << 8)|(1u << 9)|(1u << 16)|(1u << 17)|(1u << 18)|(1u << 19)|(1u << 20)|(1u << 21)|(1u << 22)|(1u << 23)|(1u << 24))
/* ---- runtime regs (off RTSOFF) ---- */
#define RT_MFINDEX	0x00
#define RT_IR0		0x20
#define IR_IMAN		0x00
#define IR_IMOD		0x04
#define IR_ERSTSZ	0x08
#define IR_ERSTBA	0x10
#define IR_ERDP		0x18
#define IMAN_IE		(1u << 1)
#define IMAN_IP		(1u << 0)
#define ERDP_EHB	(1u << 3)
/* ---- TRB ---- */
struct trb { uint32_t p_lo, p_hi, st, ctl; } __attribute__((packed));
#define TRB_CYCLE	(1u << 0)
#define TRB_TC		(1u << 1)
#define TRB_LINK	(1u << 2)
#define TRB_TYPE(v)	(((v) & 0x3f) << 10)
#define TRB_TYPE_CMD	1
#define TRB_TYPE_CPL	33
#define TRB_TYPE_PSC	34
#define TRB_TYPE_XFER	32
#define TRB_TYPE_SETUP	2
#define TRB_TYPE_DATA	3
#define TRB_TYPE_STATUS	4
#define TRB_TYPE_LINK	6
#define TRB_TYPE_ENSLOT	9
#define TRB_TYPE_ADDR	11
#define TRB_TYPE_CFGEP	12
#define TRB_TYPE_EVAL	13
#define TRB_IDT		(1u << 6)
#define TRB_IOC		(1u << 5)
/* slot/endpoint contexts — 32 bytes each */
struct ctx32 { uint32_t d[8]; };
/* input ctrl ctx: add/drop flags at d[0..1] */
struct input_ctx { struct ctx32 ctrl; struct ctx32 slot; struct ctx32 ep[31]; };
struct dev_ctx  { struct ctx32 slot; struct ctx32 ep[31]; };

#define SLOT_CTX_ENTRIES(v) ((v) << 27)
#define SLOT_CTX_RHPORT(v)  ((v) << 16)
#define SLOT_CTX_SPEED(v)   ((v) << 20)
#define EP_CTX_TYPE_CTRL	(4u << 3)
#define EP_CTX_MPS(v)		(((v) & 0xffff) << 16)
#define EP_CTX_IVAL(v)		((v) << 16)
#define EP_CTX_CERR(v)		(((v) & 3) << 1)

struct ring { struct trb *trbs; int n, enq, deq; int pcs; };

static uint32_t rd32(uint64_t b, uint32_t o) { return *(volatile uint32_t *)(uintptr_t)(b + o); }
static void wr32(uint64_t b, uint32_t o, uint32_t v) { *(volatile uint32_t *)(uintptr_t)(b + o) = v; }
static void wr64(uint64_t b, uint32_t o, uint64_t v) { *(volatile uint64_t *)(uintptr_t)(b + o) = v; }

static uint64_t op_base, rt_base, db_base;
static struct ring cring, ering, ep0;
static struct input_ctx *ic;
static struct dev_ctx *dctx;
static struct trb *erst;
static uint64_t *dcbaa;
static int max_ports, slot_id;

static uint64_t phys(void *p) { return uk_paging_virt_to_phys((__vaddr_t)p); }

static void ring_init(struct ring *r, struct trb *t, int n)
{
	memset(t, 0, n * sizeof(*t));
	r->trbs = t; r->n = n; r->enq = 0; r->deq = 0; r->pcs = 1;
	/* link trb at end: point back to start, TC so consumer cycle flips */
	t[n - 1].p_lo = (uint32_t)(phys(t) & ~0xfu);
	t[n - 1].p_hi = (uint32_t)(phys(t) >> 32);
	t[n - 1].st = 0;
	t[n - 1].ctl = TRB_TYPE(TRB_TYPE_LINK) | TRB_TC;
}

static void ring_push(struct ring *r, uint64_t p, uint32_t st, uint32_t ctl)
{
	struct trb *t = &r->trbs[r->enq];
	t->p_lo = (uint32_t)p; t->p_hi = (uint32_t)(p >> 32);
	t->st = st;
	t->ctl = ctl | r->pcs;
	r->enq++;
	if (r->enq >= r->n - 1) {
		struct trb *lk = &r->trbs[r->n - 1];
		lk->ctl = TRB_TYPE(TRB_TYPE_LINK) | TRB_TC | r->pcs;
		r->enq = 0;
		r->pcs ^= 1;
	}
	/* fence */
}

static void doorbell(int slot, int target)
{
	wr32(db_base, slot * 4, target);
}

/* Poll the event ring for a completion TRB whose TRB-pointer field
 * matches `want` (0 = any). Advances dequeue, acknowledges EHB. */
static int event_wait(struct ring *ev, uint64_t want, uint32_t *cc, uint32_t *slot)
{
	int spins = 50000000;
	while (spins--) {
		struct trb *t = &ev->trbs[ev->deq];
		if ((t->ctl & 1) != ev->pcs) { continue; }
		uint64_t ptr = ((uint64_t)t->p_hi << 32) | t->p_lo;
		uint32_t type = (t->ctl >> 10) & 0x3f;
		uint32_t ccode = (t->st >> 24) & 0xff;
		ev->deq++;
		if (ev->deq >= ev->n - 1) { ev->deq = 0; ev->pcs ^= 1; }
		/* release event: ERDP = dequeue phys | EHB */
		uint64_t dp = phys(&ev->trbs[ev->deq]);
		wr64(rt_base, RT_IR0 + IR_ERDP, dp | ERDP_EHB);
		/* fence */
		if (type == TRB_TYPE_CPL || type == TRB_TYPE_XFER || type == TRB_TYPE_PSC) {
			if (!want || ptr == want) {
				if (cc) *cc = ccode;
				if (slot) *slot = (t->ctl >> 24) & 0xff;
				return (int)type;
			}
		}
	}
	return -1;
}

static int xcmd(uint64_t p, uint32_t st, uint32_t ctl)
{
	uint64_t tptr = phys(&cring.trbs[cring.enq]);
	ring_push(&cring, p, st, ctl);
	doorbell(0, 0);

	uint32_t cc = 0, sl = 0;
	int r = event_wait(&ering, tptr, &cc, &sl);
	if (r < 0) { DBG("xhci: cmd timeout type=%u", (ctl >> 10) & 0x3f); return -1; }
	if (cc != 1) { DBG("xhci: cmd type=%u cc=%u", (ctl >> 10) & 0x3f, cc); return -1; }
	return sl;
}

/* GET_DESCRIPTOR(Device) on ep0's transfer ring */
static int ep0_get_desc(uint8_t *buf)
{
	struct trb *t = &ep0.trbs[ep0.enq];
	uint64_t tptr = phys(t);
	uint64_t setup = 0;
	/* GET_DESCRIPTOR: bmRT 0x80, bReq 6, wValue 0x0100, wIndex 0, wLength 18 */
	setup = (uint64_t)0x80 | (6ull << 8) | (0x0100ull << 16) | (18ull << 48);
	/* setup stage — TRT=3 (in-data), IDT */
	t->p_lo = (uint32_t)setup; t->p_hi = (uint32_t)(setup >> 32);
	t->st = 8; t->ctl = TRB_TYPE(TRB_TYPE_SETUP) | TRB_IDT | (3u << 16) | ep0.pcs;
	ep0.enq++;
	t = &ep0.trbs[ep0.enq];
	t->p_lo = (uint32_t)phys(buf); t->p_hi = (uint32_t)(phys(buf) >> 32);
	t->st = 18; /* TRB transfer length = full device descriptor */
	t->ctl = TRB_TYPE(TRB_TYPE_DATA) | (1u << 16) | TRB_IOC | ep0.pcs; /* DIR=in */
	ep0.enq++;
	t = &ep0.trbs[ep0.enq];
	t->p_lo = 0; t->p_hi = 0; t->st = 0;
	t->ctl = TRB_TYPE(TRB_TYPE_STATUS) | TRB_IOC | ep0.pcs;
	uint64_t st_ptr = phys(t);
	ep0.enq++;
	(void)tptr;
	doorbell(slot_id, 1);
	uint32_t cc = 0, sl = 0;
	int r = event_wait(&ering, st_ptr, &cc, &sl);
	return (r < 0) ? -1 : (int)cc;
}

int xhci_probe(struct pci_device *dev)
{
	/* BAR0 = 64-bit MMIO (regs 0x10/0x14); also set Memory Space
	 * Enable + Bus Master in the command register (0x04). */
	uint64_t bar = (uint64_t)(cfg32(dev, 0x10) & ~0xfu) |
	    ((uint64_t)cfg32(dev, 0x14) << 32);
	uint32_t cmd = cfg32(dev, 0x04);
	{
		uint32_t a = 0x80000000u | ((uint32_t)dev->addr.bus << 16) |
		    ((uint32_t)dev->addr.devid << 11) |
		    ((uint32_t)dev->addr.function << 8) | 0x04;
		outl(0xcf8, a);
		outl(0xcfc, (cmd | 0x6));
	}
	uint32_t cap0, hcs1, hcs2, caplen, dboff, rtsoff;

	DBG("xhci: pci %04x:%04x class %08x/%08x bar0=%lx",
	    dev->id.vendor_id, dev->id.device_id,
	    dev->id.class_id, dev->id.sub_class_id, (unsigned long)bar);

	cap0 = rd32(bar, 0);
	hcs1 = rd32(bar, 0x04);
	hcs2 = rd32(bar, 0x08);
	caplen = CAP_CAPLENGTH(cap0);
	dboff = CAP_DBOFF(rd32(bar, 0x14));
	rtsoff = CAP_RTSOFF(rd32(bar, 0x18));
	op_base = bar + caplen;
	rt_base = bar + rtsoff;
	db_base = bar + dboff;
	max_ports = CAP_HCSPARAMS1_MAXPORTS(hcs1);
	DBG("xhci: hci v%x.%02x slots=%u ints=%u ports=%u scratch=%u caplen=%u dboff=%x rtsoff=%x",
	    CAP_HCIVERSION(cap0) >> 8, CAP_HCIVERSION(cap0) & 0xff,
	    CAP_HCSPARAMS1_MAXSLOTS(hcs1), CAP_HCSPARAMS1_MAXINTS(hcs1),
	    max_ports, CAP_HCSPARAMS2_SCRATCH(hcs2), caplen, dboff, rtsoff);

	/* reset */
	wr32(op_base, OP_USBCMD, rd32(op_base, OP_USBCMD) | USBCMD_HCRST);
	for (int i = 0; i < 100000 && (rd32(op_base, OP_USBCMD) & USBCMD_HCRST); i++);
	for (int i = 0; i < 100000 && (rd32(op_base, OP_USBSTS) & USBSTS_CNR); i++);
	DBG("xhci: reset done sts=%x", rd32(op_base, OP_USBSTS));

	/* dcbaa + config */
	{
		uint64_t *d = uk_malloc(uk_alloc_get_default(), 8 * 65 + 64);
		dcbaa = (uint64_t *)(((uintptr_t)d + 63) & ~63ul);
		memset(dcbaa, 0, 8 * 65);
	}
	wr64(op_base, OP_DCBAAP, phys(dcbaa));
	wr32(op_base, OP_CONFIG, 8);

	/* command ring (64 trbs) + event ring (64 trbs) + erst */
	struct trb *ct = uk_malloc(uk_alloc_get_default(), 64 * sizeof(struct trb) + 64);
	ct = (struct trb *)(((uintptr_t)ct + 63) & ~63ul);
	struct trb *et = uk_malloc(uk_alloc_get_default(), 64 * sizeof(struct trb) + 64);
	et = (struct trb *)(((uintptr_t)et + 63) & ~63ul);
	erst = uk_malloc(uk_alloc_get_default(), 128);
	erst = (struct trb *)(((uintptr_t)erst + 63) & ~63ul);
	ring_init(&cring, ct, 64);
	ring_init(&ering, et, 64);
	/* event ring is consumed by us: first TRB must be written with C=1
	 * so controller-produced events (C=1) match pcs=1 */
	wr64(op_base, OP_CRCR, phys(cring.trbs) | 1);
	erst->p_lo = (uint32_t)phys(ering.trbs);
	erst->p_hi = (uint32_t)(phys(ering.trbs) >> 32);
	erst->st = 63;
	erst->ctl = 0;
	wr32(rt_base, RT_IR0 + IR_ERSTSZ, 1);
	wr64(rt_base, RT_IR0 + IR_ERSTBA, phys(erst));
	wr64(rt_base, RT_IR0 + IR_ERDP, phys(ering.trbs));
	/* ack stale IP, enable interrupter */
	wr32(rt_base, RT_IR0 + IR_IMAN, rd32(rt_base, RT_IR0 + IR_IMAN) | IMAN_IP | IMAN_IE);
	wr32(op_base, OP_USBCMD, rd32(op_base, OP_USBCMD) | USBCMD_RS | USBCMD_INTE);
	for (int i = 0; i < 100000 && (rd32(op_base, OP_USBSTS) & USBSTS_HCH); i++);
	DBG("xhci: running sts=%x", rd32(op_base, OP_USBSTS));

	/* port scan */
	int port = -1; uint32_t ps = 0;
	for (int i = 0; i < max_ports; i++) {
		ps = rd32(op_base, OP_PORTSC(i));
		DBG("xhci: port%d sc=%08x ccs=%d speed=%u", i, ps, !!(ps & PSC_CCS), PSC_SPEED(ps));
		if (ps & PSC_CCS) { port = i; break; }
	}
	if (port < 0) { DBG("xhci: no devices attached"); return -2; }
	/* port reset (USB2 port needs it; USB3 auto) */
	wr32(op_base, OP_PORTSC(port), (ps & ~PSC_WRC) | PSC_PR | (ps & PSC_CCS));
	for (int i = 0; i < 1000000; i++) {
		ps = rd32(op_base, OP_PORTSC(port));
		if (!(ps & PSC_PR)) break;
	}
	/* fence */
	/* drain PSC events */
	while (event_wait(&ering, 0, NULL, NULL) == TRB_TYPE_PSC);
	ps = rd32(op_base, OP_PORTSC(port));
	DBG("xhci: port%d post-reset sc=%08x ped=%d speed=%u", port, ps, !!(ps & PSC_PED), PSC_SPEED(ps));

	/* enable slot */
	slot_id = xcmd(0, 0, TRB_TYPE(TRB_TYPE_ENSLOT));
	DBG("xhci: enable slot -> id=%d", slot_id);
	if (slot_id <= 0) return -3;

	/* input context: ctrl add slot|ep0, slot ctx, ep0 ctrl ctx */
	ic = uk_malloc(uk_alloc_get_default(), sizeof(struct input_ctx) + 64);
	ic = (struct input_ctx *)(((uintptr_t)ic + 63) & ~63ul);
	memset(ic, 0, sizeof(*ic));
	dctx = uk_malloc(uk_alloc_get_default(), sizeof(struct dev_ctx) + 64);
	dctx = (struct dev_ctx *)(((uintptr_t)dctx + 63) & ~63ul);
	memset(dctx, 0, sizeof(*dctx));
	ic->ctrl.d[0] = 0;          /* drop flags */
	ic->ctrl.d[1] = 0x3;        /* add flags: slot ctx + ep0 */
	uint32_t speed = PSC_SPEED(ps);
	ic->slot.d[0] = SLOT_CTX_ENTRIES(1) | SLOT_CTX_SPEED(speed);
	ic->slot.d[1] = SLOT_CTX_RHPORT(port + 1);
	/* ep0: control type, cerr=3, mps by speed (USB2 64, USB3 512) */
	struct trb *ept = uk_malloc(uk_alloc_get_default(), 32 * sizeof(struct trb) + 64);
	ept = (struct trb *)(((uintptr_t)ept + 63) & ~63ul);
	ring_init(&ep0, ept, 32);
	uint32_t mps = (speed == 4) ? 512 : 64;
	ic->ep[0].d[0] = 0;
	ic->ep[0].d[1] = EP_CTX_TYPE_CTRL | EP_CTX_CERR(3) | EP_CTX_MPS(mps);
	ic->ep[0].d[2] = (uint32_t)(phys(ep0.trbs) | 1); /* dequeue ptr | DCS=1 */
	ic->ep[0].d[3] = (uint32_t)(phys(ep0.trbs) >> 32);
	ic->ep[0].d[4] = 8; /* avg trb length */

	dcbaa[slot_id] = phys(dctx);
	int r = xcmd(phys(ic), 0, TRB_TYPE(TRB_TYPE_ADDR) | (slot_id << 24));
	DBG("xhci: address device rc=%d", r);
	if (r < 0) return -4;

	/* device descriptor */
	uint8_t *buf = uk_malloc(uk_alloc_get_default(), 64);
	memset(buf, 0, 64);
	int cc = ep0_get_desc(buf);
	DBG("xhci: get-device-descriptor cc=%d", cc);
	if (cc != 1) return -5;
	DBG("xhci: devdesc len=%u vid=%02x%02x pid=%02x%02x",
	    buf[0], buf[9], buf[8], buf[11], buf[10]);
	DBG("xhci: enumerated vid=%02x%02x pid=%02x%02x",
	    buf[9], buf[8], buf[11], buf[10]);
	return 0;
}
