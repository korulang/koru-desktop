---
name: usb-hid
iface: koruos/input.usb-hid
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [usb-xhci]
accept: accept.sh — usb-kbd/usb-mouse events reach evdev; host sees them
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
---

# usb-hid

USB HID class driver: keyboards, mice, tablets — boot-protocol or
report-protocol input reports → evdev events. Depends on `usb-xhci`.
This is the slot that makes the desktop *usable* on anything but a
PS/2 port — real laptops have no PS/2 keyboard path for USB-only
internal keyboards, and QEMU's `usb-kbd`/`usb-tablet` are the tier:qemu
stand-ins.

## Acceptance

`accept.sh` boots `-device qemu-xhci -device usb-kbd -device usb-mouse`,
injects keystrokes and pointer motion via QMP `send-key`/`mouse-move`,
and the guest surfaces them as evdev events. Host-side oracle: the
serial log (or a guest dump the script pulls) contains the injected
scancodes/button state; stronger form — unmodified `libinput
 debug-events` or `evtest` output captured on serial matches the
injected sequence (the M7b oracle shape). Falsifiable: `-device
usb-kbd` absent → no events, script fails.

## Notes

- Boot protocol (8-byte reports) is the honest first pass; report
  protocol adds parsing for consumer-control keys (volume, brightness)
  — the remote/voice grammar omni wants may live here later.
- `usb-tablet` (absolute pointer) is the better QEMU device for a
  desktop than `usb-mouse` (relative) — claim-time choice.
- Touchpads are I2C/HID-over-I2C or USB HID — the `tier: hw` rung will
  meet them; keep the report parser device-agnostic.
