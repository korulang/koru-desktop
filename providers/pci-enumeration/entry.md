---
name: pci-enumeration
iface: koruos/pci
arms: [unikraft/*, koru-native]
donor: none
needs: []
accept: none — landed substrate; evidence is upstream
qemu: -device <platform default>
tier: qemu
status: landed
home: upstream-koruos
lane: upstream-koruos
evidence: M
license: none (no code in this repo — upstream evidence cited)
provenance: upstream: Cypoe/koru-os @ c7c3474 — see evidence/UPSTREAM.md
---

# pci-enumeration

PCI configuration-space enumeration — the substrate every `donor-c`
slot stands on. **Landed upstream, not ours:** Unikraft core has
`ukbus`/pci in-tree; koru-os's native arm has `koru/native/pci.zig`
("drivers look hardware up, they don't scan it" — enumerate-once table,
commit `527401d` in platform/koru-os).

## Acceptance

N/A — this entry exists so `needs:` edges resolve. Evidence pointer:
platform/koru-os PLAN.md "Landed" section and `experiments/m1-blk`.
