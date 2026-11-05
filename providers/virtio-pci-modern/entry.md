---
name: virtio-pci-modern
iface: koruos/virtio-pci
arms: [unikraft/linuxu-x86_64, unikraft/qemu-x86_64]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration]
accept: accept.sh — probe negotiates VERSION_1 on a virtio-1.0-only device
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: V
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# virtio-pci-modern

The virtio 1.0 PCI transport: capability-list parsing (common cfg, notify,
isr, device cfg, PCI cfg caps), modern queue layout, MSI-X. **The gap is
measured, not guessed:** Unikraft 0.21's `drivers/virtio/pci/virtio_pci.c`
is **legacy only** — I/O-port BAR0, no capability parsing (koru-os-cypoe
BLOCKERS.md B13, evidence class V). Every modern-only QEMU device
(`virtio-gpu-pci` non-transitional, `virtio-keyboard-pci`, `virtio-input`,
`virtio-fs`) is undrivable until this lands. It gates `drm-virtio-gpu`
and the USB-input path if virtio-input is chosen over usb-xhci.

## Acceptance

`accept.sh` builds a minimal probe (C, in the donor tree) that enumerates
a virtio-1.0-only QEMU device — `virtio-gpu-pci` is the natural first
target since `drm-virtio-gpu` needs it anyway — walks its PCI caps,
negotiates `VERSION_1`, and prints the negotiated fields to serial.
Host-side check: the serial log contains the negotiated version and
feature fields; a legacy-only transport cannot produce them (it would
read garbage or fail cap walk). Falsifiable: run against
`-device virtio-net-pci` transitional and confirm the probe takes the
cap path only when caps exist.

## Notes

Alternative donors: KolibriOS has no virtio; Linux `virtio_pci_modern`
is the reference ABI if a `silo` lane is ever chosen. Upstreaming to
unikraft/unikraft is plausible long-term — the transport is generic.
