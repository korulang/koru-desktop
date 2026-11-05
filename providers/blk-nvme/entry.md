---
name: blk-nvme
iface: koruos/blk.nvme
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration]
accept: accept.sh — guest reads/writes the NVMe namespace; host verifies
tier: qemu
status: open
home: this-repo
donor: unikraft-fork
evidence: D
license: none-yet
provenance: none-yet — record source+SHA+license before first derived line
---

# blk-nvme

NVMe block storage. Unikraft core has virtio-blk only — no NVMe driver
exists in-tree or in the standard lib-* catalog (verified against
`donors/unikraft-staging` lib/ listing, 89 entries). QEMU provides
`-device nvme` with a raw-or-image namespace, so this is a real-driver
authorship slot, not composition.

## Acceptance

`accept.sh` boots with `-device nvme,serial=korudesktop,drive=nvme0`
backed by a host image file containing a known pattern at a known LBA.
Guest driver identifies the namespace (serial `korudesktop`), reads the
pattern sector, writes a second pattern, flushes. Host-side: `dd`/`cmp`
the image file at the written LBA — the guest cannot fake host bytes.
Optional stronger form: guest makes an ext2 filesystem on the namespace
and host `e2fsck -fn` + `debugfs` read-back (the DoD-1 oracle shape).

## Notes

NVMe is native PCIe — no virtio transport needed, but it does need
MSI-X vectors (`irq-msix` landed) and PRP/SGL command rings. Real-hw
NVMe drives are a different firmware/queue-depth world; `tier:` stays
qemu until a hw rung is designed.
