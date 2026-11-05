# session M1 — evdev events flowing (per DESIGN.md ladder)

VERDICT: GREEN — unmodified Debian evtest 1.36 reads /dev/input/event0
on the m6 linuxu image; QMP sendkey produces real EV_KEY + MSC_SCAN +
SYN_REPORT frames on serial.

## The seam

koru-os-cypoe lib/koruinput (evdev_linux.c + ps2kbd.c, upstream M7
personality) compiled into the m6 linuxu image = unikraft 0.21 +
app-elfloader + korukms + koruinput + 11 unikraft patches. i440fx
PS/2 is the device — no virtio-input needed.

## Evidence (evidence/)

- m1-evtest-keys.raw: `input rows bound: /dev/input/event0 -> koru-os
  i8042 keyboard (set 1)`; evtest capability dump (EV_KEY/EV_MSC/EV_LED/
  EV_REP, full keycode table); sendkey a/b/spc/ret ->
  `Event: type 1 (EV_KEY), code 30 (KEY_A), value 1` etc with scancode
  MSC_SCAN frames and SYN_REPORT — press AND release for each key.
- m1-evtest-nonexistent.raw: `evtest /dev/input/event9` -> `evtest: No
  such file or directory` — falsifiable refusal, not silent pass.

## Measured deltas vs Windows-box m7 run

- evtest needs the device on argv (-append 'evtest /dev/input/event0') —
  interactive select waits on stdin forever.
- HMP monitor (unix sock) accepts `sendkey`, NOT QMP `input-send-event` —
  the earlier silent-injection debugging cost a cycle.
- -nographic swallows the monitor socket; use -display none like the
  drm-join.
- EINITRD_PATH is baked into the build config with the ORIGINAL workdir
  path (/tmp/m6-linuxu/...) — copying the workdir requires retargeting
  that path or the embedded cpio goes stale silently.
- evtest 1.36 deb: /debian/pool/main/e/evtest/evtest_1.36-1_amd64.deb
  (no libevdev dep — static-linked deps inside).
- Both providers coexist: without -device virtio-gpu-pci, bochs wins
  /dev/dri/card0 (bind order), vgpu reports the absence honestly.
