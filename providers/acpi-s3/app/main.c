/* acpi-s3 reachability measurement — parse ACPI tables, wire the FACS
 * firmware_waking_vector to a real-mode 'W' stub, then SLP_TYP|SLP_EN.
 * If QEMU/SeaBIOS jumps the wake vector on resume, serial shows 'W'.
 * Original work against ACPI spec + QEMU pc/q35 PM layout. BSD-3-Clause. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <uk/essentials.h>
#include <uk/paging.h>

static inline void outw(uint16_t p, uint16_t v)
{ __asm__ __volatile__("outw %0,%1" :: "a"(v), "dN"(p)); }
static inline uint16_t inw(uint16_t p)
{ uint16_t v; __asm__ __volatile__("inw %1,%0" : "=a"(v) : "dN"(p)); return v; }
static inline uint32_t rd32(uint64_t a) { return *(volatile uint32_t *)(uintptr_t)a; }

#define PM1_CNT_Q35	0x604
#define PM1_CNT_PC	0xb004
#define SLP_EN		(1 << 13)
#define SLP_TYP_S3	(0x1 << 10)
#define WAKE_ADDR	0x0000000000008000ULL

/* real-mode stub: mov dx,0x3f8 ; mov al,'W' ; out dx,al ; hlt ; jmp -5 */
static const uint8_t stub[] = {
	0xba, 0xf8, 0x03, 0xb0, 'W', 0xee, 0xf4, 0xeb, 0xf9
};

static __vaddr_t low;  /* kmap of phys 0..1MB for BIOS/table reads */

static uint64_t find_rsdp(void)
{
	struct uk_pagetable *pt = uk_paging_pt_get_active();
	/* map the low 1MB so EBDA + BIOS scan is safe */
	low = uk_paging_page_kmap(pt, 0, 256, 0);
	uint16_t ebda_seg = *(volatile uint16_t *)(low + 0x40e);
	uint64_t ebda = (uint64_t)ebda_seg << 4;
	uint64_t lo = ebda ? ebda : 0xe0000;
	uint64_t hi = 0x100000;
	for (uint64_t a = lo; a < hi; a += 16)
		if (!memcmp((void *)(low + a), "RSD PTR ", 8))
			return a;
	return 0;
}

int main(void)
{
	printf("acpi-s3: guest up\n");
	for (volatile long i = 0; i < 20000000; i++);
	uint64_t rsdp = find_rsdp();
	printf("acpi-s3: rsdp=%lx\n", (unsigned long)rsdp);
	if (!rsdp) { printf("acpi-s3: REFUSE no rsdp\n"); goto out; }
	uint64_t rsdp_v = low + rsdp;
	uint64_t rsdt = rd32(rsdp_v + 0x18);      /* rsdt phys addr */
	printf("acpi-s3: rsdt=%lx\n", (unsigned long)rsdt);
	/* scan rsdt entries for FACP */
	uint32_t nent = (rd32(low + rsdt + 4) - 36) / 4;
	uint64_t fadt = 0;
	for (uint32_t i = 0; i < nent; i++) {
		uint64_t t = rd32(low + rsdt + 36 + i * 4);
		if (!memcmp((void *)(low + t), "FACP", 4)) { fadt = t; break; }
	}
	printf("acpi-s3: fadt=%lx\n", (unsigned long)fadt);
	if (!fadt) { printf("acpi-s3: REFUSE no fadt\n"); goto out; }
	uint64_t facs = rd32(low + fadt + 0x24);      /* FIRMWARE_CTRL */
	uint64_t xfac = rd32(low + fadt + 0x84);      /* X_FIRMWARE_CTRL */
	uint32_t pm1a = rd32(low + fadt + 0x4c);      /* PM1a_CNT_BLK */
	printf("acpi-s3: facs=%lx xfac=%lx pm1a=%04x\n",
	    (unsigned long)facs, (unsigned long)xfac, pm1a);

	/* plant the real-mode wake stub + point FACS at it */
	memcpy((void *)(low + WAKE_ADDR), stub, sizeof(stub));
	uint64_t fv = facs ? facs : xfac;
	*(volatile uint32_t *)(uintptr_t)(low + fv + 12) = (uint32_t)WAKE_ADDR;
	printf("acpi-s3: wake vector -> %lx\n", (unsigned long)WAKE_ADDR);

	uint16_t cnt = pm1a ? (uint16_t)pm1a : (inw(PM1_CNT_PC) != 0xffff ? PM1_CNT_PC : PM1_CNT_Q35);
	printf("acpi-s3: suspend via pm1cnt=%04x\n", cnt);
	outw(cnt, SLP_EN | SLP_TYP_S3);
	for (volatile long i = 0; i < 50000000; i++);
	printf("acpi-s3: back — SLP write didn't suspend\n");
out:
	for (;;) { }
}
