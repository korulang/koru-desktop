#!/usr/bin/env bash
# drm-virtio-gpu acceptance oracle.
# Positive: virtio-gpu-pci,disable-legacy=on — the driver negotiates
# VERSION_1, walks the controlq chain (GET_DISPLAY_INFO, CREATE_2D,
# ATTACH_BACKING, SET_SCANOUT, TRANSFER_2D, FLUSH), programs a 640x480
# scanout, and a HOST-side QMP screendump pixel-check confirms the
# framebuffer content (blue left quarter, red second, green right).
# Falsifiable: -vga std (no virtio-gpu) MUST yield no scanout.
set -u
cd "$(dirname "$0")"

IMG=app/build/vgpu_qemu-x86_64
EV=evidence
MON=/tmp/vgpu-accept-mon
mkdir -p "$EV"
fail() { echo "accept.sh: FAIL — $*"; exit 1; }
[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

hmp() { # $1=socket, $2=command
  python3 - "$1" "$2" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.settimeout(4)
time.sleep(0.5)
try: s.recv(65536)
except: pass
s.sendall((sys.argv[2] + "\n").encode())
time.sleep(2)
try: s.recv(65536)
except: pass
s.close()
PY
}

pix() { # $1=ppm — verify painted bands at fixed coords
  python3 - "$1" <<'PY'
import sys
d = open(sys.argv[1],'rb').read()
e = d.index(b'255\n') + 4; px = d[e:]
w,h = map(int, d.split(b'\n')[1].split())
def at(x,y):
    i=(y*w+x)*3; return px[i],px[i+1],px[i+2]
ok = (at(50,200)==(0,0,255) and at(200,200)==(255,0,0)
      and at(500,200)==(0,255,0) and at(50,50)==(0,255,255))
sys.exit(0 if ok else 1)
PY
}

rm -f "$MON" "$EV/scanout.ppm"
# ---- positive ----------------------------------------------------------
timeout 40 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot -no-shutdown -display none \
  -device virtio-gpu-pci,disable-legacy=on,id=vgpu0 \
  -monitor unix:"$MON",server,nowait \
  > "$EV/serial.log" 2>&1 &
for i in $(seq 1 12); do sleep 1; [ -S "$MON" ] && break; done
[ -S "$MON" ] || fail "monitor socket never appeared"
sleep 4
hmp "$MON" "screendump $EV/scanout.ppm vgpu0" >/dev/null
wait || true

grep -aq 'vgpu: virtio-gpu 1af4:' "$EV/serial.log" \
  || fail "virtio-gpu never probed (see $EV/serial.log)"
grep -aq 'vgpu: VERSION_1 negotiated' "$EV/serial.log" \
  || fail "VERSION_1 not negotiated (see $EV/serial.log)"
grep -aq 'vgpu: scanout live' "$EV/serial.log" \
  || fail "scanout chain never completed (see $EV/serial.log)"
grep -aq 'vgpu: flush resp=1100' "$EV/serial.log" \
  || fail "FLUSH not acknowledged (see $EV/serial.log)"
[ -s "$EV/scanout.ppm" ] || fail "screendump missing (no display surface)"
pix "$EV/scanout.ppm" \
  || fail "screendump pixels don't match painted bands (guest lied or scanout absent)"

# ---- negative: -vga std (no virtio-gpu) must yield no scanout ----------
timeout 20 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot -no-shutdown -display none \
  -vga std > "$EV/serial-nodev.log" 2>&1
! grep -aq 'vgpu: scanout live' "$EV/serial-nodev.log" \
  || fail "phantom scanout on -vga std"
grep -aq 'found=0' "$EV/serial-nodev.log" \
  || fail "no-device run didn't report found=0 (see $EV/serial-nodev.log)"

echo "accept.sh: PASS — virtio-gpu scanout 640x480 byte-verified; -vga std yielded none"
