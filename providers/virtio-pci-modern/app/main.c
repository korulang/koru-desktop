/* virtio-pci-modern probe — walks the virtio 1.0 PCI capability list
 * on a legacy-disabled device, negotiates VERSION_1 through the common
 * cfg region, prints the negotiated fields to serial.
 * Original work against the virtio 1.0 spec §4.1.4 (PCI transport)
 * + virtio pci driver spec §4.1.5.  No lifted code.
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
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
	uint32_t v = inl(0xcfc);
	return (reg & 2) ? (v >> 16) : v;
}
static uint8_t cfg8(struct pci_device *d, int reg)
{
	outl(0xcf8, cfg_addr(d, reg & ~3u));
	uint32_t v = inl(0xcfc);
	return (v >> ((reg & 3) * 8)) & 0xff;
}

/* virtio cap structure (virtio 1.0 spec §4.1.4.1) */
struct virtio_cap {
	uint8_t cfg_type, bar;
	uint8_t id;
	uint32_t offset, length;
	uint32_t notify_mul;   /* notify cap only */
};

static const char *cap_name(int t)
{
	switch (t) {
	case 1: return "common";
	case 2: return "notify";
	case 3: return "isr";
	case 4: return "device";
	case 5: return "pci-cfg";
	default: return "?";
	}
}

static uint64_t bar_base(struct pci_device *dev, int barno)
{
	uint32_t lo = cfg32(dev, 0x10 + barno * 4);
	uint64_t b = lo & ~0xfu;
	if ((lo & 6) == 4)
		b |= (uint64_t)cfg32(dev, 0x10 + barno * 4 + 4) << 32;
	return b;
}

static int found;
static struct virtio_cap caps[8];
static int ncaps;

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

