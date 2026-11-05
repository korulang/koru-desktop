---
name: usb-xhci
iface: koruos/usb.host
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration, irq-msix]
accept: accept.sh — guest enumerates a QEMU USB device on the xhci controller
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
---

# usb-xhci

USB 3.0 host controller (xHCI): register model, command/event/transfer
rings, slot/context management, device enumeration. **Nothing exists:**
Unikraft core and the lib-* catalog have no USB at all (verified against
`donors/unikraft-staging` — only Xen `usbif.h` *headers* for the Xen
platform, no driver). This is the root of the whole USB subtree —
`usb-hid` and `usb-storage` both need it.

## Acceptance

`accept.sh` boots with `-device qemu-xhci -device usb-kbd` (or
`usb-tablet`). Guest driver initializes the controller (host
controller reset, CRCR/DCBAAP setup), scans ports, allocates a slot for
the keyboard, reads its device descriptor. Host-side: serial log shows
enumeration (port status, slot id, VID:PID `0627:0001` for usb-kbd);
optionally the keyboard then produces input events via `usb-hid`.
Falsifiable: `-device qemu-xhci` with no devices must enumerate zero
slots, not crash.

## Notes

- QEMU's `qemu-xhci` is the USB-3 controller; `-device usb-ehci`/
  `piix3-usb-uhci` are older — xhci alone covers USB1/2/3 devices in
  QEMU, so one controller suffices for tier:qemu.
- Real-hw xhci varies wildly (Renesas, ASMedia, Intel) — `tier: hw`
  rung will need quirk handling; design the register-access layer to
  absorb it.
- Prior art outside Unikraft: NONOS has a working no-std xhci capsule
  (found in research — keybs/mice/disk on root ports only, no isoc);
  `suhteevah/xhci-nostd` is a Rust no_std reference. Worth reading
  before authorship — evidence class C until read.
