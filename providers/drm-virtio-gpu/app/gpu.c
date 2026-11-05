/* drm-virtio-gpu: virtio-gpu over the modern virtio-pci transport —
 * common-cfg queue setup, split virtqueue, controlq commands
 * (GET_DISPLAY_INFO, RESOURCE_CREATE_2D, ATTACH_BACKING, SET_SCANOUT,
 * TRANSFER_TO_HOST_2D, FLUSH), framebuffer scanout. Host-side oracle
 * is a QMP screendump pixel check. Original work against the virtio
 * 1.0 + virtio-gpu specs; transport helpers derived from same-author
 * virtio-pci-modern probe (BSD-3-Clause).
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include <uk/bus/pci.h>
#include <uk/paging.h>

#define DBG(...) printf(__VA_ARGS__)

/* ---------- PCI config space ---------- */
static inline void outl(uint16_t p, uint32_t v)
{ __asm__ __volatile__("outl %0,%1" :: "a"(v), "dN"(p)); }
static inline uint32_t inl(uint16_t p)
{ uint32_t v; __asm__ __volatile__("inl %1,%0" : "=a"(v) : "dN"(p)); return v; }
static uint32_t cfg_addr(struct pci_device *d, int reg)
{
	return 0x80000000u | ((uint32_t)d->addr.bus << 16) |
	    ((uint32_t)d->addr.devid << 11) |
	    ((uint32_t)d->addr.function << 8) | (reg & ~3u);
}
static uint32_t cfg32(struct pci_device *d, int reg)
{ outl(0xcf8, cfg_addr(d, reg)); uint32_t v = inl(0xcfc); return (reg & 2) ? (v >> 16) : v; }
static uint8_t cfg8(struct pci_device *d, int reg)
{ outl(0xcf8, cfg_addr(d, reg & ~3u)); uint32_t v = inl(0xcfc); return (v >> ((reg & 3) * 8)) & 0xff; }
static uint64_t bar_base(struct pci_device *dev, int barno)
{
	uint32_t lo = cfg32(dev, 0x10 + barno * 4);
	uint64_t b = lo & ~0xfu;
	if ((lo & 6) == 4) b |= (uint64_t)cfg32(dev, 0x10 + barno * 4 + 4) << 32;
	return b;
}

/* ---------- mmio ---------- */
static void mw8(uint64_t a, uint8_t v) { *(volatile uint8_t *)(uintptr_t)a = v; }
static void mw16(uint64_t a, uint16_t v) { *(volatile uint16_t *)(uintptr_t)a = v; }
static void mw32(uint64_t a, uint32_t v) { *(volatile uint32_t *)(uintptr_t)a = v; }
static void mw64(uint64_t a, uint64_t v) { *(volatile uint64_t *)(uintptr_t)a = v; }
static uint8_t mr8(uint64_t a) { return *(volatile uint8_t *)(uintptr_t)a; }
static uint16_t mr16(uint64_t a) { return *(volatile uint16_t *)(uintptr_t)a; }
static uint32_t mr32(uint64_t a) { return *(volatile uint32_t *)(uintptr_t)a; }
static uint64_t phys(void *p) { return (uint64_t)uk_paging_virt_to_phys((__vaddr_t)p); }

/* ---------- virtio caps ---------- */
struct vcap { uint8_t type, bar; uint32_t off, len, mul; };
static struct vcap caps[8]; static int ncaps;
static uint64_t common, notify, isr;

static int find_caps(struct pci_device *dev)
{
	uint8_t cp = cfg8(dev, 0x34); int hops = 0;
	while (cp && cp >= 0x40 && hops++ < 32 && ncaps < 8) {
		uint8_t id = cfg8(dev, cp), nxt = cfg8(dev, cp + 1);
		if (id == 0x09 && cfg8(dev, cp + 2) >= 16) {
			struct vcap *c = &caps[ncaps];
			c->type = cfg8(dev, cp + 3);
			c->bar  = cfg8(dev, cp + 4);
			c->off  = cfg32(dev, cp + 8);
			c->len  = cfg32(dev, cp + 12);
			c->mul  = cfg8(dev, cp + 2) >= 20 ? cfg32(dev, cp + 16) : 0;
			ncaps++;
		}
		cp = nxt;
	}
	for (int i = 0; i < ncaps; i++) {
		if (caps[i].type == 1) common = bar_base(dev, caps[i].bar) + caps[i].off;
		if (caps[i].type == 2) notify = bar_base(dev, caps[i].bar) + caps[i].off;
		if (caps[i].type == 3) isr    = bar_base(dev, caps[i].bar) + caps[i].off;
	}
	return ncaps;
}

