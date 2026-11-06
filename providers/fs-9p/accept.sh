#!/usr/bin/env bash
# fs-9p acceptance oracle.
# Positive: host share dir with planted file; guest mounts 'hostshare'
# via virtio-9p-pci, asserts planted bytes, writes guest-out.txt; host
# verifies guest-written bytes on the share — the guest cannot fake
# host-side file contents.
# Negative: wrong mount_tag → mount refuses → gate says no.
set -u
cd "$(dirname "$0")"

IMG=app/.unikraft/build/fs9p_qemu-x86_64
EV=evidence
SHARE="$EV/share"
mkdir -p "$SHARE"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

# ---- positive ----------------------------------------------------------
printf 'HOST-PLANTED-9P: provider=fs-9p\n' > "$SHARE/planted.txt"
rm -f "$SHARE/guest-out.txt"

timeout 45 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -fsdev local,path="$SHARE",security_model=none,id=fsdev0 \
  -device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare \
  > "$EV/serial.log" 2>&1 || true

grep -aq "fs-9p: mounted hostshare at /host" "$EV/serial.log" \
  || fail "guest never mounted hostshare (see $EV/serial.log)"
grep -aq "fs-9p: host-planted file verified byte-identical" "$EV/serial.log" \
  || fail "planted readback failed (see $EV/serial.log)"
grep -aq "fs-9p: wrote /host/guest-out.txt" "$EV/serial.log" \
  || fail "guest never wrote guest-out.txt (see $EV/serial.log)"
grep -aq "fs-9p: ACCEPTED" "$EV/serial.log" \
  || fail "guest never reached ACCEPTED (see $EV/serial.log)"

# host-side verification — the bytes must exist on the host share
[ -f "$SHARE/guest-out.txt" ] \
  || fail "guest-out.txt absent on host share"
grep -aq "GUEST-WROTE-9P: provider=fs-9p lane=compose" "$SHARE/guest-out.txt" \
  || fail "host-side bytes wrong in guest-out.txt"

# ---- negative: wrong mount_tag → mount must refuse ---------------------
timeout 40 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -fsdev local,path="$SHARE",security_model=none,id=fsdev0 \
  -device virtio-9p-pci,fsdev=fsdev0,mount_tag=WRONGTAG \
  > "$EV/serial-bad.log" 2>&1 || true
if grep -aq "fs-9p: mounted" "$EV/serial-bad.log"; then
  fail "mount succeeded on wrong tag — gate cannot say no"
fi
grep -aq "fs-9p: REFUSE mount" "$EV/serial-bad.log" \
  || fail "mount didn't refuse on wrong tag (see $EV/serial-bad.log)"

echo "accept.sh: PASS — host share mounted, planted bytes verified, guest write host-verified, wrong tag refused"
