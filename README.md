# koru-desktop

> Working name. The host platform between koru-os and oshun/omni —
> rename is one command; say the word.

The **host that isn't macOS**. koru-os (Cypoe's platform namespace) owns
the contract: arms, provider rows, realization, milestones. oshun owns the
UI vocabulary and omni the environment charter. This repo owns the
composition that makes a bootable desktop out of them: disk images,
session wiring, compositor binding, and the **provider catalog** — the
drivers and services a desktop needs, each bound to a `koruos/*` row or
refused loudly.

**Arm-agnostic by design.** Unikraft is the first arm and first donor,
not the identity — koru-os's own doctrine is "Unikraft is one arm, not
the OS." Catalog entries declare `arms:`/`donor:`/`lane:` per slot;
`koru-native` providers and driver-silo personalities slot into the same
catalog without restructuring.

**Status: SKELETON.** Layout and catalog seeded; nothing boots yet.

## Layout

| path | owns |
|---|---|
| `providers/` | the catalog — one self-contained dir per provider slot (`entry.md` + `accept.sh` + `src/` + `evidence/`). See `PROVIDERS_CHALLENGE.md` |
| `session/` | compositor/shell/panel composition — non-driver work |
| `images/` | disk image recipes, QEMU run scripts, firmware notes |
| `patches/` | cross-cutting donor patches (per-entry patches live in `providers/<name>/patches/`) |
| `scripts/` | `realize-matrix.sh` (scoreboard), `check-provider.sh`, `board.sh` |
| `origins.lock` | provenance — every upstream source pinned to a SHA |

## Evidence classes

Inherited from koru-os: **V** verified at a pinned SHA, **M** measured on
the box (log given), **C** carried-over claim, **D** design only.
Self-report never counts — every `landed`/`naive`/`idiomatic` status
requires artifacts under `evidence/` or `board.sh` fails the claim.

## Pins (`origins.lock`)

| name | origin | revision | role |
|------|--------|----------|------|
| koru-os | Cypoe/koru-os | `c7c3474` | platform contract — consumed, not forked |
| koru | korulang/koru | `91014797` | toolchain |
| koru-libs | korulang/koru-libs | `0fcacb97` | packages + `unikraft/*` lifts |
| unikraft-stable | unikraft/unikraft | `7351f8b5` | L2 arm donor (kraft `stable` — the arm that boots Hyprland) |
| unikraft-staging | unikraft/unikraft | `3fdffba8` | staging donor (koru-libs lifts lineage) |
| oshun | korulang/oshun | `1adc1171` | host surface — the acceptance consumer |

Not pinned: omni (charter repo — rides on top as apps, never a build
dependency). Replication: `vand replicate origins.lock --root <clone-root>`
(see koru-os-cypoe README for the tool and its caveats).

## Boundaries

- **Contract/platform bugs** → upstream to Cypoe/koru-os as clean,
  separable commits.
- **Reusable Koru lifts** → koru-libs (`unikraft/*` under
  UNIKRAFT_CHALLENGE discipline).
- **UI vocabulary/components** → oshun; the `host/index.koruos.kz`
  platform part lands there once this repo makes the seams real.
- **Laws/charter** → omni `SCENE.md`.
- **This repo** owns composition, driver/provider enablement, images,
  and acceptance evidence — and none of the above.
