#!/usr/bin/env bash
# virtio-input acceptance oracle.
# Positive: virtio-keyboard-pci + virtio-tablet-pci (disable-legacy=on
# exercises the modern transport); QMP injects send-key + abs pointer
# events; guest serial shows evdev-class events decoded to the
# injection.  Negative: no input devices → no identification, no
# events — a check that can only confirm is not a gate.
set -u
cd "$(dirname "$0")"

IMG=app/.unikraft/build/vinput_qemu-x86_64
EV=evidence
QMP=/tmp/vinput-accept-qmp
mkdir -p "$EV"

fail() { echo "accept.sh: FAIL — $*"; exit 1; }

[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"
rm -f "$QMP"

qmp() { printf '%s\n' "$@" | nc -U "$QMP" -w 3 >/dev/null; }

# ---- positive: devices present, inject + match -------------------------
timeout 55 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device virtio-keyboard-pci,disable-legacy=on \
  -device virtio-tablet-pci,disable-legacy=on \
  -qmp unix:"$QMP",server,nowait \
  > "$EV/serial.log" 2>&1 &
sleep 4
i=0; while [ ! -S "$QMP" ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i+1)); done
[ -S "$QMP" ] || fail "QMP socket never appeared"
# one connection — every new QMP session needs qmp_capabilities first
printf '%s\n' \
  '{"execute":"qmp_capabilities"}' \
  '{"execute":"send-key","arguments":{"keys":[{"type":"qcode","data":"a"}]}}' \
  '{"execute":"send-key","arguments":{"keys":[{"type":"qcode","data":"b"}]}}' \
  '{"execute":"input-send-event","arguments":{"events":[{"type":"abs","data":{"axis":"x","value":512}},{"type":"abs","data":{"axis":"y","value":384}}]}}' \
  | nc -U "$QMP" -w 3 >/dev/null
sleep 30

grep -aq "vinput: virtio-input 1af4:1052" "$EV/serial.log" \
  || fail "no virtio-input device probed (see $EV/serial.log)"
grep -aq "vinput: device name 'QEMU Virtio Keyboard'" "$EV/serial.log" \
  || fail "keyboard never identified (see $EV/serial.log)"
grep -aq "vinput: device name 'QEMU Virtio Tablet'" "$EV/serial.log" \
  || fail "tablet never identified (see $EV/serial.log)"
grep -aq 'vinput: DRIVER_OK, eventq armed' "$EV/serial.log" \
  || fail "virtio-input never reached DRIVER_OK (see $EV/serial.log)"
grep -aq 'virtio-input: ev type=1 code=30 .*a=down' "$EV/serial.log" \
  || fail "send-key 'a' press never surfaced (see $EV/serial.log)"
grep -aq 'virtio-input: ev type=1 code=48 .*b=down' "$EV/serial.log" \
  || fail "send-key 'b' press never surfaced (see $EV/serial.log)"
grep -aq 'virtio-input: ev type=3 code=0 ' "$EV/serial.log" \
  || fail "tablet abs-X event never surfaced (see $EV/serial.log)"

# ---- negative: devices absent ------------------------------------------
timeout 35 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  > "$EV/serial-none.log" 2>&1 || true
if grep -aq 'vinput: DRIVER_OK' "$EV/serial-none.log"; then
  fail "DRIVER_OK with no devices present — gate cannot say no"
fi
if grep -aq 'virtio-input: ev ' "$EV/serial-none.log"; then
  fail "events surfaced with no devices present — gate cannot say no"
fi
grep -aq 'virtio-input: no devices present' "$EV/serial-none.log" \
  || fail "guest didn't report device absence (see $EV/serial-none.log)"

echo "accept.sh: PASS — keyboard+tablet identified, injected keys+pointer decoded, absent case refused"
