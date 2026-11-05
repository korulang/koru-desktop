#!/usr/bin/env bash
# usb-storage acceptance oracle.
# Positive: guest boots under qemu-xhci with a host-planted usb-storage
# disk, INQUIRYs the LUN, reads the host pattern at sector 8, writes a
# second pattern at sector 9. Host-side dd verifies the write — guest
# cannot fake host bytes.
# Negative: same image with no usb device MUST fail.
set -u
cd "$(dirname "$0")"

IMG=app/build/usbstorage_qemu-x86_64
EV=evidence
DISK=$EV/usb.img
fail() { echo "accept.sh: FAIL — $*"; exit 1; }
mkdir -p "$EV"
[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

# host-planted disk: zeroed + pattern at sector 8
dd if=/dev/zero of="$DISK" bs=512 count=64 2>/dev/null
printf 'USBS-HOST-PLANTED-PATTERN-AAAA' | dd of="$DISK" bs=1 seek=$((8*512)) conv=notrunc 2>/dev/null

# ---- positive -----------------------------------------------------------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device qemu-xhci \
  -drive if=none,id=usbstick,file="$DISK",format=raw \
  -device usb-storage,drive=usbstick \
  > "$EV/serial.log" 2>&1

grep -aq "msc: INQUIRY vendor='QEMU" "$EV/serial.log" \
  || fail "no INQUIRY on serial (see $EV/serial.log)"
grep -aq "sector8 pattern='USBS-HOST-PLANTED-PATTERN-AAAA" "$EV/serial.log" \
  || fail "guest did not read the host pattern"
grep -aq "sector9 pattern='USBS-GUEST-WROTE-THIS" "$EV/serial.log" \
  || fail "guest did not verify its own write"
dd if="$DISK" bs=512 skip=9 count=1 2>/dev/null | head -c 21 > "$EV/host-verify.log"
grep -aq 'USBS-GUEST-WROTE-THIS' "$EV/host-verify.log" \
  || fail "host image lacks the guest's write at sector 9"

# ---- negative control: no usb-storage device ----------------------------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot -device qemu-xhci \
  > "$EV/serial-nodev.log" 2>&1
if grep -aq "INQUIRY\|sector8" "$EV/serial-nodev.log"; then
  fail "guest read a device that was never attached — oracle broken"
fi
grep -aq 'no devices\|no msc device' "$EV/serial-nodev.log" \
  || fail "negative run produced no refusal evidence"

echo "accept.sh: PASS — INQUIRY + host-pattern read + guest write verified host-side; no-device refused"
exit 0
