/* usb-storage: BOT mass-storage — CBW/data/CSW over the bulk eps
 * xhci.c configures. SCSI: TEST_UNIT_READY, INQUIRY, READ_CAPACITY_10,
 * READ_10, WRITE_10. Written from USB MSC BOT 1.0 + SBC; no code
 * lifted. SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include "msc.h"

#define DBG(...) printf(__VA_ARGS__)

#define CBW_SIG 0x43425355u /* 'USBC' */
#define CSW_SIG 0x53425355u /* 'USBS' */

struct cbw {
	uint32_t sig, tag, dlen;
	uint8_t flags, lun, cbwcb_len, cbwcb[16];
} __attribute__((packed));
struct csw { uint32_t sig, tag, res; uint8_t status; } __attribute__((packed));

static uint32_t tag = 1;

/* one BOT transaction: CBW out -> optional data -> CSW in.
 * Returns CSW status byte (0=good) or -1 on transport error. */
static int bot(void *scb, int scb_len, void *data, uint32_t dlen, int in)
{
	struct cbw *c = uk_malloc(uk_alloc_get_default(), sizeof(*c) + 16);
	c = (struct cbw *)(((uintptr_t)c + 15) & ~15ul);
	struct csw *w = uk_malloc(uk_alloc_get_default(), sizeof(*w) + 16);
	w = (struct csw *)(((uintptr_t)w + 15) & ~15ul);
	memset(c, 0, sizeof(*c)); memset(w, 0, sizeof(*w));
	c->sig = CBW_SIG; c->tag = tag++; c->dlen = dlen;
	c->flags = in ? 0x80 : 0;
	c->cbwcb_len = scb_len;
	memcpy(c->cbwcb, scb, scb_len);
	if (usb_bulk_out(c, 31) != 1) { DBG("msc: cbw fail"); return -1; }
	if (dlen) {
		int cc = in ? usb_bulk_in(data, dlen) : usb_bulk_out(data, dlen);
		if (cc != 1 && cc != 0) { DBG("msc: data cc=%d", cc); }
	}
	if (usb_bulk_in(w, 13) != 1) { DBG("msc: csw fail"); return -1; }
	if (w->sig != CSW_SIG || w->tag != c->tag) { DBG("msc: bad csw sig=%08x tag=%x", w->sig, w->tag); return -1; }
	return w->status;
}

int msc_inquiry(void)
{
	uint8_t scb[6] = { 0x12, 0, 0, 0, 36, 0 };
	uint8_t *b = uk_malloc(uk_alloc_get_default(), 36);
	memset(b, 0, 36);
	int st = bot(scb, 6, b, 36, 1);
	if (st) { DBG("msc: inquiry st=%d", st); return -1; }
	printf("msc: INQUIRY vendor='%.8s' product='%.16s' rev='%.4s'\n",
	       b + 8, b + 16, b + 32);
	return 0;
}

int msc_read(uint32_t lba, void *buf)
{
	uint8_t scb[10] = { 0x28, 0,
		(uint8_t)(lba >> 24), (uint8_t)(lba >> 16),
		(uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0 };
	return bot(scb, 10, buf, 512, 1) ? -1 : 0;
}

int msc_write(uint32_t lba, const void *buf)
{
	uint8_t scb[10] = { 0x2a, 0,
		(uint8_t)(lba >> 24), (uint8_t)(lba >> 16),
		(uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, 1, 0 };
	return bot(scb, 10, (void *)buf, 512, 0) ? -1 : 0;
}
