# m6-image provenance audit — 2026-11-05

Every fetch/clone/download the recipe consumes, resolved to an exact
identity and verified against the remote (`git ls-remote`) or by sha256
of the fetched artifact. See `origins.lock` for the machine-readable
pin; `AUDIT-2026-11-05.md` for the earlier pass's findings.

## Git dependencies (kraft `libraries:` + `unikraft: stable`)

| dep | repo | SHA (verified via ls-remote) | license |
|---|---|---|---|
| unikraft | github.com/unikraft/unikraft.git | `7351f8b563c9e6773da9fdd5bacde6ac91e32112` | BSD-3-Clause |
| app-elfloader | github.com/unikraft/app-elfloader.git | `6ecbcd08d85b4688112a3b275a9c8446cf59e25d` | BSD-3-Clause |
| lib-libelf | github.com/unikraft/lib-libelf.git | `c03d4ceb22a2b9e9799238f9c277a14284d313ad` | BSD-3-Clause |

## Debian userland (ftp.debian.org pool downloads)

Pin is the sha256 of the `.deb` artifact — pool URLs move between
mirrors, so the digest is the identity, not the path.

| deb | version | sha256 | license |
|---|---|---|---|
| libc6 | 2.41-12+deb13u4 | `967aa62605721081c3eb2a17650611a792aa802d76a6511d1840242623d204c9` | LGPL-2.1-or-later |
| libdrm2 | 2.4.124-2 | `fe2276901c7cd7b8079de63072d37fe1cbeb4eb001a3bc1f1d662ad89aa0890e` | MIT |
| libdrm-tests | 2.4.124-2 | `387a1206956ea7ccb8562b0f8d060c4e0224a34b1cb8dc630ec1f446775d5be6` | MIT |

## Recipe-local sources (no external fetch)

- `gen-hello.py` — generates `rootfs/bin/hello` (a minimal syscall-only
  ELF) inline in Python. Zero fetches.
- `rootfs/` — local directory packed into the embedded initrd by kraft.
  Contains the Debian artifacts above plus the generated hello ELF.
- `korukms/` sources (bochs.c, drm_linux.c, virtio_gpu.c, korusysfs.c)
  come from the sibling koru-os repo — authored in-repo, MIT, not an
  external fetch (upstream SHA tracked in koru-os git history).

## Non-fetches confirmed

- **No musl**: the recipe's userland is Debian glibc only. Nothing to
  pin (the diligence clause asks for a pin per *source*, not per
  hypothetical).
- **No other downloads**: Kraftfile lists exactly the two library repos
  + `unikraft: stable` + `rootfs:` + `cmd:`; no curl/wget/clone in
  `gen-hello.py`, `NOTES.md`, or anywhere else in the recipe.

## Honest drift note

`version: stable` is still a floating ref *at build time* — the SHAs
above record what `stable` resolved to when audited (2026-11-05). A
rebuild tomorrow may resolve differently; if reproducibility matters,
change `version:` to the SHA and kraft will pin it.
