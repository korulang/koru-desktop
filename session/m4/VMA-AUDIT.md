# Commission: fix the vma_list corruption blocking Hyprland render (M4)

## Mission

Get Hyprland to produce its first rendered frame on the virtio-gpu
scanout. Everything upstream is proven — this is ONE diagnosed memory bug
between "renderer initialized" and "pixels on screen."

## Read first (ground truth, in order)

1. `session/m4/NOTES.md` — the full diagnosis + evidence pointer
   (`evidence/m4-vma-corruption.raw`). This is the bug report; trust it.
2. `session/DESIGN.md` — the milestone ladder. You are on the M3→M4 rung.
3. `session/README.md` — how sessions/builds/QEMU are run here.

## The bug (from M4, verified by instrument)

`vmem_vma_find` (in `lib/ukvmem`, upstream Unikraft — NOT committed; it is
fetched into `providers/*/app/build/libukvmem/` or `~/.unikraft` at build
time) gained a canonical-address guard that fires on a corrupt node:

    ukvmem: corrupt vma_list node 0xff111111ff1110f1 at find 0x1017b26000-...

- The "node" is a pointer into the faulting render VA region carrying
  `0xff` pixel bytes in its link fields.
- Two candidate causes, both live:
  - **free-without-unlink** — a VMA node freed but left linked; llvmpipe's
    pixel buffers reuse the memory and scribble `0xff` over the links (UAF)
  - **wild write** — llvmpipe writes out of bounds straight into the
    list node's link fields
- The corruption is DURABLE — it poisons every later list walk including
  find(0), so it is list-state corruption, not a one-off bad deref.

## The audit (named path from M4)

Audit `lib/ukvmem/vma.c` unlink/teardown paths for a `free`/`kfree` that
does not `list_del`/`vma_remove` first. Hyprland runs render threads
against the same `vas`, so also weigh concurrent mmap/munmap — if the
writer is a race, the fix is vas-level locking or lock ordering, not a
missing unlink.

## How to localize WHICH operation corrupts it

The instrument already prints the corrupt node address each run. Bisect:
re-run with the `vmem_vma_find` guard after each suspect step —
GBM buffer alloc, mmap/munmap, render submit, buffer destroy — until the
first guard-fire names the syscall. That turns "somewhere in Hyprland"
into "this call."

If the wild-write branch wins: the `0xff` bytes are pixel data, not
allocator metadata — suspect llvmpipe's scanout/color buffer writing past
its VMA length. Compare the buffer size Mesa requests vs the VMA coverage
ukvmem grants.

## Constraints / context

- Build/run recipe is in `session/README.md` and the provider dirs —
  QEMU + the linuxu/unikraft image M4 used. Do NOT re-derive it.
- `gpu-virgl-3d` has NO acceptance oracle yet (only `scratch/`) — if your
  fix path touches 3D, note that gap; the 2D scanout (`drm-virtio-gpu`)
  is the proven surface.
- Evidence discipline: keep the corrupt-node guard; never weaken it to
  green. A guard that only confirms is not a gate.

## Done looks like

Hyprland completes init and presents a frame — QMP screendump shows a
non-black desktop (wlroots background/clock at minimum). Then M4 opens:
a wayland client connects, and M5 (`import oshun`) becomes reachable.

Report: which hypothesis won (unlink vs wild-write vs race), the exact
fix, and the screendump evidence.
