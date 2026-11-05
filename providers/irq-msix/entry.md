---
name: irq-msix
iface: koruos/irq
arms: [unikraft/*, koru-native]
donor: none
needs: [pci-enumeration]
accept: none — landed substrate; evidence is upstream
qemu: <platform default>
tier: qemu
status: landed
home: upstream-koruos
lane: upstream-koruos
evidence: M
license: none (no code in this repo — upstream evidence cited)
provenance: upstream: Cypoe/koru-os @ c7c3474 + koru-libs/unikraft/intctlr — see evidence/UPSTREAM.md
---

# irq-msix

Interrupt delivery to drivers: 8259 PIC, IDT, LAPIC, MSI-X on modern
devices. **Landed upstream:** koru-os native carries PIC + IDT +
exception delivery, LAPIC + MSI-X (vectors 0x30+, EOI to 0xFEE000B0),
IOAPIC routing, and `koru_dev_sleep(kind)` wait-kind parking
(PLAN.md "Landed", commits `c6c0ed4`, `ioapic:` series). The
`unikraft/intctlr` lift in koru-libs covers the Unikraft-arm side
(shipped catalog, 2-state ratchets).

## Acceptance

N/A — substrate entry. Modern virtio devices need MSI-X; legacy devices
need IOAPIC/PIC routing. Both are claimed landed upstream; per-entry
`accept.sh` re-proves the slice it depends on.
