# pci-enumeration — upstream evidence pointer

This slot is `landed` upstream, not here. The proof lives in the pinned
platform repo (`origins.lock`: Cypoe/koru-os @ c7c3474):

- `PLAN.md` "Landed (koru-native arm)" — "PCI enum + virtio-gpu LANDED
  (pci.zig enumerate-once table)"; commit `527401d` "drivers look
  hardware up, they don't scan it — pci enum is substrate".
- `experiments/m1-blk/` — virtio-blk over the enumerated PCI tree,
  3/3 TCG runs, host `cmp` verified.
- Unikraft arm: `ukbus`/pci in `donors/unikraft-staging` core tree.

Evidence class carried over: **M** (measured upstream under TCG).
Re-verification on this repo's images is each consumer entry's job —
their `accept.sh` re-proves the slice they depend on.
