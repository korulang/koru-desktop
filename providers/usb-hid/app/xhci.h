/* xhci.h — shared types for the xhci + hid layers.
 * Derived from providers/usb-xhci/app (same author). BSD-3-Clause. */
#ifndef USB_XHCI_H
#define USB_XHCI_H

#include <stdint.h>
#include <uk/essentials.h>
#include <uk/bus/pci.h>

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
#define TRB_TYPE_NORMAL	1
#define TRB_TYPE_LINK	6
#define TRB_TYPE_ENSLOT	9
#define TRB_TYPE_ADDR	11
#define TRB_TYPE_CFGEP	12
#define TRB_TYPE_EVAL	13
#define TRB_IDT		(1u << 6)
#define TRB_IOC		(1u << 5)
#define TRB_ISP		(1u << 2)
#define TRB_DIR_IN	(1u << 16)

struct ctx32 { uint32_t d[8]; };
struct input_ctx { struct ctx32 ctrl; struct ctx32 slot; struct ctx32 ep[31]; };
struct dev_ctx  { struct ctx32 slot; struct ctx32 ep[31]; };

struct ring { struct trb *trbs; int n, enq, deq; int pcs; };

#define SLOT_CTX_ENTRIES(v) ((v) << 27)
#define SLOT_CTX_RHPORT(v)  ((v) << 16)
#define SLOT_CTX_SPEED(v)   ((v) << 20)
#define EP_CTX_TYPE_CTRL	(4u << 3)
#define EP_CTX_MPS(v)		(((v) & 0xffff) << 16)
#define EP_CTX_IVAL(v)		((v) << 16)
#define EP_CTX_CERR(v)		(((v) & 3) << 1)

struct xhci_ops {
	uint64_t (*phys)(void *);
	void (*ring_init)(struct ring *, struct trb *, int);
	void (*ring_push)(struct ring *, uint64_t, uint32_t, uint32_t);
	void (*doorbell)(int, int);
	int (*event_wait)(struct ring *, uint64_t, uint32_t *, uint32_t *);
	int (*xcmd)(uint64_t, uint32_t, uint32_t);
};

int xhci_probe(struct pci_device *dev);

#endif
