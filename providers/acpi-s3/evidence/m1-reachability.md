# acpi-s3 M1 reachability measurement (devin, w1C:p4T)

## Question: does QEMU pc/q35 support S3 under -kernel?
**Suspend: YES — measured. Resume: NOT via the naive SLP write —
needs the FACS firmware_waking_vector wired to a real-mode trampoline.**

## Measured (serial-m1.log, monitor `info status`)
- `pm1cnt pc(0xb004)=0xffff absent; q35(0x604)=0x0000` — QEMU here
  exposes the q35 ICH9 ACPI block.
- `outw(0x604, SLP_EN|SLP_TYP_S3)` → monitor reports
  `VM status: paused (suspended)` — QEMU honored the suspend request.
- `system_wakeup` → monitor reports `paused (shutdown)` — the machine
  powered off; the guest never resumed (no post-suspend counter on
  serial). The resume path needs FACS.firmware_waking_vector pointing
  at a real-mode wake trampoline — the piece the naive path lacks.

## Open question (not ruled out)
SeaBIOS *does* run under `-kernel` (it's the firmware that loaded the
kernel), so the firmware wake-vector route is plausible but unmeasured:
a real attempt needs the RSDP→RSDT→FADT→FACS walk + a 16-bit trampoline
in low RAM. Blocked on low-mem mapping: `uk_paging_pt_get_active`/
`uk_paging_page_kmap` are behind `CONFIG_LIBUKPAGING`, whose dep chain
(LIBUKFALLOC → LIBUKFALLOC_BUDDY → allocators) isn't yet resolved in
the copied build config.
