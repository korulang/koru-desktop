#!/usr/bin/env bash
# blk-nvme acceptance oracle.
# Positive: guest boots against -device nvme,serial=korudesktop backed by
# a host image file, identifies the namespace by serial, reads the
# host-planted pattern at LBA 8, writes a second pattern at LBA 16 and
# flushes. Host then dd/cmp's the image at LBA 16 — guest cannot fake
# host bytes. Negative: a wrong serial MUST be refused by the guest.
set -u
cd "$(dirname "$0")"

IMG=app/.unikraft/build/blknvme_qemu-x86_64
EV=evidence
NSIMG="$EV/ns.img"
mkdir -p "$EV"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"
command -v qemu-system-x86_64 >/dev/null || fail "qemu-system-x86_64 not found"

# ---- plant the host pattern at LBA 8 ------------------------------------
dd if=/dev/zero of="$NSIMG" bs=1M count=16 2>/dev/null
printf 'KORUDESKTOP-HOST-PATTERN-A' | dd of="$NSIMG" bs=512 seek=8 conv=notrunc 2>/dev/null

# ---- positive run -------------------------------------------------------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -drive if=none,id=nvme0,file="$NSIMG",format=raw \
  -device nvme,serial=korudesktop,drive=nvme0 \
  > "$EV/serial.log" 2>&1

grep -aq "nvme: controller serial 'korudesktop'" "$EV/serial.log" \
  || fail "guest never identified the namespace serial (see $EV/serial.log)"
grep -aq 'nvme: host pattern A verified at lba8' "$EV/serial.log" \
  || fail "guest did not verify host pattern A at LBA 8 (see $EV/serial.log)"
grep -aq 'nvme: write lba16 rc=0' "$EV/serial.log" \
  || fail "guest write to LBA 16 did not succeed (see $EV/serial.log)"
grep -aq 'nvme: flush rc=0' "$EV/serial.log" \
  || fail "guest flush did not succeed (see $EV/serial.log)"

# ---- host-side verify: the guest cannot fake these bytes ----------------
dd if="$NSIMG" bs=512 skip=16 count=1 2>/dev/null | head -c 27 > "$EV/lba16.out"
GOT=$(cat "$EV/lba16.out")
[ "$GOT" = "KORUDESKTOP-GUEST-PATTERN-B" ] \
  || fail "host read of LBA 16 shows '$GOT' — guest bytes absent from image"

# ---- negative control: wrong serial must be refused ---------------------
dd if=/dev/zero of="$NSIMG.bad" bs=1M count=16 2>/dev/null
printf 'KORUDESKTOP-HOST-PATTERN-A' | dd of="$NSIMG.bad" bs=512 seek=8 conv=notrunc 2>/dev/null
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -drive if=none,id=nvme0,file="$NSIMG.bad",format=raw \
  -device nvme,serial=WRONGSERIAL,drive=nvme0 \
  > "$EV/serial-bad.log" 2>&1
grep -aq "nvme: refused: serial 'WRONGSERIAL' != 'korudesktop'" "$EV/serial-bad.log" \
  || fail "guest accepted a wrong-serial device — the serial check is not real"
if grep -aq 'nvme: write lba16 rc=0' "$EV/serial-bad.log"; then
  fail "guest wrote to a device it should have refused — oracle broken"
fi

echo "accept.sh: PASS — serial identified, pattern A read, pattern B on host image, wrong serial refused"
exit 0
