/* audio-hda: minimal Intel HDA driver — GCTL reset, CORB/RIRB verb
 * channel, codec enumeration, output-stream DMA playback. Written
 * from the Intel HDA spec (097a0384) — no code lifted.
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/alloc.h>
#include <uk/bus/pci.h>
#include "hda.h"

#define DBG(...) printf(__VA_ARGS__)

static inline void outl(uint16_t p, uint32_t v)
{ __asm__ __volatile__("outl %0,%1" :: "a"(v), "dN"(p)); }
static inline uint32_t inl(uint16_t p)
{ uint32_t v; __asm__ __volatile__("inl %1,%0" : "=a"(v) : "dN"(p)); return v; }
static uint32_t cfg32(struct pci_device *d, int reg)
{
	uint32_t a = 0x80000000u | ((uint32_t)d->addr.bus << 16) |
	    ((uint32_t)d->addr.devid << 11) |
	    ((uint32_t)d->addr.function << 8) | (reg & ~3u);
	outl(0xcf8, a);
	uint32_t v = inl(0xcfc);
	return (reg & 2) ? (v >> 16) : v;
}

/* HDA register file (BAR0) */
#define R_GCAP		0x00	/* 16: gcap */
#define R_GCTL		0x08	/* 32: crst bit0, unsol bit8, flush bit9 */
#define R_WAKEEN	0x0C	/* 16 */
#define R_STATESTS	0x0E	/* 16: codec present bits */
#define R_INTCTL	0x20	/* 32 */
#define R_INTSTS	0x24	/* 32 */
#define R_CORBLBASE	0x40	/* 32 */
#define R_CORBUBASE	0x44	/* 32 */
#define R_CORBWP	0x48	/* 16 */
#define R_CORBRP	0x4A	/* 16: set RST bit then clear */
#define R_CORBCTL	0x4C	/* 8: run bit1 */
#define R_CORBSTS	0x4D	/* 8 */
#define R_CORBSIZE	0x4E	/* 8: 0x02=256 entries */
#define R_RIRBLBASE	0x50
#define R_RIRBUBASE	0x54
#define R_RIRBWP	0x58	/* 16 */
#define R_RINTCNT	0x5A	/* 16 */
#define R_RIRBCTL	0x5C	/* 8: run bit1, wake bit0 */
#define R_RIRBSTS	0x5D	/* 8 */
#define R_RIRBSIZE	0x5E	/* 8 */
#define R_SD0		0x80	/* stream desc base, 0x20 each */
#define SD_CTL		0x00	/* 24: run bit1, irq bit0, stream num 20-23 */
#define SD_STS		0x03	/* 8 */
#define SD_LPIB		0x04	/* 32: link position in buffer */
#define SD_CBL		0x08	/* 32: cyclic buffer length */
#define SD_LVI		0x0C	/* 16: last valid index */
#define SD_FIFOS	0x10	/* 16 */
#define SD_FMT		0x12	/* 16: base/mult/div/type/bits/channels */
#define SD_BDPL		0x18	/* 32: bdl ptr low */
#define SD_BDPU		0x1C	/* 32: bdl ptr high */

static uint64_t mmio;
static uint32_t rd32(uint64_t o)  { return *(volatile uint32_t *)(uintptr_t)(mmio + o); }
static void wr32(uint64_t o, uint32_t v) { *(volatile uint32_t *)(uintptr_t)(mmio + o) = v; }
static uint16_t rd16(uint64_t o)  { return *(volatile uint16_t *)(uintptr_t)(mmio + o); }
static void wr16(uint64_t o, uint16_t v) { *(volatile uint16_t *)(uintptr_t)(mmio + o) = v; }
static uint8_t rd8(uint64_t o)    { return *(volatile uint8_t *)(uintptr_t)(mmio + o); }
static void wr8(uint64_t o, uint8_t v) { *(volatile uint8_t *)(uintptr_t)(mmio + o) = v; }

#define NCORB 256
static uint32_t *corb;		/* 256 x 4B verb buffer */
static uint64_t *rirb;		/* 256 x 8B response buffer */
static int corb_pos, rirb_pos;

/* QEMU's corb engine is BH-driven and lags a tight MMIO poll under
 * TCG — batch sends, drain at the end. send returns the wp index the
 * response will land at; wait polls for it. */
