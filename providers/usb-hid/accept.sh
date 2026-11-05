#!/usr/bin/env bash
# usb-hid acceptance oracle.
# Positive: qemu-xhci + usb-kbd, keystrokes injected via the QEMU
# monitor `sendkey`; guest serial shows the HID interface (proto=1),
# endpoint config, SET_CONFIGURATION, then boot-protocol reports that
# decode to the injected scancodes (a=04, b=05).
# Negative: qemu-xhci with NO usb-kbd must produce no HID reports and
# no enumeration — a check that can only confirm is not a gate.
set -u
cd "$(dirname "$0")"

IMG=app/build/usbhid_qemu-x86_64
EV=evidence
MON=/tmp/usbhid-accept-mon
mkdir -p "$EV"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"
rm -f "$MON"

# ---- positive: inject keystrokes, expect decoded reports ---------------
timeout 50 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device qemu-xhci -device usb-kbd \
  -monitor unix:"$MON",server,nowait \
  > "$EV/serial.log" 2>&1 &
sleep 8
printf 'sendkey a\nsendkey b\n' | nc -U "$MON" -w 2 || fail "monitor socket write failed"
sleep 30

grep -aq 'xhci: enumerated vid=0627 pid=0001' "$EV/serial.log" \
  || fail "usb-kbd never enumerated (see $EV/serial.log)"
grep -aq 'usb-hid: iface=.*proto=1.*ep=0x81' "$EV/serial.log" \
  || fail "no HID boot-protocol interface found (see $EV/serial.log)"
grep -aq 'usb-hid: set-config 1 cc=1' "$EV/serial.log" \
  || fail "SET_CONFIGURATION failed (see $EV/serial.log)"
grep -aq 'usb-hid: report.*keys=04.*char=a' "$EV/serial.log" \
  || fail "sendkey 'a' never surfaced as a report (see $EV/serial.log)"
grep -aq 'usb-hid: report.*keys=05.*char=b' "$EV/serial.log" \
  || fail "sendkey 'b' never surfaced as a report (see $EV/serial.log)"

# ---- negative: no usb-kbd → no enumeration, no reports -----------------
timeout 40 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device qemu-xhci \
  > "$EV/serial-nodev.log" 2>&1

grep -aq 'xhci: no devices attached' "$EV/serial-nodev.log" \
  || fail "empty bus did not enumerate zero slots (see $EV/serial-nodev.log)"
! grep -aq 'usb-hid: report' "$EV/serial-nodev.log" \
  || fail "phantom HID report with no device attached"
! grep -aq 'usb-hid: configured' "$EV/serial-nodev.log" \
  || fail "HID configured with no device attached"
! grep -aq 'CRIT\|Unikraft Crash' "$EV/serial-nodev.log" \
  || fail "guest crashed on empty bus (see $EV/serial-nodev.log)"

echo "accept.sh: PASS — usb-kbd enumerated, sendkey a/b decoded as reports; empty bus yielded none"
