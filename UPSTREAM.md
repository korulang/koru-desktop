# UPSTREAM.md — notes for Cypoe (koru-os)

*Draft for Lars to send however you two talk. Repo is public:
github.com/korulang/koru-desktop.*

## What this repo is

The host-platform layer between koru-os and oshun/omni: bootable
images, session composition, and a provider catalog — device slots each
bound to a `koruos/*` row or refused loudly. Arm-agnostic by design;
Unikraft is donor #1. Nothing here redefines your contract — provider
asks flow up as catalog demands, contract changes are yours.

## State (2026-11-05)

16 catalog slots: **12 landed** (pci-enumeration, irq-msix,
net-virtio-lwip, blk-nvme[V], usb-xhci, usb-hid, usb-storage,
audio-hda, virtio-pci-modern, virtio-input[V], fs-9p[V],
drm-virtio-gpu[M]) + acpi-s3/gpu-virgl-3d/net-wifi/bluetooth open
(hw pair parked for real hardware). All QEMU-verified on the Mac;
`accept.sh` oracles must be able to say no; `board.sh` enforces
evidence-or-it-didn't-happen; provenance clause is board-enforced.

## The offer: `korukms-virtio-gpu` @ `89091d5`

Branch in worktree `~/src/koru-os-korukms`, parented on your pin
`c7c3474`. virtio-gpu scanout provider binding `kms.*` rows — M1 pass,
host screendump oracle green (bind→paint→scanout→640x480 bands).
Two real QEMU-model fixes it drove: ATTACH_BACKING must emit
per-phys-run entries; dma_map refuses ISA-hole-spanning pools.
`drm_linux.c` untouched, bochs seam preserved. M2 (`modetest` on
`/dev/dri/card0`) is now feasible — the linuxu image builds on the
Mac (m6-image landed: elfloader + Debian userland, no Windows box).

## Overlap flag

`blk-nvme` and `virtio-pci-modern` are your named roadmap items
(arm-gap-enumeration "virtio first, then NVMe/AHCI"; B13 modern
virtio-pci) — landed donor-side in our catalog; upstreamable if you
want them in your arm.

## Questions for you

1. Our `koruos/*` iface names are catalog demands, not your contract
   rows — do any collide with your taxonomy?
2. virtio-gpu provider — PR when M2 lands, or hold until your native
   compositor rung decides?
3. `native/thinkpad-bringup` in your checkout is preserved WIP from
   another session — not ours, quarantined off `korukms-virtio-gpu`.
   `fix-signal-deliver-terminate` @ `3895785` is a separate clean
   branch (undeliverable-fault termination fix).

## Provenance

`origins.lock` pins everything consumed; every derived file names
source+SHA+license; copyleft named before code lands (NONOS xhci
AGPL-3.0+ excluded per clause).
