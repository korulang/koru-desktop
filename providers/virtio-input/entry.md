---
name: virtio-input
iface: koruos/input.virtio
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration, virtio-pci-modern, irq-msix]
accept: accept.sh — virtio-keyboard/tablet events reach evdev; host injects and verifies
tier: qemu
status: landed
home: this-repo
evidence: V
license: BSD-3-Clause
provenance: app/ original work; PCI cap-walk + cfg helpers derived from this repo's providers/virtio-pci-modern/app/main.c (same author, BSD-3-Clause); vring/input protocol from virtio 1.0 spec §2.7/§5.10; no foreign lifts
---

# virtio-input

Virtio input devices: keyboard, mouse, tablet — event_idx/status
queues of `struct virtio_input_event` reports. Unblocked now:
`virtio-pci-modern` landed (cap-list transport proven), `usb-hid`
landed (the evdev consumer + oracle shape exists). QEMU devices:
`virtio-keyboard-pci`, `virtio-mouse-pci`, `virtio-tablet-pci` —
run with `disable-legacy=on` so they exercise the modern transport
this slot exists to prove.

## Acceptance

`accept.sh` boots `-device virtio-keyboard-pci,disable-legacy=on
-device virtio-tablet-pci,disable-legacy=on`, injects keys/pointer
via QMP `send-key`/`mouse-move`, guest surfaces evdev events.
Host-side oracle identical to usb-hid's: serial/evtest/libinput
debug-events output matches the injected sequence; `-nic`-style
falsifier = devices absent → no events → fail.

## Notes

- Redundant with `usb-hid` on QEMU deliberately: USB proves the
  physical-device path, virtio-input proves the paravirt path —
  the native arm will want virtio-input long before USB xhci.
- `virtio-tablet` is absolute-pointer — the right QEMU tablet for
  a desktop; claim-time choice between mouse (relative) and tablet.
- Prior art: virtio 1.0 spec §5.10 (input); QEMU `hw/input/virtio-input.c`
  is the device-model reference (read-only).

## Decision record

- **Chosen:** `donor-c` new driver (cap-walk → VERSION_1 → split vrings →
  eventq decode); devices keyboard + tablet — tablet is absolute-pointer,
  the desktop-right QEMU device.
- **Alternative not taken:** extending the usb-hid path (different
  transport + report format), virtio-mouse relative-pointer.
