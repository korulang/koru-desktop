# irq-msix — upstream evidence pointer

This slot is `landed` upstream, not here. The proof lives in the pinned
platform repo (`origins.lock`: Cypoe/koru-os @ c7c3474):

- `PLAN.md` "Landed (koru-native arm)" — "8259 PIC + IDT + exception
  delivery; interrupt-driven virtio-blk completion… LAPIC + MSI-X for
  modern devices (vectors 0x30+, EOI to 0xFEE000B0, never the PIC;
  `koru_dev_sleep(kind)` parks a wait on its WaitKind)"; commits
  `c6c0ed4` (ioapic routing) and the MSI-X series.
- `experiments/m1-blk/` — interrupt-driven blk completion measured.
- Unikraft arm: `koru-libs/unikraft/intctlr` shipped lift (catalog
  entry, 2-state ratchets, booted evidence in that repo).

Evidence class carried over: **M**. Per-entry `accept.sh` re-proves
the interrupt path each consumer actually uses.
