# Commission: port the proven m9-hyprland boot to this Mac

## Mission

Reproduce the Hyprland desktop boot that already works on Cypoe's box —
this time on the Mac under QEMU. The recipe donor is complete and proven;
your job is to run it here and iterate to a rendered desktop.

## The donor recipe (read these FIRST, in koru-os-cypoe)

`~/src/koru-os-cypoe/experiments/m9-hyprland/` — a working, screenshotted
Hyprland boot:

- `inputs-debs.txt` — 219 Debian forky amd64 .deb filenames (the
  Hyprland+foot+seatd+libinput+mesa closure)
- `fetch.sh` — downloads them
- `mkroot.sh` — assembles the userland tree: binaries, ld.so-resolved
  library closure, fonts/xkb/glvnd data, writes `etc/m9-hyprland.lua`
- `run-m9.sh` — builds ext2 image (rootfs + busybox + `/etc/m9-init`),
  boots QEMU (`-kernel`, bochs-display, virtio-blk ×2, virtio-input),
  drives it via `drive-m9.py` QMP, screendumps to `shot.ppm`
- `hyprland.lua`, `check.py`, `refused-by-design.txt`, `evidence/` —
  config, acceptance check, known-refusals, prior run logs
- Proof it works: `~/src/koru-os-cypoe/evidence/show/m9-first-frame.png`
  and `m9-foot-window.png` — Hyprland's first frame + a foot window.

## Context you need

- This is the "linuxu" arm — Unikraft `linuxu-x86_64` running Debian
  userland, NOT the native ThinkPad path and NOT a different stack.
- `~/src/unikraft` exists locally and already carries
  `patches/unikraft-0.21-ukvmem-vma-lock.patch` (a parallel session is
  validating that race fix — do NOT touch `~/src/unikraft` source or the
  patch; build against whatever kernel image the recipe produces).
- koru-desktop's own Hyprland attempt (session/m3,m4) used a DIFFERENT
  substrate (app-elfloader + initrd) and died on a `vma_list` race. The
  m9 recipe is the proven alternative — treat it as the reference, not
  something to merge into m4 yet.
- Work in a PERSISTENT dir — NEVER `/tmp` (the last image died there).
  Suggest `~/src/koru-desktop/session/m5-m9port/` or a `workspaces/` dir.

## How to run it

1. Read the four recipe files end-to-end before running anything.
2. `fetch.sh` the debs (network — 219 packages, forky amd64).
3. You need the two extra inputs `mkroot.sh` takes: a forky *stage*
   tree (the unpacked debs) and a `libudev-zero` .so — check `fetch.sh`
   and the m9 NOTES/`refused-by-design.txt` for how those are produced.
4. `mkroot.sh <stage> <libudev-zero.so> <dst>` → userland tree.
5. `run-m9.sh <kernel> <busybox> <root> <init> <outdir>` — you need a
   kernel ELF and a static busybox; the m6-image work
   (`providers/drm-virtio-gpu/m6-image/`) shows how the linuxu kernel +
   busybox are produced on this machine. Reuse that, don't reinvent.
6. Iterate on failures — serial.log is your oracle; `shot.ppm` is the
   win. A non-black screendump with Hyprland's desktop = done.

## Watch for

- The `vma_list` race may or may not fire on this path — if Hyprland
  crashes with a corrupted-vma signature, that's the SAME bug the other
  session is fixing; capture the serial and report it, don't patch
  around it.
- `refused-by-design.txt` lists expected refusals — a refusal NOT in
  that file is a real signal.
- macOS QEMU vs the Windows-box original: paths, `timeout`, `realpath`,
  mke2fs availability may differ — fix the scripts, don't assume.

## Done looks like

`shot.ppm` shows a rendered Hyprland desktop (wallpaper/clock/panel at
minimum, ideally the foot window from the reference screenshots). Report
the working command sequence + any Mac-specific deltas, so the recipe is
reproducible.