/* ---------- split virtqueue ---------- */
#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2
struct vdesc { uint64_t addr; uint32_t len; uint16_t flags, next; } __attribute__((packed));
struct vavail { uint16_t flags, idx, ring[0]; };
struct vused_elem { uint32_t id, len; };
struct vused { uint16_t flags, idx; struct vused_elem ring[0]; };

struct vq {
	int idx, n;
	uint16_t last_used;
	struct vdesc *desc;
	struct vavail *avail;
	struct vused *used;
	uint64_t notify_addr;
};

static int vq_setup(int idx)
{
	mw16(common + 0x16, idx);                 /* queue_select */
	uint16_t sz = mr16(common + 0x18);
	if (!sz) return -1;
	struct vq *q = uk_malloc(uk_alloc_get_default(), sizeof(*q));
	memset(q, 0, sizeof(*q));
	q->idx = idx; q->n = sz;
	void *m = uk_malloc(uk_alloc_get_default(),
	    sz * 16 + 16 + sz * 2 + 16 + sz * 8 + 64);
	uintptr_t p = (uintptr_t)m;
	q->desc  = (struct vdesc *)p;  p += sz * 16;
	p = (p + 15) & ~15ul;
	q->avail = (struct vavail *)p; p += 4 + sz * 2 + 2;
	p = (p + 15) & ~15ul;
	q->used  = (struct vused *)p;
	memset(q->desc, 0, sz * 16);
	memset(q->avail, 0, 4 + sz * 2 + 2);
	memset(q->used, 0, 4 + sz * 8);
	q->notify_addr = notify + (uint64_t)mr16(common + 0x1e) * 4;
	mw64(common + 0x20, phys(q->desc));
	mw64(common + 0x28, phys(q->avail));
	mw64(common + 0x30, phys(q->used));
	mw16(common + 0x1c, 1);                   /* queue_enable */
	/* stash */
	extern struct vq *vqs[4];
	vqs[idx] = q;
	DBG("vgpu: queue%d size=%u notify_off=%u\n", idx, sz, mr16(common + 0x1e));
	return 0;
}
struct vq *vqs[4];

/* submit a 2-desc out+in command, poll used ring for completion */
static int vq_xfer(struct vq *q, void *out, int outlen, void *in, int inlen)
{
	uint64_t op = phys(out), ip = phys(in);
	q->desc[0].addr = op; q->desc[0].len = outlen;
	q->desc[0].flags = VRING_DESC_F_NEXT; q->desc[0].next = 1;
	q->desc[1].addr = ip; q->desc[1].len = inlen;
	q->desc[1].flags = VRING_DESC_F_WRITE; q->desc[1].next = 0;
	uint16_t aidx = q->avail->idx;
	q->avail->ring[aidx % q->n] = 0;
	__sync_synchronize();
	q->avail->idx = aidx + 1;
	mw16(q->notify_addr, q->idx);
	for (volatile long i = 0; i < 50000000; i++)
		if (((volatile struct vused *)q->used)->idx != q->last_used) {
			uint16_t ui = q->last_used % q->n;
			q->last_used++;
			return q->used->ring[ui].len;
		}
	mw16(common + 0x16, q->idx);
	uint32_t u_lo = mr32(common + 0x30), u_hi = mr32(common + 0x34);
	uint32_t a_lo = mr32(common + 0x28), d_lo = mr32(common + 0x20);
	DBG("vgpu: vq dbg used_reg=%x:%x avail_reg=%x desc_reg=%x myused=%lx idx=%x fl=%x aidx=%x\n",
	    u_hi, u_lo, a_lo, d_lo, (unsigned long)phys(q->used),
	    q->used->idx, q->used->flags, q->avail->idx);
	return -1;
}

