---
name: drm-virtio-gpu
iface: koruos/kms.drm
arms: [unikraft/linuxu-x86_64]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration, virtio-pci-modern]
accept: accept.sh — unmodified libdrm/modetest or kmscube on /dev/dri/card0
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# drm-virtio-gpu

A DRM/KMS device backed by virtio-gpu, so unmodified Linux graphics
stack (libdrm, Mesa `kms_swrast`/`virtio_gpu` kernel-side, kmscube,
Hyprland's DRM needs) sees a real card. **This is the biggest single
slot in the catalog** — it needs `virtio-pci-modern` first, then a
virtio-gpu driver in the donor, then a DRM personality or native KMS
provider exposing dumb buffers + CRTCs + fences the way koru-os's
`korukms` already does for bochs-display (platform/koru-os `lib/korukms`,
B18 — that provider is the reference implementation for the ioctl
surface).

## Acceptance

`accept.sh` boots the linuxu arm with `-device virtio-gpu-pci` and runs
unmodified Debian `modetest -M <driver> -s Virtual-1:<mode>` and/or
`kmscube`; host-side QMP `screendump` pixel checks at known coordinates
(the M6/M8 oracle shape — byte-verified, never self-report). Falsifiable:
the same script against `-vga std` must fail to find the driver.

## Notes

- B13 consequence: without `virtio-pci-modern` this slot cannot start —
  `virtio-gpu-pci` is a virtio-1.0 device.
- Scoping choice to make at claim time: 2D-only (dumb buffers, no
  virgl) is the honest first pass; `gpu-virgl-3d` is the separate slot.
- Alternative: reuse/extend `korukms` — it already speaks the DRM
  personality over bochs-display; a virtio-gpu scanout engine could
  plug the same ioctl surface. `home:` may end up `upstream-koruos`.
