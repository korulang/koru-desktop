/* usb-hid: continue past xhci enumeration — read the configuration
 * descriptor, ConfigureEP for the interrupt-IN endpoint, SET_CONFIGURATION,
 * then run the interrupt transfer ring and decode boot-protocol
 * keyboard reports. Original work (BSD-3-Clause). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include <uk/paging.h>
#include <uk/bus/pci.h>
#include "xhci.h"



#define DBG(...) printf(__VA_ARGS__)

/* ---- reused xhci plumbing (shims exported from xhci.c) ---- */
int xhci_probe(struct pci_device *dev);


extern int xhci_slot_id(void);
extern struct ring *xhci_ep0(void);
extern struct ring *xhci_ep0_event_ring(void);
extern uint64_t *xhci_dcbaa(void);
extern struct input_ctx *xhci_ic(void);
extern struct dev_ctx *xhci_dctx(void);
extern struct xhci_ops xhci;

struct hid_endp {
	uint8_t addr;   /* bEndpointAddress */
	uint8_t attr;   /* bmAttributes */
	uint16_t mps;
	uint8_t ival;
	uint8_t cfg_val;
	uint8_t iface;
	uint8_t proto;
};

/* USB descriptors */
#define DESC_CONFIG	2
#define DESC_INTERFACE	4
#define DESC_ENDPOINT	5
#define DESC_HID	0x21

/* boot-protocol scancode table (usage ID -> char) for the serial oracle */
static const char hid_us[] = {
	[4]='a',[5]='b',[6]='c',[7]='d',[8]='e',[9]='f',[10]='g',
	[11]='h',[12]='i',[13]='j',[14]='k',[15]='l',[16]='m',
	[17]='n',[18]='o',[19]='p',[20]='q',[21]='r',[22]='s',
	[23]='t',[24]='u',[25]='v',[26]='w',[27]='x',[28]='y',
	[29]='z',[30]='1',[31]='2',[32]='3',[33]='4',[34]='5',
	[35]='6',[36]='7',[37]='8',[38]='9',[39]='0',[40]='\n',
	[42]='\b',[43]='\t',[44]=' '
};

/* generic control transfer on ep0: bmRT/bReq/wVal/wIdx/wLen + optional data */
static int ctrl_xfer(struct ring *ep0, int slot, uint8_t bmrt, uint8_t breq,
    uint16_t wval, uint16_t widx, uint16_t wlen, uint8_t *buf, int dir_in)
{
	struct trb *t = &ep0->trbs[ep0->enq];
	uint64_t setup = (uint64_t)bmrt | ((uint64_t)breq << 8) |
	    ((uint64_t)wval << 16) | ((uint64_t)widx << 32) |
	    ((uint64_t)wlen << 48);
	uint32_t trt = wlen ? (dir_in ? 3u : 2u) : 0;
	t->p_lo = (uint32_t)setup; t->p_hi = (uint32_t)(setup >> 32);
	t->st = 8;
	t->ctl = TRB_TYPE(TRB_TYPE_SETUP) | TRB_IDT | (trt << 16) | ep0->pcs;
	ep0->enq++;
	if (wlen) {
		t = &ep0->trbs[ep0->enq];
		uint64_t bp = xhci.phys(buf);
		t->p_lo = (uint32_t)bp; t->p_hi = (uint32_t)(bp >> 32);
		t->st = wlen;
		t->ctl = TRB_TYPE(TRB_TYPE_DATA) | (dir_in ? TRB_DIR_IN : 0) |
		    TRB_IOC | ep0->pcs;
		ep0->enq++;
	}
	t = &ep0->trbs[ep0->enq];
	t->p_lo = 0; t->p_hi = 0; t->st = 0;
	/* status stage: for IN-data the status is OUT (dir 0); for
	 * OUT/no-data the status is IN (dir 1) */
	uint32_t sdir = (!wlen || !dir_in) ? TRB_DIR_IN : 0;
	t->ctl = TRB_TYPE(TRB_TYPE_STATUS) | TRB_IOC | sdir | ep0->pcs;
	uint64_t st_ptr = xhci.phys(t);
	ep0->enq++;
	xhci.doorbell(slot, 1);
	uint32_t cc = 0, sl = 0;
	int r = xhci.event_wait(xhci_ep0_event_ring(), st_ptr, &cc, &sl);
	(void)sl;
	return (r < 0) ? -1 : (int)cc;
}

/* scan config descriptor for the first HID interface + int-IN endpoint */
static int parse_config(uint8_t *d, int len, struct hid_endp *e)
{
	int i = 0, found_if = 0;
	while (i + 2 <= len) {
		int blen = d[i], btype = d[i + 1];
		if (blen < 2) break;
		if (btype == DESC_INTERFACE && blen >= 9 && d[i + 5] == 3) {
			e->iface = d[i + 2];
			e->proto = d[i + 7];
			found_if = 1;
		} else if (btype == DESC_ENDPOINT && found_if && blen >= 7) {
			uint8_t attr = d[i + 3] & 3;
			if ((d[i + 2] & 0x80) && attr == 3) {
				e->addr = d[i + 2];
				e->attr = attr;
				e->mps = d[i + 4] | (d[i + 5] << 8);
				e->ival = d[i + 6];
				return 0;
			}
		} else if (btype == DESC_CONFIG && blen >= 9) {
			e->cfg_val = d[i + 5];
		}
		i += blen;
	}
	return -1;
}

