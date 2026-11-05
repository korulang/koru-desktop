# session M2 — unmodified libinput on /dev/input/event0 (DESIGN.md ladder)

VERDICT: GREEN — `libinput debug-events` (Debian libinput-tools 1.31.3,
unmodified) opens /dev/input/event0, reports DEVICE_ADDED for the
koru-os i8042 keyboard (cap:k, seat0), and prints KEYBOARD_KEY events
for host-injected keys.

## Evidence (evidence/)

- m2-libinput-debug-events.raw: DEVICE_ADDED + KEYBOARD_KEY
  press/release pairs per sendkey (a, b, ret)
- m2-libinput-show-keycodes.raw: same run with --show-keycodes
- m2-libinput-no-device.raw: `--device /dev/input/event9` ->
  `libinput error: client bug: Invalid path` + `Failed to initialize
  device` — loud refusal, falsifiable direction proven

## The udev-less path (per DESIGN.md's own note)

libinput needs libudev for sysfs device metadata. No udevd exists —
solved with **libudev-zero** (ISC, illiliti/libudev-zero) at commit
8f1d3d0624496f1c4967f18d6afaddbd552d8bce + the upstream m9
pipe-monitor patch, cross-compiled for x86_64-linux-gnu on this Mac
with `zig cc -target` (brew zig 0.15.2), LIBUDEV_183 version-script so
Debian binaries bind cleanly. It reads /sys uevent data — korusysfs
already populates 39 sysfs entries including the input device view.

## Dep chain actually required (each missing = loud ld.so refusal, fixed in order)

libinput.so.10 -> libevdev.so.2, libmtdev.so.1, libudev.so.1,
libwacom.so.9 -> libgudev-1.0.so.0 -> libgobject/glib/gio ->
libffi.so.8, libpcre2-8-0, libatomic.so.1, libpthread.so.0.
libwacom9 must be >=2.18 (LIBWACOM_2.15/2.18 versioned symbols) —
2.20.0 shipped; 2.14 fails with a version-script refusal.

## origins pins (sha256, ftp.debian.org trixie)

libinput10_1.31.3-1 0e2628eec17c8cd54470718afad92976678767231dfcfefb6f012503bf58321a
libinput-tools_1.31.3-1 ef0787e4279cdbf3694cc0a36089bb7be5097e3a8a9214b93c473b717284931e
libevdev2_1.14.0+dfsg-1 7e0547e08aa3be04b9356cba9d4271c0c84d6f6647d00977f0fffc497e9f1710
libmtdev1t64_1.1.7-1+b2 65c8542b22873aeec6167d1e4cf3227d7eacf85b8395cbaf0b762ab716a986f1
libwacom9_2.20.0-1 20542df020f98ddf199702b8a93ca922e0b28406af7f945cda9c188dca4bb1c3
libgudev-1.0-0_238-7+b2 4645a8d63693aba2b16b2d8989f9bf9a245a9cc5c499e5e933dd1b78f54cb197
libglib2.0-0t64_2.84.4-3~deb13u5 e2baf92c57d1db1753e5781a61450d9a01e7833c7bd2dfe87776a6811c11ecc2
libffi8_3.8.0-2 8f52979b2f3831088c419aae858f10fd3ad7e42cc6c1d28967bae996a98223a4
libpcre2-8-0_10.46-1~deb13u3 e226f661d918f04daf38cdbc4806b7ed7d6ef95c7eb0ade692fc350e31970040
libatomic1_14.2.0-19 212b399aae2f7299203d261a57e49372e09565a9a5ea971905f94a3960366c05
evtest_1.36-1 0df760a6fb43a9ffb2e6d1d2774abdef9cc7e7cb73dc794dac2362976e8fadaa
libudev-zero @ 8f1d3d0624496f1c4967f18d6afaddbd552d8bce + m9 pipe-monitor patch (koru-os-cypoe)

## Honest deltas / gaps

- `/usr/share/libinput` quirks data is absent — libinput warns loudly
  (`failed to find data files`) and still works; device classification
  (cap:k) is correct via libudev-zero's uevent bitmaps. Quirks affect
  exotic-device corrections, not the keyboard path.
- keycode names print as `*** (-1)` without --show-keycodes — cosmetic;
  the event stream is complete (press/release per key).