static int add(struct pci_device *dev)
{
	if (dev->id.vendor_id != 0x1af4)
		return -1;
	found = 1;
	printf("vpci-modern: virtio dev %04x:%04x\n",
	       dev->id.vendor_id, dev->id.device_id);

	/* walk the PCI capability list — pointer at config 0x34 */
	uint8_t cp = cfg8(dev, 0x34);
	ncaps = 0;
	int hops = 0;
	while (cp && cp >= 0x40 && hops++ < 32 && ncaps < 8) {
		uint8_t id = cfg8(dev, cp);
		uint8_t nxt = cfg8(dev, cp + 1);
		if (id == 0x09) {           /* vendor-specific */
			uint8_t len = cfg8(dev, cp + 2);
			if (len >= 16) {
				struct virtio_cap *c = &caps[ncaps];
				c->cfg_type = cfg8(dev, cp + 3);
				c->bar      = cfg8(dev, cp + 4);
				c->id       = cfg8(dev, cp + 5);
				uint32_t lo = cfg32(dev, cp + 8);
				uint32_t hi = cfg32(dev, cp + 12);
				c->offset = lo;
				c->length = hi;
				c->notify_mul = (len >= 20) ? cfg32(dev, cp + 16) : 0;
				printf("vpci-modern: cap[%d] type=%s(%d) bar=%d off=%x len=%x\n",
				    ncaps, cap_name(c->cfg_type), c->cfg_type,
				    c->bar, c->offset, c->length);
				ncaps++;
			}
		}
		cp = nxt;
	}
	printf("vpci-modern: caps=%d\n", ncaps);
	if (!ncaps) {
		printf("vpci-modern: REFUSE — no virtio caps (legacy-only device)\n");
		return -2;
	}

	struct virtio_cap *common = NULL, *notify = NULL, *isr = NULL, *dcfg = NULL;
	for (int i = 0; i < ncaps; i++) {
		if (caps[i].cfg_type == 1) common = &caps[i];
		if (caps[i].cfg_type == 2) notify = &caps[i];
		if (caps[i].cfg_type == 3) isr = &caps[i];
		if (caps[i].cfg_type == 4) dcfg = &caps[i];
	}
	if (!common || !notify || !isr) {
		printf("vpci-modern: REFUSE — missing common/notify/isr cap\n");
		return -3;
	}
	uint64_t cbase = bar_base(dev, common->bar) + common->offset;
	uint64_t nbase = bar_base(dev, notify->bar) + notify->offset;
	uint64_t ibase = bar_base(dev, isr->bar) + isr->offset;
	printf("vpci-modern: common=%lx notify=%lx isr=%lx mul=%u\n",
	    (unsigned long)cbase, (unsigned long)nbase,
	    (unsigned long)ibase, notify->notify_mul);

	/* feature negotiation (virtio 1.0 §3.1.1): device_status walk */
	mmio_w(cbase + 0x14, 1, 0);                     /* reset */
	mmio_w(cbase + 0x14, 1, 1);                     /* ACKNOWLEDGE */
	uint8_t st = mmio_r(cbase + 0x14, 1);
	mmio_w(cbase + 0x14, 1, st | 2);                /* DRIVER */
	uint32_t nq = mmio_r(cbase + 0x12, 2);
	printf("vpci-modern: queues=%u\n", nq);

	/* device feature bits, select bank 0 + 1 */
	mmio_w(cbase + 0x00, 4, 0);
	uint32_t df0 = mmio_r(cbase + 0x04, 4);
	mmio_w(cbase + 0x00, 4, 1);
	uint32_t df1 = mmio_r(cbase + 0x04, 4);
	printf("vpci-modern: devfeat %08x:%08x\n", df1, df0);

	/* offer VERSION_1 (bit 32 → bank 1 bit 0) + generic bits */
	mmio_w(cbase + 0x08, 4, 0);
	mmio_w(cbase + 0x0c, 4, 0);
	mmio_w(cbase + 0x08, 4, 1);
	mmio_w(cbase + 0x0c, 4, 1);                     /* VERSION_1 */
	st = mmio_r(cbase + 0x14, 1);
	mmio_w(cbase + 0x14, 1, st | 8);                /* FEATURES_OK */
	st = mmio_r(cbase + 0x14, 1);
	if (!(st & 8)) {
		printf("vpci-modern: REFUSE — FEATURES_OK rejected st=%x\n", st);
		return -4;
	}
	printf("vpci-modern: VERSION_1 negotiated st=%x\n", st);

	/* isr must be readable without side effects */
	uint8_t isrv = mmio_r(ibase, 1);
	printf("vpci-modern: isr=%x\n", isrv);
	printf("vpci-modern: accept device %04x queues=%u\n",
	    dev->id.device_id, nq);
	return 0;
}

static const struct pci_device_id vpci_ids[] = {
	/* transitional ids (0x1000-0x103f) — cap list still present */
	{ PCI_DEVICE_ID(0x1af4, 0x1000) },   /* net transitional */
	{ PCI_DEVICE_ID(0x1af4, 0x1001) },   /* blk transitional */
	{ PCI_DEVICE_ID(0x1af4, 0x1010) },   /* gpu transitional */
	/* modern ids (0x1040-0x107f) */
	{ PCI_DEVICE_ID(0x1af4, 0x1041) },   /* net */
	{ PCI_DEVICE_ID(0x1af4, 0x1042) },   /* blk */
	{ PCI_DEVICE_ID(0x1af4, 0x1043) },   /* console */
	{ PCI_DEVICE_ID(0x1af4, 0x1044) },   /* rng */
	{ PCI_DEVICE_ID(0x1af4, 0x1045) },   /* balloon */
	{ PCI_DEVICE_ID(0x1af4, 0x1050) },   /* gpu */
	{ PCI_DEVICE_ID(0x1af4, 0x1052) },   /* input */
	{ PCI_ANY_DEVICE_ID },
};

static struct pci_driver vpci_drv = {
	.device_ids = vpci_ids,
	.add_dev = add,
};
PCI_REGISTER_DRIVER(&vpci_drv);

int main(void)
{
	printf("vpci-modern: guest up\n");
	printf("vpci-modern: probe found=%d\n", found);
	return 0;
}
