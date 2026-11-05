# SCENE.md — koru-desktop

Read-many, write-rarely. Slow clock; the fast clock is
`.claude/skills/desktop-toolchain/` (planted by `arbiters-init`).

## North star

A Koru desktop on koru-os arms: a bootable image where `import oshun`
apps render real windows, input is real, storage is real, and every
device the desktop touches is a provider row — bound with evidence or
refused loudly. Unikraft is the first arm because it is the only arm
that runs unmodified Linux userland today; `koru-native` is where the
architecture wants to end up.

## The laws

1. **Evidence or it didn't happen.** Every provider claim carries an
   `accept.sh` that can say no and artifacts a hostile reader can rerun.
   Self-report is not evidence; screendumps, serial logs, and host-side
   checks are.
2. **Rows are the contract.** A provider earns its place by binding a
   `koruos/*` iface on a named arm — not by existing. `realize matrix`
   is the scoreboard; `refused:` lines are the honest gap catalog.
3. **QEMU device models are the hardware.** Acceptance runs against
   named QEMU devices (`-device qemu-xhci`, `nvme`, `hda-duplex`…).
   Real hardware is `tier: hw` — a later rung with its own acceptance,
   never a silent upgrade of a QEMU claim.
4. **Donors are donors.** Unikraft, Linux, KolibriOS supply drivers and
   ABIs; koru-os owns the seams they plug into. Nothing here IS the OS —
   it is the composition that proves the OS contract can carry a desktop.
5. **Derived, never authored.** The catalog enumerates what the desktop
   demands; a slot that isn't demanded doesn't get built. Grok's gap
   list started this catalog — the realize matrix maintains it.

## The layer map

```
omni        environment charter — custody plane, rafts, agents
oshun       host surface — markup→solver→render; host verbs, gated parts
desktop     THIS REPO — host platform: images, session, provider catalog
koru-os     platform contract — arms, rows, realize, milestones (Cypoe)
koru/koru-libs  toolchain + packages (incl. unikraft/* lifts)
unikraft    donor kernel(s)
```

Dependency arrows point down: desktop drives koru-os (provider asks
flow through realize rows); oshun's `host/index.<platform>.kz` part
lands in oshun once this repo makes the seams bootable; omni rides on
top as rafts and apps.

## Loose directions (beds, not blueprints)

- **First win is composition, not authorship** — `net-virtio-lwip` is
  mostly wiring existing parts (virtio-net + lib-lwip + posix-socket)
  into a bootable image. Prove the catalog machinery on a slot that
  needs no new driver code.
- **Driver silos are the long game** — USB/GPU/audio/NVMe/Wi-Fi are
  `donor-c` slots today; the `silo` lane (Linux/KolibriOS drivers
  behind provider rows) is the escape hatch when authorship stalls.
- **The acceptance consumer is an oshun app** — the desktop is done
  proving itself when an unmodified `import oshun` window runs on it.

## What it says no to

- Becoming a second koru-os — contract changes go upstream, not forked.
- Unikraft-shaped identity — entries declare `arms:`/`donor:`; the
  catalog outlives any single donor.
- Real-hardware claims on QEMU evidence — `tier:` is load-bearing.
- Drivers nobody demanded — the catalog is derived from what the
  desktop needs, not from what donors happen to offer.
- Fallbacks — a provider either binds its row with evidence or it is
  `refused:` and says so.
