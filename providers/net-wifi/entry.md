---
name: net-wifi
iface: koruos/net.wifi
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

# net-wifi

Wireless networking. On the gap list; the hardest realistic slot —
most Wi-Fi chips need (a) a bus driver (PCIe or USB — see `usb-xhci`
for the USB path), (b) firmware blobs (licensing per-chip), (c) a MAC
implementation, (d) cfg80211/mac80211-equivalent plumbing or a
monolithic driver.

## Acceptance

Deferred by design — `tier: hw` means no QEMU oracle exists. When
claimed for real hardware: associate to a known AP, DHCP, ping/HTTP to
a host on the LAN, evidence = serial log + host-side capture. The
`accept.sh` will be written against the actual hardware target, not
invented now.

## Notes

- Candidate first targets (research, unmeasured): USB Wi-Fi dongles
  (rtl88xx series — huge Linux driver corpus to crib from) ride the
  `usb-xhci` slot; PCIe cards (ath9k — open firmware, simplest MAC) ride
  `pci-enumeration`. USB is probably the cheaper first win.
- Firmware blobs: check redistribution terms per chip before any
  `origins.lock` entry — some are non-free.
- This is the slot most likely to force the `silo` lane (borrow Linux
  driver code behind provider rows) if authorship stalls — the
  challenge brief names that escape hatch.
