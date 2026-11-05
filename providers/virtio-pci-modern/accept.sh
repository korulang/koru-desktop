#!/usr/bin/env bash
# virtio-pci-modern acceptance oracle.
# Positive: virtio-net-pci,disable-legacy=on (modern-only) — probe walks
# the PCI capability list (common/notify/isr/device/pci-cfg), negotiates
# VIRTIO_F_VERSION_1 through the common cfg, prints negotiated fields.
# Negative: virtio-net-pci,disable-modern=on (legacy-only) MUST be
# refused — a modern probe on a legacy path reads no caps and says no.
# Transitional (both paths present): cap path taken because caps exist.
set -u
cd "$(dirname "$0")"

IMG=app/build/vpcimodern_qemu-x86_64
EV=evidence
mkdir -p "$EV"
fail() { echo "accept.sh: FAIL — $*"; exit 1; }
[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

run() { timeout 30 qemu-system-x86_64 -cpu max -kernel "$IMG" -nographic \
    -no-reboot -netdev user,id=n0 "$@" 2>&1 | strings; }

# ---- positive: modern-only device --------------------------------------
run -device virtio-net-pci,disable-legacy=on > "$EV/serial.log"

grep -aq 'vpci-modern: virtio dev 1af4:1041' "$EV/serial.log" \
  || fail "modern virtio device never probed (see $EV/serial.log)"
grep -aq 'vpci-modern: cap.*type=common' "$EV/serial.log" \
  || fail "common cap never parsed (see $EV/serial.log)"
grep -aq 'vpci-modern: cap.*type=notify' "$EV/serial.log" \
  || fail "notify cap never parsed (see $EV/serial.log)"
grep -aq 'vpci-modern: cap.*type=isr' "$EV/serial.log" \
  || fail "isr cap never parsed (see $EV/serial.log)"
grep -aq 'vpci-modern: VERSION_1 negotiated' "$EV/serial.log" \
  || fail "FEATURES_OK/VERSION_1 never negotiated (see $EV/serial.log)"

# ---- transitional: caps exist → modern path taken -----------------------
run -device virtio-net-pci > "$EV/serial-trans.log"
grep -aq 'vpci-modern: VERSION_1 negotiated' "$EV/serial-trans.log" \
  || fail "transitional device caps not parsed (see $EV/serial-trans.log)"

# ---- negative: legacy-only → REFUSE ------------------------------------
run -device virtio-net-pci,disable-modern=on > "$EV/serial-legacy.log"
grep -aq 'vpci-modern: REFUSE' "$EV/serial-legacy.log" \
  || fail "legacy-only device not refused (see $EV/serial-legacy.log)"
! grep -aq 'vpci-modern: VERSION_1 negotiated' "$EV/serial-legacy.log" \
  || fail "phantom VERSION_1 on legacy-only device"
! grep -aq 'vpci-modern: cap\[' "$EV/serial-legacy.log" \
  || fail "caps hallucinated on legacy-only device"

echo "accept.sh: PASS — modern caps parsed + VERSION_1 negotiated; legacy-only refused"
