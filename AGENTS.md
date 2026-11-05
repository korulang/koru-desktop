# AGENTS.md — koru-desktop

Host platform between koru-os and oshun/omni. `SCENE.md` is the north
star; `PROVIDERS_CHALLENGE.md` is the standing brief for catalog work;
`providers/CATALOG.md` is the slot board.

## Inherited rules (from koru / koru-libs / koru-os)

- **A koruc wall is a compiler bug.** The fix lands in `~/src/koru`,
  never a consumer workaround — no spelling dodges, no wrapper hacks.
- **Evidence classes V/M/C/D.** Claims get re-measured; "unmeasured"
  is a complete sentence.
- **Refusals are the spec's loud axis.** A provider that can't bind
  says `refused:` with a reason — never a silent skip, never ENOSYS
  without a refusal entry.
- **No fallbacks.** Real surface or loud failure.
- **Never present option menus.** Pick the steelman, defend it in a
  paragraph, let Lars veto.

## Pins discipline

`origins.lock` is the provenance ledger — every upstream source a build
or read depends on is pinned to a full SHA. New dependency → new entry,
never a floating branch. `vand replicate --dry-run` before real
replication (the libudev-zero `make install` caveat in koru-os applies).

## Provider entry discipline

- One self-contained dir per slot under `providers/`. Copy
  `providers/_template/`.
- `entry.md` frontmatter is the contract: `iface`, `arms`, `lane`,
  `donor`, `needs`, `accept`, `tier`, `status`, `home`.
- `accept.sh` must be able to say **no** — a check that can only confirm
  is not a gate. Host-side verification preferred (e2fsck, screendump
  diff, serial bytes) over guest self-report.
- Status transitions: `open → claimed → naive-wrap → idiomatic → landed`
  (or `waived` with a named reason). `landed` requires `evidence/`
  artifacts; `scripts/board.sh` enforces it.
- `needs:` edges must resolve to catalog entries that are `landed` or
  `waived` before a slot is claimed.
- Append your line to `providers/CATALOG.md` on landing — that is how
  the next contestant knows the slot is gone.

## Boundaries (where work goes instead)

| work | home |
|---|---|
| koru-os contract / provider-row asks / platform bugs | upstream to Cypoe/koru-os, clean separable commits |
| reusable Koru lifts of Unikraft subsystems | koru-libs `unikraft/*` (UNIKRAFT_CHALLENGE) |
| UI vocabulary, components, `host/index.<platform>.kz` parts | oshun |
| environment charter, laws, commissions | omni SCENE.md |
| driver/provider enablement, images, session, acceptance | **here** |

## Before pushing

```bash
bash scripts/board.sh              # catalog ↔ evidence consistency
bash scripts/realize-matrix.sh     # arm-matrix.txt over provider programs
bash providers/<name>/accept.sh    # per-entry oracle
```

`git push` to a public remote is submission surface — explicit approval
of exact content required, every time.
