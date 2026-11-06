---
name: gpu-virgl-3d
iface: koruos/kms.gpu3d
arms: [unikraft/linuxu-x86_64]
lane: donor-c
donor: unikraft-fork
needs: [drm-virtio-gpu]
accept: accept.sh — GL app renders via virgl; screendump differs over time
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# gpu-virgl-3d

Hardware-accelerated 3D over virtio-gpu virgl/venus: `-device
virtio-gpu-pci,virgl=on` (requires host virglrenderer + host GL). This is
what makes Hyprland actually smooth and what Qt Quick/QML (DoD-4's
exemplar) wants — swrast works but is not the desktop experience.

## Acceptance

`accept.sh` runs an unmodified GL app (glxgears-class or kmscube with a
gl renderer) on the virgl-backed card; two QMP screendumps 1s apart
differ and are non-black — the M8b oracle shape. Falsifiable: same
script with `virgl=off` must fall back or fail, never silently pass.

## Notes

Deferred by design: depends on `drm-virtio-gpu` (2D) first, and on
host-side virglrenderer availability — a build-host dependency worth
pinning in `origins.lock` notes when claimed. Venus (Vulkan) is a later
slot of its own if ever demanded; not seeded — the catalog grows on
demand (SCENE law 5).

## Decision record

- **Chosen:** QEMU source-build spike — `--enable-virglrenderer` against
  the brew-installed virglrenderer 1.3.0 (no time budget constraint;
  push as far as this host allows).
- **Alternative not taken:** host-blocked defer to a Linux build host —
  remains the fallback if the source build can't produce a working
  virgl consumer on macOS (EGL/surfaceless is the likely wall).
