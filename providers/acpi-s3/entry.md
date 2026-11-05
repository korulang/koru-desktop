---
name: acpi-s3
iface: koruos/power.s3
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: []
accept: accept.sh — guest suspends to S3 and resumes with state intact
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# acpi-s3

Suspend-to-RAM. On the gap list (power management). Partial substrate
exists: koru-os native has ACPI discovery (RSDP/XSDT walk, MADT/FADT,
`_S5` AML decode) and a working **S5 poweroff door** (PLAN.md — "QEMU
exits on them"). S3 is different: it needs ACPI sleep-state entry
(SLP_TYP S3 write + resume path + state save/restore), not just poweroff.

## Acceptance

`accept.sh` boots, issues the S3 sleep sequence, QEMU actually powers
down/up (or `-no-shutdown` pause/resume via monitor if S3 isn't wired),
and the guest resumes to a shell that can prove state survived (a
marker file + a running counter). Host-side: QEMU monitor log shows the
power transition; serial shows post-resume banner. Falsifiable: a guest
that writes SLP_TYP and hangs without QEMU suspending fails.

## Notes

- **Unmeasured whether QEMU's pc machine supports S3 resume for a
  `-kernel` boot.** Standard QEMU S3 needs firmware (SeaBIOS/OVMF) to
  own the sleep entry; a direct kernel boot may never get the resume
  vector. First task at claim time: measure whether S3 is reachable at
  all under `-kernel`, or whether this slot is `tier: hw`-only.
- If unreachable under -kernel, options: boot via firmware+GRUB
  (koru-os already has the mb2/OVMF arms), or reclassify the slot.
- Real laptops suspend constantly — this is a `tier: hw` requirement
  eventually even if QEMU can't test it.
