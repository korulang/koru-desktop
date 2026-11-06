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

## Decision record — source-build spike 2026-11-05 (D2 revised)

**Verdict: host-blocked stands — measured, not assumed.** QEMU 11.0.3
source build (pinned aeec49e in origins.lock) gets far on macOS but
VIRGL never reaches the guest.

Measured chain, each step demonstrated:
- `qemu-system-x86_64` BUILDS with `VirGL support: YES 1.3.0` +
  `OpenGL support (epoxy): YES` — required a patched libepoxy 1.5.10
  built from source (brew epoxy ships no `epoxy/egl.h`; epoxy's
  `dispatch_common.h` hardcodes `PLATFORM_HAS_EGL 0` on `__APPLE__`
  and never defines `EGL_LIB`/`.dylib` GLES names — patched at
  /tmp/epoxy-src → /tmp/epoxy-egl).
- `virtio-gpu-gl-pci` DOES appear in `-device help` once OpenGL
  compiles — first half of the unblock test passes.
- Mesa 26.2.6 surfaceless EGL DOES initialize:
  `eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA)` +
  `eglInitialize` → EGL 1.5 Mesa (probe /tmp/eglprobe2).
- QEMU `egl-headless` needed two patches to reach it: a darwin
  surfaceless branch in `egl_init` (no GBM/X11/WGL on macOS →
  `qemu_egl_rn_ctx` was never set) and `EGL_SURFACE_TYPE` relaxed to
  `EGL_DONT_CARE` (WINDOW_BIT can't exist without a window system).
- AFTER all that: device realizes cleanly (no QEMU errors), guest
  sees 1af4:1050, walks 5 virtio caps, negotiates VERSION_1 —
  `devfeat 00000101:30000013`. **Feature bit 35 (`VIRTIO_GPU_F_VIRGL`
  → bank1 0x08) is NOT offered.** Bank1 = VERSION_1 + RING_RESET only.

Root cause hypothesis (unproven): virgl_renderer's in-process init
(vrend_egl) likely fails silently inside QEMU — Mesa's darwin GLES
stack can't satisfy what vrend needs (no rendernode, no real GBM).
The vhost-user-gpu alternative is double-dead: QEMU gates it
`host_os == 'linux'` (meson.build:1411) + requires GBM.

**What unblocks the slot:** a Linux build host (GBM+EGL+virglrenderer
all native there), or virgl_test_server's standalone renderer with a
vhost-user device — which needs the linux-gated QEMU path anyway.
The guest-side work (virtio-gpu driver, virgl cmdq) is unaffected
and still ready when a host appears.

Artifacts: patched epoxy + QEMU sources in providers/gpu-virgl-3d/
scratch/ (qemu @ aeec49e, epoxy @ c84bc94 both pinned in
origins.lock). QEMU patches live uncommitted in the scratch tree.
