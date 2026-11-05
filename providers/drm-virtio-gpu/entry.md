---
name: drm-virtio-gpu
iface: koruos/kms.drm
arms: [unikraft/linuxu-x86_64]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration, virtio-pci-modern]
accept: accept.sh — unmodified libdrm/modetest or kmscube on /dev/dri/card0
tier: qemu
status: landed
home: this-repo
donor: unikraft-fork
evidence: M
license: BSD-3-Clause (app/ is original work)
provenance: original work against virtio 1.0 + virtio-gpu specs; transport/control path derived from same-author virtio-pci-modern probe (BSD-3-Clause); no foreign lifts
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

`accept.sh` (current): boots qemu/x86_64 + `virtio-gpu-pci,disable-legacy=on`,
driver negotiates VERSION_1 + runs the controlq chain through
TRANSFER_2D/FLUSH, programs a 640x480 scanout; host-side `screendump vgpu0`
pixel-check verifies painted bands (blue/red/green at fixed coords) —
byte-verified, never self-report. Falsifiable: `-vga std` yields `found=0`
+ no scanout. PASSES both directions.

Frozen-oracle wall (honest): `modetest`/`kmscube` on `/dev/dri/card0`
needs the linuxu arm + a DRM ioctl personality (korukms port) — the
virtio-gpu device path itself is proven; the ioctl surface is the
remaining upstream piece (home: upstream-koruos per Notes).

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
