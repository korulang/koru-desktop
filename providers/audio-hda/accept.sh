#!/usr/bin/env bash
# audio-hda acceptance oracle.
# Positive: guest probes intel-hda, identifies codec via CORB/RIRB verbs,
# plays a 500 Hz ±20000 square wave over the output stream; QEMU's wav
# audiodev writes it to a host file — verify real PCM, not just "opened".
# Negative: no codec attached MUST fail.
set -u
cd "$(dirname "$0")"
IMG=app/build/audiohda_qemu-x86_64
EV=evidence
fail() { echo "accept.sh: FAIL — $*"; exit 1; }
mkdir -p "$EV"
[ -f "$IMG" ] || fail "image $IMG missing — build the provider app first"

# ---- positive -----------------------------------------------------------
rm -f "$EV/cap.wav"
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot \
  -device intel-hda \
  -audiodev wav,id=snd0,path="$EV/cap.wav" \
  -device hda-output,audiodev=snd0 \
  > "$EV/serial.log" 2>&1

grep -aq 'hda: codec[0-9]* vid=1af4' "$EV/serial.log" \
  || fail "codec not identified on serial"
grep -aq 'hda: stream run' "$EV/serial.log" \
  || fail "stream never programmed"
grep -aq 'hda: lpib=[1-9]' "$EV/serial.log" \
  || fail "LPIB never advanced — DMA did not consume"
python3 - "$EV/cap.wav" <<'PYEOF' || fail "wav has no real PCM signal"
import struct, sys
d = open(sys.argv[1], 'rb').read()[44:]
s = struct.unpack('<%dh' % (len(d)//2), d[:len(d)//2*2])
nz = sum(1 for x in s if x)
peak = max((abs(x) for x in s), default=0)
sys.exit(0 if (nz > 1000 and peak > 10000) else 1)
PYEOF

# ---- negative: no codec --------------------------------------------------
timeout 60 qemu-system-x86_64 -cpu max \
  -kernel "$IMG" -nographic -no-reboot -device intel-hda \
  > "$EV/serial-nocodec.log" 2>&1
if grep -aq 'hda: codec[0-9]* vid=\|hda: stream run' "$EV/serial-nocodec.log"; then
  fail "guest played with no codec attached — oracle broken"
fi
grep -aq 'no codec\|probe hit=1' "$EV/serial-nocodec.log" \
  || fail "negative run produced no refusal evidence"

echo "accept.sh: PASS — codec vid=1af4 on serial, ±20000 PCM in host wav, no-codec refused"
exit 0