static int hda_verb_send(uint32_t v)
{
	uint16_t wp = (uint16_t)(corb_pos + 1);
	corb[wp] = v;
	corb_pos = wp;
	(void)corb[wp];
	wr16(R_CORBWP, wp);
	return wp;
}
/* QEMU writes responses at SEQUENTIAL rirb slots — rirbwp counts
 * responses issued, not corb indices. Consume in order. */
static int hda_verb_wait(int wp, uint32_t *resp)
{
	for (int i = 0; i < 20000000; i++) {
		if (rd16(R_RIRBWP) != rirb_pos) {
			rirb_pos = rd16(R_RIRBWP) & 0xff;
			if (resp) *resp = (uint32_t)rirb[rirb_pos];
			wr8(R_RIRBSTS, 1);
			return 0;
		}
		if (!(i & 0xfffff)) wr16(R_CORBWP, wp);
	}
	return -1;
}
static int hda_verb(uint32_t v, uint32_t *resp)
{
	return hda_verb_wait(hda_verb_send(v), resp);
}

#define VERB(cad, nid, v, p) \
	((uint32_t)(cad) << 28 | (uint32_t)(nid) << 20 | \
	 (uint32_t)(v) << 8 | (uint32_t)(p))
#define V_GET_PARAM	0xF00
#define V_GET_CONNSEL	0xF01
#define V_SET_CONNSEL	0x701
#define V_GET_CONV	0xF06
#define V_SET_STREAM	0x706
#define V_SET_FMT	0x200
#define V_SET_POWER	0x705
#define V_SET_AMP_OUT	0x300
#define V_SET_AMP_IN	0x360
#define V_GET_CFG_DEF	0xF1C
#define P_VENDOR	0x00
#define P_REVISION	0x02
#define P_AFG_CAPS	0x08
#define P_AUDIO_CAPS	0x09
#define P_PIN_CAPS	0x0C
#define P_OUT_STREAM	0x12

static int codec = -1;

int hda_probe(struct pci_device *dev)
{
	/* enable mmio + bus master */
	uint32_t cmd = cfg32(dev, 0x04) | 0x6;
	uint32_t a = 0x80000000u | ((uint32_t)dev->addr.bus << 16) |
	    ((uint32_t)dev->addr.devid << 11) |
	    ((uint32_t)dev->addr.function << 8) | 0x04;
	outl(0xcf8, a); outl(0xcfc, cmd);
	mmio = ((uint64_t)cfg32(dev, 0x14) << 32) | (cfg32(dev, 0x10) & ~0xfu);
	DBG("hda: pci %04x:%04x bar0=%llx", dev->id.vendor_id,
	    dev->id.device_id, (unsigned long long)mmio);

	/* reset: CRST 0 -> wait -> 1 -> wait */
	wr32(R_GCTL, 0);
	for (int i = 0; i < 100000; i++) { if (!(rd32(R_GCTL) & 1)) break; }
	for (volatile int i = 0; i < 100000; i++);
	wr32(R_GCTL, 1);
	for (int i = 0; i < 200000; i++) { if (rd32(R_GCTL) & 1) break; }
	/* codec discover: poll STATESTS — QEMU may not model wake bits,
	 * so also try verbs directly per-cad later */
	wr16(R_WAKEEN, 0x7fff);
	uint16_t st = 0;
	for (int i = 0; i < 2000000 && !st; i++)
		st = rd16(R_STATESTS);
	DBG("hda: gcap=%04x statests=%04x", rd16(R_GCAP), st);

	/* CORB/RIRB dma rings, 256 entries each */
	corb = uk_malloc(uk_alloc_get_default(), NCORB * 4 + 256);
	corb = (uint32_t *)(((uintptr_t)corb + 255) & ~255ul);
	memset(corb, 0, NCORB * 4);
	rirb = uk_malloc(uk_alloc_get_default(), NCORB * 8 + 256);
	rirb = (uint64_t *)(((uintptr_t)rirb + 255) & ~255ul);
	memset(rirb, 0xff, NCORB * 8);	/* sentinel: QEMU writes response|ex */

	wr8(R_CORBCTL, 0);			/* stop corb */
	for (int i = 0; i < 100000 && (rd8(R_CORBCTL) & 2); i++);
	wr8(R_CORBSIZE, 0x02);			/* 256 entries */
	wr32(R_CORBLBASE, (uint32_t)(uintptr_t)corb);
	wr32(R_CORBUBASE, 0);
	wr16(R_CORBRP, 0x8000);			/* reset rp */
	for (volatile int i = 0; i < 100; i++);
	wr16(R_CORBRP, 0);
	wr16(R_CORBWP, 0); corb_pos = 0;
	wr8(R_CORBCTL, 0x03);			/* cme + run */

	wr8(R_RIRBCTL, 0);
	for (int i = 0; i < 100000 && (rd8(R_RIRBCTL) & 2); i++);
	wr8(R_RIRBSIZE, 0x02);
	wr32(R_RIRBLBASE, (uint32_t)(uintptr_t)rirb);
	wr32(R_RIRBUBASE, 0);
	wr16(R_RIRBWP, 0x8000);			/* reset wp */
	wr16(R_RINTCNT, 0x00ff);
	wr8(R_RIRBSTS, rd8(R_RIRBSTS));		/* clear */
	wr8(R_RIRBCTL, 0x03);

	uint32_t r;
	for (int c = 0; c < 4; c++) {
		if (hda_verb(VERB(c, 0, V_GET_PARAM, P_VENDOR), &r) == 0 && r) {
			DBG("hda: codec%d vid=%04x rev=%04x", c, r >> 16, r & 0xffff);
			if (codec < 0) codec = c;
		}
	}
	if (codec < 0) { DBG("hda: no codec answered"); return -2; }
	hda_verb(VERB(codec, 0, V_GET_PARAM, P_REVISION), &r);
	DBG("hda: codec%d afn=%02x fgrp=%04x", codec, r & 0xff, (r >> 8) & 0xffff);
	return codec;
}

