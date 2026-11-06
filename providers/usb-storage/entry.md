---
name: usb-storage
iface: koruos/blk.usb
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [usb-xhci]
accept: accept.sh — guest reads a host-written pattern off a usb-storage disk
tier: qemu
status: landed
home: this-repo
donor: unikraft-fork
evidence: D
license: BSD-3-Clause (own driver code over unikraft-staging BSD-3 — no copyleft, no blobs)
provenance: xhci.c/xhci.h derived from sibling usb-xhci driver (in-repo, BSD-3, spec-authored); msc.c BOT/SCSI layer written from USB MSC BOT 1.0 + SBC specs; unikraft-staging 3fdffba8 in origins.lock
---

# usb-storage

USB Mass Storage (BOT — Bulk-Only Transport): CBW/CSW command blocks,
INQUIRY/READ10/WRITE10, sense keys. Depends on `usb-xhci`. Gives the
desktop USB sticks and SD-card readers — the "storage beyond virtio"
line on the gap list, at QEMU scale first.

## Acceptance

`accept.sh` attaches a host image as `-device usb-storage,drive=usbstick`
(QEMU's usb-storage device is BOT already), guest driver enumerates the
LUN, reads a host-planted pattern sector, writes a second pattern,
flushes. Host-side `dd`/`cmp` on the backing image at the written LBA —
guest cannot fake host bytes. Optional stronger form: guest formats
ext2 and host `e2fsck -fn` is clean (DoD-1 oracle shape on USB media).

## Notes

- UASP (usb-attached-sCSI) is the modern protocol — QEMU's
  `usb-storage` is BOT; `usb-uas` exists for the faster path. BOT first.
- Real-hw flash quirks (read-only switches, eject semantics, spin-up
  delays) are tier:hw concerns — keep the transport layer honest about
  sense/phase errors so they surface later, not silently.

## Decision record

- **Chosen:** BOT (Bulk-Only Transport) — QEMU `usb-storage` is BOT.
- **Alternative not taken:** UASP (`usb-uas` device) — faster protocol,
  later pass; BOT first because it's what the emulated device speaks.
