#!/usr/bin/env bash
# usb-xhci acceptance oracle.
# Positive: guest probes qemu-xhci, enumerates usb-kbd — serial shows
# port attach, slot id, VID:PID 0627:0001. Evidence is the serial log.
# Negative: qemu-xhci with NO devices attached MUST produce zero
# enumerated slots (the falsifiable clause from entry.md), not a crash.
set -u
cd "$(dirname "$0")"

IMG=app/.unikraft/build/usbxhci_qemu-x86_64
EV=evidence
mkdir -p "$EV"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

# ---- positive case ------------------------------------------------------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device qemu-xhci -device usb-kbd \
  > "$EV/serial.log" 2>&1

grep -aq 'xhci: pci 1b36:000d' "$EV/serial.log" \
  || fail "xhci controller never probed (see $EV/serial.log)"
grep -aq 'xhci: running' "$EV/serial.log" \
  || fail "controller never reached running (see $EV/serial.log)"
grep -aq 'xhci: port.*ccs=1' "$EV/serial.log" \
  || fail "no port attach detected (see $EV/serial.log)"
grep -aq 'xhci: enable slot -> id=1' "$EV/serial.log" \
  || fail "EnableSlot never completed (see $EV/serial.log)"
grep -aq 'xhci: enumerated vid=0627 pid=0001' "$EV/serial.log" \
  || fail "usb-kbd VID:PID 0627:0001 not enumerated (see $EV/serial.log)"

# ---- negative: no devices must enumerate zero slots, not crash ----------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device qemu-xhci \
  > "$EV/serial-nodev.log" 2>&1

grep -aq 'xhci: pci 1b36:000d' "$EV/serial-nodev.log" \
  || fail "controller missing on empty-bus run (see $EV/serial-nodev.log)"
grep -aq 'xhci: no devices attached' "$EV/serial-nodev.log" \
  || fail "empty bus did not enumerate zero slots (see $EV/serial-nodev.log)"
! grep -aq 'xhci: enumerated' "$EV/serial-nodev.log" \
  || fail "phantom device enumerated with none attached"
! grep -aq 'CRIT\|Unikraft Crash' "$EV/serial-nodev.log" \
  || fail "guest crashed on empty bus (see $EV/serial-nodev.log)"

echo "accept.sh: PASS — qemu-xhci enumerated usb-kbd 0627:0001; empty bus yielded zero slots"
