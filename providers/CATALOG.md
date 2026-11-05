# Provider catalog

The slot board. Read first; append your line on landing. Generated
from `providers/*/entry.md` frontmatter — `scripts/board.sh` verifies
consistency; edit entries, not this table by hand.

States: `landed | open | claimed | naive-wrap | idiomatic | waived`
Lanes: `compose | donor-c | silo | koru-lift | native-zig |
upstream-koruos` · Tiers: `qemu | hw`

| slot | iface | arms | lane | needs | tier | status | evidence |
|---|---|---|---|---|---|---|---|
| pci-enumeration | koruos/pci | — | upstream-koruos | — | qemu | **landed** | M |
| irq-msix | koruos/irq | — | upstream-koruos | pci-enumeration | qemu | **landed** | M |
| virtio-pci-modern | koruos/virtio-pci | unikraft/* | donor-c | pci-enumeration | qemu | open | V |
| net-virtio-lwip | koruos/net.tcp | unikraft/linuxu-x86_64 | compose | pci-enumeration | qemu | landed | C |
| blk-nvme | koruos/blk.nvme | unikraft/* | donor-c | pci-enumeration | qemu | open | D |
| drm-virtio-gpu | koruos/kms.drm | unikraft/linuxu-x86_64 | donor-c | pci-enumeration, virtio-pci-modern | qemu | claimed | M |
| gpu-virgl-3d | koruos/kms.gpu3d | unikraft/linuxu-x86_64 | donor-c | drm-virtio-gpu | qemu | open | D |
| usb-xhci | koruos/usb.host | unikraft/* | donor-c | pci-enumeration, irq-msix | qemu | landed | M |
| usb-hid | koruos/input.usb-hid | unikraft/* | donor-c | usb-xhci | qemu | landed | M |
| usb-storage | koruos/blk.usb | unikraft/* | donor-c | usb-xhci | qemu | landed | D |
| audio-hda | koruos/audio.pcm | unikraft/* | donor-c | pci-enumeration | qemu | landed | D |
| acpi-s3 | koruos/power.s3 | unikraft/* | donor-c | — | qemu | open | D |
| net-wifi | koruos/net.wifi | — | donor-c | pci-enumeration | **hw** | open | D |
| bluetooth | koruos/net.bluetooth | — | donor-c | pci-enumeration | **hw** | open | D |

## Unblocked now (all needs landed/waived)

`virtio-pci-modern`, `net-virtio-lwip`, `blk-nvme`, `usb-xhci`,
`audio-hda`, `acpi-s3`, `net-wifi`, `bluetooth`

## Critical path

```
pci-enumeration ─┬─ virtio-pci-modern ─ drm-virtio-gpu ─ gpu-virgl-3d
                 ├─ usb-xhci ─┬─ usb-hid
                 │            └─ usb-storage
                 ├─ net-virtio-lwip        (compose — first-win candidate)
                 ├─ blk-nvme
                 ├─ audio-hda
                 └─ acpi-s3
```

## Growing the catalog

New slot = new `providers/<name>/` dir (copy `_template/`), a row here,
and — if it demands a `koruos/*` iface that doesn't exist upstream — a
provider-row ask filed to koru-os, not invented locally.
| blk-nvme | koruos/blk.nvme | landed | QEMU NVMe driver authored in-repo (probe→BAR0→admin/IO queues→read/write/flush); accept.sh: serial 'korudesktop' identified, host-planted pattern read, guest pattern verified by host dd/cmp, wrong serial refused |