/* ---------- virtio-gpu protocol ---------- */
struct gpu_hdr { uint32_t type, flags; uint64_t fence_id; uint32_t ctx_id, pad; };
struct gpu_rect { uint32_t x, y, w, h; };
struct gpu_display_one { uint32_t enabled; struct gpu_rect r; uint32_t flags; };
struct gpu_display_info {
	uint32_t type, flags;
	struct gpu_display_one pmodes[16];
};
struct gpu_res_create_2d {
	struct gpu_hdr hdr; uint32_t resource_id, format, w, h;
};
struct gpu_mem_entry { uint64_t addr; uint32_t len, pad; };
struct gpu_attach_backing {
	struct gpu_hdr hdr; uint32_t resource_id, nr_entries;
	struct gpu_mem_entry e[1];
};
struct gpu_set_scanout {
	struct gpu_hdr hdr; struct gpu_rect r; uint32_t scanout_id, resource_id;
};
struct gpu_transfer_2d {
	struct gpu_hdr hdr; struct gpu_rect r; uint64_t offset; uint32_t resource_id, pad;
};
struct gpu_flush {
	struct gpu_hdr hdr; struct gpu_rect r; uint32_t resource_id, pad;
};

#define CMD_GET_DISPLAY_INFO	0x0100
#define CMD_RESOURCE_CREATE_2D	0x0101
#define CMD_SET_SCANOUT		0x0103
#define CMD_RESOURCE_FLUSH	0x0104
#define CMD_TRANSFER_2D		0x0105
#define CMD_ATTACH_BACKING	0x0106
#define RESP_OK_NODATA		0x1100
#define RESP_OK_DISPLAY_INFO	0x1200
#define FMT_B8G8R8X8		1

static uint8_t cmdbuf[128], respbuf[512];
static struct vq *ctrlq;

static int gpu_cmd(void *cmd, int len, void *resp, int rlen)
{
	memcpy(cmdbuf, cmd, len);
	memset(respbuf, 0, rlen);
	int r = vq_xfer(ctrlq, cmdbuf, len, respbuf, rlen);
	if (r < 0) return -1;
	memcpy(resp, respbuf, rlen);
	struct gpu_hdr *h = (struct gpu_hdr *)respbuf;
	return (int)h->type;
}

