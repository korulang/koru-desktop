---
name: bluetooth
iface: koruos/net.bluetooth
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration]
accept: deferred — tier: hw, no QEMU device exists
qemu: none
tier: hw
status: open
home: this-repo
donor: unikraft-fork
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# bluetooth

Bluetooth: USB or PCIe transport + HCI layer + L2CAP/ATT/SM + profiles
(A2DP audio, HID input, HFP). Nothing exists in Unikraft; QEMU has no
bluetooth device emulation at all, so this slot is `tier: hw` from day
one.

## Acceptance

Deferred — real hardware only. When claimed: a known dongle/device
pair, an OBEX or A2DP transfer or HID connection, host-side evidence
(packet capture or the peer device's observed behavior). `accept.sh`
written at claim time against the actual hardware.

## Notes

- Almost certainly `needs: [usb-xhci]` in practice — USB BT dongles are
  the cheap target; PCIe BT (Broadcom/Intel) needs `pci-enumeration` +
  firmware.
- Ordering: realistically last on the net/audio subtree — it depends on
  USB *and* wants audio/input profiles to be useful. Seeded so the gap
  list is complete, not because it's near-term.
- Firmware + regulatory domain are hw concerns; note them at claim.
