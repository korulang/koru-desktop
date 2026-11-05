# acpi-s3 claim — working state (devin, w1C:p4T, branch provider/acpi-s3)

## Commission (LAST open tier:qemu slot)
- First task = MEASUREMENT, not authorship: does QEMU pc machine support S3
  suspend/resume for a -kernel boot AT ALL?
- If unreachable: (a) firmware+GRUB path (koru-os mb2/OVMF arms) or
  (b) reclassify tier:hw with finding recorded.
- DO NOT write driver before reachability verdict.
- Accept: guest issues S3 seq, QEMU power down/up (or -no-shutdown monitor
  pause/resume if S3 unwired), guest proves state survived (marker + counter);
  host-side monitor shows transition. Falsifiable: SLP_TYP hang without
  QEMU suspend FAILS.
- M1 = S3-reachability verdict (that IS the deliverable); M2 = full accept.

## Key knowledge carried in this session
- Build: gmake C=<dir>/.unikraft/build/config CONFIG_UK_BASE=/Users/larsde/src/unikraft
  A=/Users/larsde/src/koru-desktop/providers/<slot>/app  (inside .unikraft/build)
- QEMU persistent: `exec qemu ... -monitor unix:/tmp/x,server,nowait
  > evidence/serial.log 2>&1` with exec timeout:0 → persistent shell;
  `-no-shutdown` keeps QEMU + device state alive after guest poweroff.
- Python HMP client at /tmp/hmp.py (unix sock, waits banner, sends cmd).
- ACPI S3 on pc/q35: PM1_CNT @ ACPI FADT PM1a_CNT_BLK (usually IO 0x604/0x6004);
  SLP_TYPa + SLP_EN write → suspend. QEMU needs ACPI (pc machine default on).
  -kernel boot: SeaBIOS builds ACPI tables; unikraft must parse RSDT->FADT.
- QEMU monitor `info status` shows paused/suspended vs running;
  `system_wakeup` wakes S3.

## M1 MEASUREMENT RESULT (evidence/serial-m1.log)
- pc-i440fx pm1cnt(0xb004)=0xffff absent; q35(0x604)=0x0000 present →
  this QEMU machine exposes the q35 ICH9 ACPI block.
- SLP_TYP|SLP_EN write -> QEMU monitor: 'VM status: paused (suspended)'
  — QEMU HONORS the suspend under -kernel.
- system_wakeup -> monitor: 'paused (shutdown)' — machine powered off,
  guest never resumed (no post-suspend ctr on serial).
- VERDICT so far: suspend reachable, resume NOT via naive SLP write —
  the wake path needs FACS firmware_waking_vector + real-mode trampoline.
  SeaBIOS DID run (firmware exists under -kernel), so the wake-vector
  route is the open question, not ruled out.
## NEXT
- Parse RSDP->RSDT->FADT->FACS, write firmware_waking_vector to a 16-bit
  trampoline in low RAM, retry suspend+wake. If QEMU/SeaBIOS jumps there,
  resume is reachable. If it still dies -> tier:hw verdict honest.

## CRASH at find_rsdp — low BIOS region unmapped
- guest crashed reading 0x40e (EBDA) / 0xe0000-0xfffff — unikraft page
  tables don't map legacy BIOS memory.
- Fix: uk_paging_page_kmap(pt, paddr, size, 0) exists for temp phys maps
  (lib/ukpaging include/uk/paging.h:780). Or uk_paging_map the 0..1MB
  region before scanning. NEXT: map EBDA+BIOS range, retry rsdp walk.

## DEP CHAIN (CONFIG_LIBUKPAGING gate)
- uk_paging_pt_get_active/page_kmap behind CONFIG_LIBUKPAGING — off in copied config.
- Enabling cascades: LIBUKPAGING needs LIBUKFALLOC (uk/falloc.h),
  then LIBUKFALLOC_BUDDY (uk/fallocbuddy.h). Add deps until build clears,
  then retry rsdp->facs->wake-vector measurement.
- ALTERNATIVE if deps keep growing: FACS addr can be read by a Linux
  reference or fixed per QEMU machine — but parse-first is the honest path.
- pm1cnt=0x604 (q35) confirmed working — suspend half proven; only the
  FACS wake-vector wiring is missing for the resume test.