/* ---- M2: output stream ---- */

#define V_GET_SUBNODE	0xF00
#define P_SUBNODE	0x04
#define V_SET_PIN_CTL	0x707

struct bdl { uint64_t addr; uint32_t len; uint32_t ioc; } __attribute__((packed));

static uint32_t sd_out_off;		/* first output stream desc offset */

static int hda_stream_play(const void *pcm, uint32_t len)
{
	/* stream descriptors: inputs first (iss), then outputs (oss) */
	uint16_t gcap = rd16(R_GCAP);
	int iss = (gcap >> 12) & 0xf;
	sd_out_off = R_SD0 + iss * 0x20;

	/* widget walk: AFG at nid1 -> subnodes -> out-conv + pin */
	uint32_t r;
	int rc0 = hda_verb(VERB(codec, 0, V_GET_PARAM, P_VENDOR), &r);
	DBG("hda: replay-vendor rc=%d r=%08x corbrp=%d rirbwp=%d corbctl=%02x rirbctl=%02x",
	    rc0, rc0 ? 0 : r, rd16(R_CORBRP), rd16(R_RIRBWP), rd8(R_CORBCTL), rd8(R_RIRBCTL));
	/* find the function-group node carrying a subnode count (usually
	 * nid1 = AFG; don't assume — scan) */
	int afg = -1, start = 0, cnt = 0;
	uint32_t res[16]; int wps[16];
	for (int n = 0; n < 16; n++)
		wps[n] = hda_verb_send(VERB(codec, n, V_GET_PARAM, P_SUBNODE));
	for (int n = 0; n < 16; n++) {
		uint32_t rr = 0;
		hda_verb_wait(wps[n], &rr);	/* sequential consume */
		res[n] = rr;
		int sn = (res[n] >> 16) & 0xff, sc = res[n] & 0xff;
		if (sc && afg < 0) { afg = n; start = sn; cnt = sc; }
	}
	DBG("hda: subnode scan r0=%08x r1=%08x r2=%08x", res[0], res[1], res[2]);
	DBG("hda: afg nid=%d nodes start=%d cnt=%d", afg, start, cnt);
	if (afg < 0) return -1;
	int conv = -1, pin = -1;
	for (int n = start; n < start + cnt && n < 64; n++) {
		hda_verb(VERB(codec, n, V_GET_PARAM, P_AUDIO_CAPS), &r);
		int type = (r >> 20) & 0xf;
		if (type == 0 && conv < 0) conv = n;
		if (type == 4 && pin < 0)  pin = n;
	}
	DBG("hda: conv=%d pin=%d", conv, pin);
	if (conv < 0) return -1;

	/* power on: AFG + conv + pin -> D0 */
	hda_verb(VERB(codec, afg, V_SET_POWER, 0), NULL);
	hda_verb(VERB(codec, conv, V_SET_POWER, 0), NULL);
	if (pin >= 0) hda_verb(VERB(codec, pin, V_SET_POWER, 0), NULL);
	/* conv: stream 1 channel 0, fmt 48k/16bit/stereo = 0x0011 */
	hda_verb(VERB(codec, conv, V_SET_STREAM, 0x10), NULL);
	hda_verb(VERB(codec, conv, V_SET_FMT, 0x0011), NULL);
	/* unmute conv out-amp (gain max, both channels) */
	hda_verb(VERB(codec, conv, V_SET_AMP_OUT, 0x7f), NULL);
	hda_verb(VERB(codec, conv, V_SET_AMP_OUT, 0x7f), NULL);
	if (pin >= 0) {
		hda_verb(VERB(codec, pin, V_SET_CONNSEL, 0), NULL);
		hda_verb(VERB(codec, pin, V_SET_PIN_CTL, 0x40), NULL);
		hda_verb(VERB(codec, pin, V_SET_AMP_OUT, 0x7f), NULL);
		hda_verb(VERB(codec, pin, V_SET_AMP_OUT, 0x7f), NULL);
	}

	/* BDL: 2 entries, cyclic */
	struct bdl *b = uk_malloc(uk_alloc_get_default(), 2 * sizeof(*b) + 64);
	b = (struct bdl *)(((uintptr_t)b + 127) & ~127ul);	/* BDL = 128B aligned */
	b[0].addr = (uint64_t)(uintptr_t)pcm; b[0].len = len / 2; b[0].ioc = 0;
	b[1].addr = (uint64_t)(uintptr_t)pcm + len / 2; b[1].len = len - len / 2; b[1].ioc = 0;

	uint64_t sd = sd_out_off;
	wr32(sd + SD_CTL, 0);			/* reset: RST bit twice */
	wr32(sd + SD_CTL, 1);
	for (volatile int i = 0; i < 10000; i++);
	wr32(sd + SD_CTL, 0);
	for (volatile int i = 0; i < 10000; i++);
	wr32(sd + SD_CBL, len);
	wr16(sd + SD_LVI, 1);			/* 2 bdl entries, lvi=1 */
	wr16(sd + SD_FMT, 0x0011);		/* 48k/16/stereo */
	wr16(sd + SD_FIFOS, 0x0100);		/* fifo 256B */
	wr32(sd + SD_BDPL, (uint32_t)(uintptr_t)b);
	wr32(sd + SD_BDPU, 0);
	/* stream tag 1 in CTL bits 20-23 + RUN bit 1 */
	wr32(R_INTCTL, 0xc0000000);		/* global + corb irq enable */
	wr32(sd + SD_CTL, (1u << 20) | 6);	/* run + ice */
	DBG("hda: stream run sd_off=%llx cbl=%u ctl=%06x lpib=%u",
	    (unsigned long long)sd, len, rd32(sd + SD_CTL) & 0xffffff,
	    rd32(sd + SD_LPIB));
	return 0;
}

int hda_play_tone(void)
{
	/* 48kHz 16-bit stereo square-ish tone: alternating loud blocks —
	 * distinctive enough for a host-side energy/pattern check */
	uint32_t n = 48000;			/* 1 second of frames */
	int16_t *pcm = uk_malloc(uk_alloc_get_default(), n * 4);
	uint32_t phase = 0;
	for (uint32_t i = 0; i < n; i++) {
		/* 500 Hz square wave, both channels */
		int16_t v = ((phase >> 6) & 1) ? 20000 : -20000;
		pcm[i * 2] = v; pcm[i * 2 + 1] = v;
		phase = (phase + 1) & 0x1ff;
	}
	int r = hda_stream_play(pcm, n * 4);
	DBG("hda: pcm[0]=%d pcm[5000]=%d", pcm[0], pcm[5000]);
	for (volatile int i = 0; i < 50000000; i++);	/* ~1s spin */
	DBG("hda: lpib=%u", rd32(sd_out_off + SD_LPIB));
	return r;
}
