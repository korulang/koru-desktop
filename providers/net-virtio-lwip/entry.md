---
name: net-virtio-lwip
iface: koruos/net.tcp
arms: [unikraft/linuxu-x86_64]
donor: none — composition of existing parts
needs: [pci-enumeration]
accept: accept.sh — guest obtains IPv4 on user-net and reaches the host
tier: qemu
status: landed
home: this-repo
donor: none
lane: compose
evidence: C
license: BSD-3-Clause (unikraft, lib-lwip, fork-lwip — no copyleft; no firmware blobs)
provenance: unikraft-staging 3fdffba8 + lib-lwip ec55ae17 + fork-lwip bef26c44 (all in origins.lock); zero derived code — composition of upstream objects only
---

# net-virtio-lwip

TCP/IP networking in a koru-os image: `drivers/virtio/net` (in Unikraft
core) + `uknetdev` (lifted in koru-libs `unikraft/net` — proven sending
and receiving Ethernet frames between two virtio-net devices) +
`lib-lwip` (upstream `unikraft/lib-lwip`, POSIX socket API over lwIP) +
`posix-socket`. **Mostly composition, not authorship** — the candidate
first-win slot: it exercises the whole catalog machinery (pins, image
recipe, accept.sh, evidence, catalog line) on a slot that needs no new
driver code.

## Acceptance

`accept.sh` boots the image under QEMU user-net (`-netdev user`,
optionally `hostfwd`), and the guest: (a) gets a lease or static IPv4,
(b) `ping`s the host gateway `10.0.2.2` or completes a TCP connect to a
hostfwd listener the script runs. Host-side artifacts: serial log with
the guest's own reported address + the ping/connect result, plus the
host listener's accept log. Falsifiable: run with `-nic none` and the
same script must fail.

## Notes

- lib-lwip is an external kraft lib — add to `origins.lock` when pinned
  (revision TBD at implementation time; do not float `staging`).
- koru-libs `unikraft/net` lift already proves the uknetdev layer;
  this slot is the *image composition*, which is desktop-repo work.
- Wi-Fi (`net-wifi`) is a separate tier:hw slot — this one is ethernet
  virtio only.

## Decision record

- **Chosen:** `compose` lane — virtio-net + lib-lwip + posix-socket
  composed into a bootable image; no new driver code.
- **Alternative not taken:** `donor-c` TCP/IP stack authorship —
  measured: composition sufficed.
