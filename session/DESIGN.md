# session — design spike (map, not implementation)

Measured 2026-11-05 on provider/m6-linuxu tree. Every claim below names
its evidence class; nothing here is implemented.

## The question

Which compositor binds first, what the first bootable "desktop-ish"
image contains, and the milestone ladder to an unmodified `import oshun`
app window on the koru-os desktop.

## Compositor candidates — three paths, one first bind

| Path | Status | Verdict |
|------|--------|---------|
| native koru comp.zig | koru-os tree has koru/native/*.zig (dirty/unproven on this Mac — m6 harness linked a unikraft kernel, not koru's own app) | UNMEASURED as desktop; the koru-os build path is `/workspace/m3img`-era (Windows) — reproducing it on Mac is itself a commission |
| Hyprland-on-linuxu | m9-hyprland experiment exists (inputs-debs, hyprland.lua, mkroot.sh, m9-accept-init, evidence/) — Windows-box recipe, never run here | Strongest oracle: unmodified compositor binary, already recipe-shaped; but needs wlroots deps + input path + a DRM fd — every dep proven piecemeal (linuxu YES, card0 YES, keyboard/mouse NO) |
| nested wlroots | koru-os-cypoe `native: nested wlroots rung` committed at ef57dff — a real wlroots compositor nested inside the host session, proven upstream | PROVEN at C code level, but "nested on the seam" is host-hosted, not the koru-os desktop |

**First bind call: Hyprland-on-linuxu.** Reasoning: the frozen-oracle
discipline this program runs on — an unmodified upstream binary is the
strongest possible acceptance, and modetest-on-card0 proved the exact
pipeline it needs (dynamic ELF load → libdrm → DRM ioctls → painted
scanout). comp.zig's desktop is unmeasured; nested wlroots is a dev
instrument, not the product. Cost: the wlroots+Hyprland dep tree in the
initrd is the largest rootfs yet (M9's mkroot.sh shows ~15 DEBs).

Fallback reading if Hyprland's dep tree fights: a minimal wlroots
compositor (wayland-scanner + tiny compositor like the m9 nested rung
proven upstream) — still unmodified libs, smaller surface.

## First desktop-ish image (image contract)

Same linuxu recipe as m6-image (unikraft 0.21 + app-elfloader + korukms
+ initrd rootfs), extended:

- `/dev/dri/card0` — LANDED (drm-virtio-gpu, modetest-verified)
- input devices — the gap that matters most: Hyprland needs libinput
  → evdev nodes `/dev/input/event*` ← `usb-hid` provider (status in
  CATALOG: check before claiming — last board read: usb-hid present,
  xhci landed ec1a13e, usb-storage landed). Ladder: hid mouse+kbd events
  readable via evdev = the M-gate.
- `/dev/fb0` or DRM dumb buffers + mmap — mmap device hook already
  patched and proven (M6 patchset applied in drm-join)
- wayland socket dir `/run/user/0` + seatd or libseat-builtin (session
  seat — probably seatd-less `WLR_LIBSEAT` env or direct /dev/dri
  ownership — m9 notes will say)
- libs: libwayland-server, wlroots 0.16.x (m9 pinned version), Hyprland
  binary, libinput, libdrm (LANDED), libxkbcommon, pixman, seatd?
  — exact list: m9-hyprland/inputs-debs.txt is the reference manifest
- optional audio-out: hda provider LANDED (79125ae) — not needed for
  first window; keep out of the critical path
- net (lwip) landed — only needed for ssh/fetch inside guest; optional

## Milestone ladder

M0 (done): modetest on card0 — painted modeset host-verified.
M1: `/dev/input/event*` present + events flowing (evtest-class binary
    from Debian, serial shows key/mouse events on QEMU input)
M2: `libinput debug-events` or equivalent reads devices — proves the
    evdev→libinput seam Hyprland consumes
M3: Hyprland binary LAUNCHES on card0 — prints its DRM/GBM init to
    serial (even if it exits after) — measures the dep tree, not yet
    rendering. Falsifiable: missing dep = ld.so error, visible
M4: a wayland client (foot/wlterm or weston-terminal-class, unmodified)
    runs under it; screendump shows a non-black frame ≠ the SMPTE bars
M5 (the ladder's top): `import oshun` — koru's own example app compiled
    to a wayland/GL surface, window visible on the Hyprland desktop in
    the screendump. (oshun's runtime shape decides wayland-native vs
    XWayland — measure which binary oshun produces first)

## Honest gap list

1. **Input is the critical path, not graphics.** card0 is landed; there
   is no evdev path proven yet. usb-hid exists as a slot; its events→
   /dev/input seam is unmeasured. Expect the first failure here.
2. **wlroots needs pixman+GBM.** GBM is Mesa's libgbm — Debian deb,
   dynamic loads proven; the gbm_create/alloc path against our DRM
   ioctls (DUMB_CREATE/MAP offsets) is where refcounting bugs will live.
   The join's mmap-device-hook was built exactly for this.
3. **seat/session mgmt**: libseat needs logind or seatd or direct fd
   ownership — none exists; seatd must be in the initrd or wlroots must
   fall back to drm_fd ownership — m9 recipe chose one, check.
4. **virgl is absent** (measured wall, deferred by ruling) — all GL is
   swrast via llvmpipe (mesa deb, large but proven class). Hyprland on
   llvmpipe is slow-but-real.
5. **koru-os comp.zig alternative unmeasured** — the Windows build path
   wasn't reproduced; koru/native/*.zig edits are uncommitted elsewhere.
6. **oshun's runtime shape unknown** — which backend (wayland vs fb vs
   x11) decides M5's shape; measure the binary first.

## Evidence classes for the ladder

- serial logs (M1-M3): event flow, ld.so resolves, compositor init
- QMP screendumps (M4-M5): non-black, pattern-diff over time
- binary checksums in origins.lock (all debs, per diligence clause)
- refusal-log lines: ENOSYS/REFUSED syscalls Hyprland hits — the honest
  gap list *grows* here, it's the whole point of the unikraft posix shim