int gpu_probe(struct pci_device *dev)
{
	if (dev->id.vendor_id != 0x1af4 ||
	    (dev->id.device_id != 0x1050 && dev->id.device_id != 0x1010))
		return -1;
	DBG("vgpu: virtio-gpu %04x:%04x\n", dev->id.vendor_id, dev->id.device_id);
	if (!find_caps(dev)) { DBG("vgpu: REFUSE no caps\n"); return -2; }
	DBG("vgpu: caps=%d common=%lx notify=%lx isr=%lx\n", ncaps,
	    (unsigned long)common, (unsigned long)notify, (unsigned long)isr);

	/* status protocol + VERSION_1 */
	mw8(common + 0x14, 0);
	mw8(common + 0x14, 1);
	uint8_t st = mr8(common + 0x14);
	mw8(common + 0x14, st | 2);
	mw32(common + 0x08, 1);
	mw32(common + 0x0c, 1);                     /* VERSION_1 */
	st = mr8(common + 0x14);
	mw8(common + 0x14, st | 8);
	st = mr8(common + 0x14);
	if (!(st & 8)) { DBG("vgpu: REFUSE features st=%x\n", st); return -3; }
	DBG("vgpu: VERSION_1 negotiated\n");
	mw8(common + 0x14, st | 4);                 /* DRIVER_OK */

	if (vq_setup(0)) { DBG("vgpu: REFUSE no ctrlq\n"); return -4; }
	ctrlq = vqs[0];

	/* GET_DISPLAY_INFO */
	struct gpu_hdr gc = { .type = CMD_GET_DISPLAY_INFO };
	struct gpu_display_info di;
	int t = gpu_cmd(&gc, sizeof(gc), &di, sizeof(di));
	int w = 0, h = 0;
	if (t == RESP_OK_DISPLAY_INFO) {
		for (int i = 0; i < 16; i++)
			if (di.pmodes[i].enabled && !w) {
				w = di.pmodes[i].r.w; h = di.pmodes[i].r.h;
				DBG("vgpu: mode%d %ux%u\n", i, w, h);
			}
	} else {
		DBG("vgpu: disp info resp=%x — no dpy backend; programming 640x480\n", t);
		w = 640; h = 480;
	}
	if (!w) { DBG("vgpu: no enabled scanout\n"); return -6; }

	/* RESOURCE_CREATE_2D — dumb framebuffer */
	uint32_t rid = 1;
	struct gpu_res_create_2d rc = { .hdr.type = CMD_RESOURCE_CREATE_2D,
	    .resource_id = rid, .format = FMT_B8G8R8X8, .w = w, .h = h };
	t = gpu_cmd(&rc, sizeof(rc), respbuf, 8);
	DBG("vgpu: create2d %ux%u resp=%x\n", w, h, t);
	if (t != RESP_OK_NODATA) return -7;

	/* ATTACH_BACKING — the framebuffer pages */
	uint32_t fbsz = w * h * 4;
	uint8_t *fb = uk_malloc(uk_alloc_get_default(), fbsz + 4096);
	fb = (uint8_t *)(((uintptr_t)fb + 4095) & ~4095ul);
	memset(fb, 0, fbsz);
	struct gpu_attach_backing ab = { .hdr.type = CMD_ATTACH_BACKING,
	    .resource_id = rid, .nr_entries = 1 };
	ab.e[0].addr = phys(fb); ab.e[0].len = fbsz;
	t = gpu_cmd(&ab, sizeof(ab), respbuf, 8);
	DBG("vgpu: attach resp=%x\n", t);
	if (t != RESP_OK_NODATA) return -8;

	/* SET_SCANOUT */
	struct gpu_set_scanout so = { .hdr.type = CMD_SET_SCANOUT,
	    .r = { 0, 0, w, h }, .scanout_id = 0, .resource_id = rid };
	t = gpu_cmd(&so, sizeof(so), respbuf, 8);
	DBG("vgpu: scanout resp=%x\n", t);
	if (t != RESP_OK_NODATA) return -9;

	/* paint: left half red, right half blue, center yellow band —
	 * screendump-verifiable at fixed coords */
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uint32_t *px = (uint32_t *)(fb + (y * w + x) * 4);
			if (x < w / 4) *px = 0x00ff0000;        /* B? — B8G8R8: b in byte0 */
			else if (x < w / 2) *px = 0x000000ff;   /* R */
			else if (y < h / 4) *px = 0x0000ffff;   /* R+G = yellow */
			else *px = 0x0000ff00;                /* G */
		}
	/* B8G8R8X8: byte0=B byte1=G byte2=R — write explicit bytes */
	for (int i = 0; i < w * h; i++) {
		int x = i % w, y = i / w;
		uint8_t *px = fb + i * 4;
		px[0] = (x < w / 4) ? 255 : 0;                     /* B */
		px[1] = (y < h / 4) ? 255 : ((x < w / 2) ? 0 : 255); /* G */
		px[2] = (x < w / 4) ? 0 : ((x < w / 2) ? 255 : 0);   /* R */
		px[3] = 0;
	}

	/* TRANSFER_TO_HOST_2D + FLUSH */
	struct gpu_transfer_2d tr = { .hdr.type = CMD_TRANSFER_2D,
	    .r = { 0, 0, w, h }, .offset = 0, .resource_id = rid };
	t = gpu_cmd(&tr, sizeof(tr), respbuf, 8);
	DBG("vgpu: transfer resp=%x\n", t);
	if (t != RESP_OK_NODATA) return -10;
	struct gpu_flush fl = { .hdr.type = CMD_RESOURCE_FLUSH,
	    .r = { 0, 0, w, h }, .resource_id = rid };
	t = gpu_cmd(&fl, sizeof(fl), respbuf, 8);
	DBG("vgpu: flush resp=%x\n", t);
	if (t != RESP_OK_NODATA) return -11;
	DBG("vgpu: scanout live %ux%u — serial done\n", w, h);
	return 0;
}