/* interrupt transfer ring + report loop */
static struct ring int_ring;
static uint8_t *repbuf;

static void post_report_trb(void)
{
	struct trb *t = &int_ring.trbs[int_ring.enq];
	uint64_t bp = xhci.phys(repbuf);
	t->p_lo = (uint32_t)bp; t->p_hi = (uint32_t)(bp >> 32);
	t->st = 8;                     /* TRB len = boot report size */
	t->ctl = TRB_TYPE(TRB_TYPE_NORMAL) | TRB_IOC | TRB_ISP |
	    int_ring.pcs;
	uint64_t tp = xhci.phys(t);
	int_ring.enq++;
	(void)tp;
}

int hid_probe(struct pci_device *dev)
{
	int r = xhci_probe(dev);   /* enumeration through device descriptor */
	if (r) return r;
	int slot = xhci_slot_id();
	DBG("usb-hid: slot=%d enumerated — continuing to HID\n", slot);

	/* configuration descriptor — header then full */
	uint8_t *cfg = uk_malloc(uk_alloc_get_default(), 256);
	struct ring *ep0 = xhci_ep0();
	int cc = ctrl_xfer(ep0, slot, 0x80, 6, 0x0200, 0, 9, cfg, 1);
	if (cc != 1) { DBG("usb-hid: cfg-hdr cc=%d\n", cc); return -10; }
	int total = cfg[2] | (cfg[3] << 8);
	cc = ctrl_xfer(ep0, slot, 0x80, 6, 0x0200, 0, total, cfg, 1);
	if (cc != 1) { DBG("usb-hid: cfg cc=%d\n", cc); return -11; }
	DBG("usb-hid: config total=%d\n", total);

	struct hid_endp e = {0};
	if (parse_config(cfg, total, &e)) { DBG("usb-hid: no HID int-in ep\n"); return -12; }
	DBG("usb-hid: iface=%u proto=%u ep=0x%02x mps=%u ival=%u cfg=%u\n",
	    e.iface, e.proto, e.addr, e.mps, e.ival, e.cfg_val);

	/* DCI for interrupt-IN endpoint: epn*2 + 1 */
	int epn = e.addr & 0xf;
	int dci = epn * 2 + 1;

	/* ConfigureEP input context: add slot ctx + endpoint ctx dci */
	struct input_ctx *ic = xhci_ic();
	memset(ic, 0, sizeof(*ic));
	ic->ctrl.d[1] = 1 | (1u << dci);          /* add slot + ep dci */
	ic->slot.d[0] = (dci << 27);              /* context entries = dci */
	struct trb *it = uk_malloc(uk_alloc_get_default(), 32 * sizeof(struct trb) + 64);
	it = (struct trb *)(((uintptr_t)it + 63) & ~63ul);
	xhci.ring_init(&int_ring, it, 32);
	/* interrupt-IN ep ctx: type 7 (int in), cerr 3, mps, ival */
	int etype = 7;
	int cival = e.ival;
	if (cival < 1) cival = 1;
	if (cival > 16) cival = 16;
	ic->ep[dci - 1].d[0] = cival << 16;
	ic->ep[dci - 1].d[1] = (etype << 3) | (3 << 1) | ((e.mps & 0x7ff) << 16);
	uint64_t dq = xhci.phys(int_ring.trbs);
	ic->ep[dci - 1].d[2] = (uint32_t)(dq | 1);
	ic->ep[dci - 1].d[3] = (uint32_t)(dq >> 32);
	ic->ep[dci - 1].d[4] = 8;
	r = xhci.xcmd(xhci.phys(ic), 0, TRB_TYPE(TRB_TYPE_CFGEP) | (slot << 24));
	DBG("usb-hid: configure ep dci=%d rc=%d\n", dci, r);
	if (r < 0) return -13;

	/* SET_CONFIGURATION */
	cc = ctrl_xfer(ep0, slot, 0x00, 9, e.cfg_val, 0, 0, NULL, 0);
	DBG("usb-hid: set-config %u cc=%d\n", e.cfg_val, cc);
	if (cc != 1) return -14;
	DBG("usb-hid: configured — hid polling ep=0x%02x\n", e.addr);

	/* interrupt report loop */
	repbuf = uk_malloc(uk_alloc_get_default(), 64);
	memset(repbuf, 0, 64);
	int reports = 0;
	for (int polls = 0; polls < 2000 && reports < 40; polls++) {
		post_report_trb();
		xhci.doorbell(slot, dci);
		uint32_t cc2 = 0, sl = 0;
		/* wait for a transfer event on this ep's ring */
		int er = xhci.event_wait(xhci_ep0_event_ring(), 0, &cc2, &sl);
		if (er < 0) continue;
		if (repbuf[2] || repbuf[3] || repbuf[4] || repbuf[5] ||
		    repbuf[6] || repbuf[7]) {
			uint8_t k = repbuf[2];
			char c = (k < sizeof(hid_us)) ? hid_us[k] : '?';
			DBG("usb-hid: report#%d mod=%02x keys=%02x %02x %02x %02x %02x %02x char=%c\n",
			    reports, repbuf[0], repbuf[2], repbuf[3], repbuf[4],
			    repbuf[5], repbuf[6], repbuf[7], c ? c : '?');
			reports++;
			memset(repbuf, 0, 8);
		}
	}
	DBG("usb-hid: done reports=%d\n", reports);
	return reports > 0 ? 0 : -20;
}
