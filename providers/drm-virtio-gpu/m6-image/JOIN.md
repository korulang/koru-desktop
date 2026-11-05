# drm-m2-join — korukms + m6 rootfs + virtio-gpu, this Mac

VERDICT: GREEN — unmodified Debian modetest opens /dev/dri/card0,
enumerates modes, and paints a real modeset; host screendump shows the
pixels. Workdir /tmp/drm-join (scratch), image = unikraft 0.21 +
app-elfloader + lib/korukms@{korukms-virtio-gpu 89091d5} + m6 rootfs
(initrd-embedded, modetest+glibc2.41+libdrm2.4.124 per origins.lock).

## What serial shows (evidence/m2-join-*.raw)

- `koru-os: kms: virtio-gpu 00:03.0 VRAM 16 MiB at phys 0xffce000, 1 modes`
- `koru-os: kms rows bound: /dev/dri/card0 -> virtio-gpu`
- `trying to open device '/dev/dri/card0'... done`
- `opened device 'koru-os kms personality' on driver 'korukms'`
- Connector `Virtual-1` (id 34) connected, mode `640x480` enumerated
- `modetest -s 34:640x480`: `setting mode 640x480-211.59Hz on connectors
  34, crtc 32` — modeset went through the provider's scanout()
- screendump: 18432 non-black pixels, gray pattern bar at row 0

## Honest deltas from the Windows-box m6-kms run

1. `LIBVIRTIO_PCI` (and the whole libvirtio stack) must NOT be in the
   config — the virtio bus claims 1af4:1050 before korukms's pci_driver.
   The provider does manual virtio over raw PCI, same as the M1 harness.
2. korusysfs_mount needs /sys to exist in the rootfs — embedded-initrd
   extraction doesn't leave an empty /sys, so the workspace copy does
   stat+mkdir before mount. Upstream option: mkmp ukopt or a /sys entry.
3. The card name string prints "provider bochs-display" — cosmetic name
   field; the bound provider is virtio-gpu (the bind line proves it).
4. Patches required (all upstream, all applied clean on 0.21): m3 set
   (vfork-fsbase, virtio-ring-destroy, elfloader-empty-arg, sigchld-wake,
   clock-nanosleep, enosys-log, refusal-log) + M6 set (devfs-subdir-rdev,
   posix-mmap-device-hook) + M7 devfs-poll-nonblock + posix-poll-legacy.
5. One refusal, expected: DRM_IOCTL_MODE_SETGAMMA — no gamma hw; the
   refusal log prints it loudly, modeset still completed.
