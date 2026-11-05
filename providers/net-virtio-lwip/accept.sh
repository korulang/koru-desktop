#!/usr/bin/env bash
# net-virtio-lwip acceptance oracle.
# Positive: guest boots under -netdev user, gets a slirp lease, and
# TCP-connects out to the host listener this script runs. Evidence is
# host-side: the listener's accept log + the guest's own serial report.
# Negative: same image under -nic none MUST fail to connect.
set -u
cd "$(dirname "$0")"

IMG=app/build/netlwip_qemu-x86_64
EV=evidence
PORT=5555
mkdir -p "$EV"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

# ---- positive case ------------------------------------------------------
: > "$EV/listener.log"
nc -l "$PORT" > "$EV/listener.log" 2>&1 &
NCPID=$!
sleep 1
timeout 40 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
  > "$EV/serial.log" 2>&1
kill "$NCPID" 2>/dev/null

grep -aq 'netlwip: connect try=.* rc=0' "$EV/serial.log" \
  || fail "guest never connected (see $EV/serial.log)"
grep -aq 'netlwip: local 10\.0\.2\.' "$EV/serial.log" \
  || fail "guest reported no slirp lease (see $EV/serial.log)"
grep -aq 'guest-says-hi' "$EV/listener.log" \
  || fail "host listener never received guest payload"

# ---- negative control: -nic none must fail ------------------------------
timeout 40 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot -nic none \
  > "$EV/serial-nonet.log" 2>&1
if grep -aq 'netlwip: connect try=.* rc=0' "$EV/serial-nonet.log"; then
  fail "guest connected with -nic none — oracle is broken"
fi
grep -aq 'netlwip: connect try=.* rc=-1' "$EV/serial-nonet.log" \
  || fail "negative run produced no connect attempts at all"

echo "accept.sh: PASS — guest 10.0.2.x reached host:$PORT, -nic none refused"
exit 0
