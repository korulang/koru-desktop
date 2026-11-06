---
name: fs-9p
iface: koruos/fs.9p
arms: [unikraft/*]
lane: compose
donor: none — composition if vfscore 9p mount exists; minimal glue otherwise
needs: [pci-enumeration]
accept: accept.sh — guest mounts host share, reads+writes host-verified files
tier: qemu
status: open
home: this-repo
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# fs-9p

9P2000 host file sharing: virtio-9p device (`drivers/virtio/ninep`
in Unikraft core) + a mount path exposing a host directory inside
the guest. The desktop demand: drag-drop equivalent, dev-file access,
and later the omni bridge's shared plane needs *some* host channel.

## Acceptance

`accept.sh` boots with `-fsdev local,path=$SHARE,security_model=none,
id=fsdev0 -device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare`
(add `disable-legacy=on` if the modern path is exercised). Guest
mounts `hostshare`, reads a host-planted file byte-identical, writes
a guest file, host verifies content on the share dir. Host-side
oracle: file bytes on the host filesystem — guest cannot fake them.
Falsifiable: wrong mount_tag / absent fsdev → mount fails → script fails.

## Notes

- **Unmeasured:** whether vfscore/posix-fs in Unikraft 0.21 exposes
  a 9p mount path. First task at claim: grep the posix-fs/vfscore
  layer for 9p/virtio-9p mount support — if absent, the slot needs a
  minimal `donor-c` glue layer and `lane:` gets revised in place.
- `virtio-fs` (FUSE-over-virtio) is the modern alternative — QEMU
  supports it; evaluate at claim time, 9p first because the driver
  exists in-tree.
- Security model `none` is correct for QEMU user-net dev loops;
  `mapped-xattr`/`none` decision recorded at claim.
