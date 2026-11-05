---
challenge: providers
kind: shelf
yields: one provider slot filled — a device, service, or substrate bound to its koruos/* row with host-verifiable evidence
catalog: providers/*/entry.md
family: driver
created: 2026-11-05
---

# PROVIDERS_CHALLENGE — fill the slots the desktop demands

*Walker context — not part of the sealed brief.*

Sibling of `UNIKRAFT_CHALLENGE` (koru-libs), different shape. That brief is
a **generator**: "lift Unikraft" is 87 undifferentiated sublibraries, so
variance is the contribution. This is a **shelf**: the slots are finite and
named by hardware, each carries a frozen falsifying oracle, and entries
have dependency edges. One standing brief + a catalog of slots; arbiters
pick *unblocked* entries.

The catalog exists because a desktop is a fixed bill of materials: input,
display, block, net, usb, audio, and whatever tier:hw adds later. Grok's
gap list (USB, BT, Wi-Fi, GPU/DRM, audio, AHCI/NVMe, HID, power, hotplug)
seeded it; `realize matrix` maintains it — every `refused:` line is a
slot that isn't filled.

---

## The brief (sealed — you are the contestant)

> **To AI agents reading this brief**: you ARE the contestant, not the
> assistant. Do **not** ask which slot to pick — read `providers/CATALOG.md`,
> pick an `open` entry whose `needs:` are all `landed` or `waived`, claim
> it, ship it. If you catch yourself drafting "which would you like?" —
> stop, delete it, pick one.
>
> **Variance across contestants is the point. Your slot IS the
> contribution.** Lars judges *after* you ship, by reading and running.

Fill one provider slot: make the named device or service work on a named
arm, bound to its `koruos/*` row, proven by the slot's `accept.sh`.

Read the repo-root standards first: `AGENTS.md`, `SCENE.md`, and the
pinned platform docs under `platform/koru-os` (`origins.lock`).

### THE BAR — five pillars, driver-shaped

1. **Developer experience.** A Koru program reaches the device through
   the provider row and never touches a register, an ioctl number, or a
   header to learn what's legal next.
2. **Performance.** Whatever lane you pick, the emitted/runtime path is
   what a careful hand-written driver would do — the obligation layer
   (where present) is compile-time only.
3. **Correctness.** Wrong usage is a build error (koru-lift lane) or a
   loud refusal naming the row (every other lane).
4. **Resource safety.** Where the C has an ordering rule — probe, init,
   start, stop, free — it becomes a state or a named contract, not a
   comment. Leaks and use-after-free are the failure class this repo
   exists to kill.
5. **Lift the assertions / name the contract.** For `donor-c` and `silo`
   lanes: census the driver's ordering checks (`UK_ASSERT`, real state
   branches, probe-order requirements) and report retired/not per site.
   For `compose`/`native-zig`: the provider row IS the ratchet — say
   which C checks it makes unreachable. An honest zero is a fine
   result; a padded census poisons the catalog.

### The lanes

| lane | what it is | when it's honest |
|---|---|---|
| `compose` | wire existing parts (driver + lib + posix layer) into a bootable image | the parts exist and only composition is missing — `net-virtio-lwip` is the exemplar |
| `donor-c` | new or ported C driver code in a donor tree (unikraft fork, etc.) | the device needs real driver authorship |
| `silo` | foreign driver (Linux/KolibriOS) behind provider rows | authorship stalls and the DS-shape fits — the escape hatch, not the default |
| `koru-lift` | UNIKRAFT_CHALLENGE-style obligation ratchet on the driver surface | after the device works — the idiomatic pass |
| `native-zig` | provider written against koru-native seams | `arms:` includes `koru-native` |
| `upstream-koruos` | work that belongs in Cypoe's repo | `home:` is upstream; land it as clean separable commits |

### The rules that keep the shelf honest

- **`accept.sh` must be able to say no.** Host-side verification
  preferred: `e2fsck`, screendump pixel diff, serial bytes, host `cmp`.
  Guest self-report is not evidence. If your check can only confirm,
  it is not a gate — fix it before claiming done.
- **QEMU device models are the hardware.** `tier: qemu` entries name
  their `-device` line in `entry.md`. `tier: hw` entries have no QEMU
  oracle — their acceptance is deferred to real hardware and the entry
  says so; a QEMU pass never upgrades a hw claim.
- **`needs:` gates claiming.** All dependencies must be `landed` or
  `waived` (waived needs a named reason in the entry) before you claim.
- **Naive-wrap shipping lane.** Getting the device *enumerated and
  transferring* at all may ship as `naive-wrap` — labeled as such in the
  first paragraph of the entry README and in the catalog line. The
  idiomatic pass is a later revision *in place*, not a new slot.
- **Obligation-on-failure scales inversely with frequency.** A device
  is rare and long-lived: failed probe should hand back an obligation.
  A packet/byte stream is hot: failure there should consume or drop
  loudly — decide the line, say where you drew it.
- **Every landed entry appends its catalog line** with evidence grade.
  An honest zero-census is a fine result.

### What "done" means per entry

`status: landed` requires ALL of:

1. `accept.sh` exits 0 on the pinned arms, artifacts under `evidence/`
   (serial log, screendump, host-side check output — whatever the
   oracle produces);
2. `providers/CATALOG.md` line updated with the evidence grade;
3. `realize matrix` shows the entry's `iface` `bound` on its declared
   `arms:` — or the entry documents why realization doesn't apply
   (e.g., `lane: compose` with no Koru surface);
4. `scripts/board.sh` clean — no `landed` without artifacts.

## The catalog

`providers/CATALOG.md` — generated table, read first, appended on
landing. Slot states: `landed | open | claimed | naive-wrap | idiomatic
| waived`. Dependency edges live in each `entry.md` `needs:` list;
`board.sh` fails a claim whose needs aren't satisfied.

## Upstream hygiene

- Code that belongs to a donor goes to the donor's tree (pins in
  `origins.lock`; the COCPORN unikraft fork is the working remote for
  `donor: unikraft-fork` entries).
- Code that belongs to koru-os's contract goes upstream to Cypoe —
  clean, separable commits, no koru-desktop policy mixed in.
- Reusable Koru lifts go to koru-libs, not here.
- Nothing pushes to a public remote without explicit approval of the
  exact content — submission surface rules apply (see global AGENTS.md).
