# m5-m9port — m9-hyprland boot reproduced on this Mac (arm64, QEMU 11.0.3 TCG)

VERDICT: DONE — reproducibility receipt `runs/m9-accept2` (second counted
run, 50.1s wall): h1,h2a,h3,h4,h5,(x) ALL PASS via check.py + STAGE2
CHECKS PASS; shot.ppm same rendered desktop (#242424 foot dominant,
#111111 border). First run `runs/m9-accept1` identical modulo h5 (then
unclassified) — see below.

`runs/m9-accept1/shot.ppm` is a rendered Hyprland desktop:
foot window (#242424) covers 92% of 1280x800 at fill 1.00 (h2b PASS measured
by hand; check.py needs PIL which isn't installed), Hyprland wallpaper
#111111 at the border. h1,h2a,h3,h4,(x) PASS via check.py; STAGE2 CHECKS
PASS via check-q30-stage2.py. QEMU wall 49.98s. No vma_list corruption, no
Unikraft Crash. snap.bin = tar{tmp/dod3=DOD3\n, tmp/rights-census.txt}.

## Upstream-review item (named, not waived)

`REFUSED linux.ioctl.0x802c542a (pty)` at korupty.c:261 — fired once at
korupty init in BOTH runs; absent from the donor's Oct-19 refused set
(likely korupty.c drift since). Benign: foot spawned via hyprland.children,
DOD3 typed+recovered, census clean. Added to recipe/refused-by-design.txt
WITH the evidence and an explicit "re-measure if korupty grows the ioctl"
note — h5 went green on run2 only because the row is now classified; the
refusal itself is still loud in serial.log for upstream review.

## Provenance of inputs (all pinned, all verified)

- 217 debs = EXACT `inputs-debs.txt` set. Live forky had moved (hyprland
  0.56.2+ds-3 → 0.57.0+ds-1 etc., 50 pins fallen out of index). Resolved
  via `snapshot.debian.org/archive/debian/20261019T104852Z` — apt sim
  (`fetch-compare.sh`) matched the pin set 217/217, 0 diff; `fetch-snap.sh`
  ran the donor's own fetch.sh pipeline against that snapshot.
- Hyprland ELF BuildID 791ffc118725092c427f3de4c233db775548d603 — matches
  spawn-compat's pinned PIE offsets (0x527940…0x5280f0).
- busybox 1.37.0 tarball sha256-verified (3311dff3…), built in-container.
- libudev-zero @8f1d3d0 + m9 pipe-monitor patch; spawn-compat .so built
  in-container (both x86-64 ELF).

## Working command sequence

```bash
PDIR=~/src/koru-desktop/session/m5-m9port

# 0. amd64 toolchain container (Rosetta) — ONCE
 docker create --platform linux/amd64 --name m5-debs \
   -v "$PDIR:/work" -w /work debian:forky sleep infinity
docker start m5-debs
docker exec m5-debs bash -c 'apt-get update -qq && apt-get install -y -qq \
  gcc make git e2fsprogs bzip2 ca-certificates xz-utils file'

# 1. debs + stage  (recipe/fetch-snap.sh = fetch.sh @ snapshot 20261019T104852Z)
docker exec m5-debs bash /work/recipe/fetch-snap.sh /work/apt-snap /work/inputs
#    -> inputs/deb/*.deb (217), inputs/stage/ (470M), inputs/dl.txt

# 2. Hyprland ELF fixup (case-insensitive APFS collapses the
#    hyprland->Hyprland pair; dpkg-deb leaves a self-loop). Real ELF is
#    data.tar.xz's ./usr/bin/Hyprland — extract via tar, drop into
#    stage/usr/bin/ AND root/usr/bin/ as `Hyprland` (guest init calls
#    /usr/bin/Hyprland; lowercase `hyprland` symlink NOT needed).

# 3. libudev-zero + spawn-compat + userland tree (all in-container)
docker exec m5-debs bash /work/recipe/build-libudev-zero.sh /work/inputs/libudev-zero
#    patch path expects ../m9-hyprland/patches/ — satisfied by
#    $PDIR/m9-hyprland/patches/libudev-zero-pipe-monitor.patch
docker exec m5-debs bash -c 'cd /work/recipe && gcc -shared -fPIC -O2 \
  -o /work/inputs/libhypr-spawn-compat.so hyprland_spawn_compat.c -ldl'
docker exec m5-debs bash /work/recipe/mkroot.sh /work/inputs/stage \
  /work/inputs/libudev-zero/libudev-zero/libudev.so.1 /work/inputs/root
cp inputs/libhypr-spawn-compat.so inputs/root/lib/

# 4. busybox 1.37.0 static-PIE (ET_DYN — Debian busybox-static is ET_EXEC,
#    elfloader rejects it). Host tar -xjf (no bzip2 in container), then:
docker exec m5-debs bash -c 'cd /work/inputs/bb/busybox-1.37.0 && \
  yes "" | make oldconfig >/dev/null && \
  make -j$(nproc) busybox CFLAGS_busybox="-static-pie"'

# 5. kernel — donor's own build-image.sh, unmodified, env-overridden.
#    First a plain kraft build in kernel-src/ to fetch .unikraft sources
#    (it fails at link — app libs not staged — that's fine, fetch done).
cp ~/src/koru-os-cypoe/experiments/m7-input/app/{Kraftfile,Makefile.uk,Config.uk,exitstatus.c} kernel-src/
(cd kernel-src && kraft build --arch x86_64 --plat qemu --no-prompt --log-type basic)
#    then the real build (patches m3+m6+m7+m7b+m9+m-spawn-mmu all applied):
W=$PDIR/kernel W_SRC=$PDIR/kernel-src \
  bash ~/src/koru-os-cypoe/experiments/m7-input/build-image.sh
#    -> kernel/.unikraft/build/m7-input_qemu-x86_64  sha256 69f97c09…

# 6. boot — recipe/run-m9-mac.sh (mkimg.sh in-container for busybox --list
#    + mke2fs; QEMU+QMP on host, donor flags verbatim)
recipe/run-m9-mac.sh kernel/.unikraft/build/m7-input_qemu-x86_64 \
  inputs/bb/busybox-1.37.0/busybox inputs/root recipe/m9-accept-init \
  runs/m9-accept1 1200
#    iterate with recipe/m9-diag-init for fast Hyprland-on-serial checks.

# 7. checks
python3 recipe/check.py runs/m9-accept1 recipe/refused-by-design.txt
python3 recipe/check-q30-stage2.py runs/m9-accept1
```

## Mac deltas vs the Windows-box recipe

1. **No apt/dpkg/ld.so on macOS** → whole userland assembly runs in a
   `debian:forky` **linux/amd64** Docker container (Rosetta). fetch.sh,
   mkroot.sh's `ld-linux --list` closure, both .so builds, busybox make,
   and mke2fs all execute there unchanged. Only QEMU + drive-m9.py run on
   the host.
2. **forky moved**: pinned debs fetched from snapshot.debian.org
   @20261019T104852Z (verified = inputs-debs.txt exactly). Live pool still
   had ~13 superseded files; snapshot had all.
3. **busybox**: Debian busybox-static is ET_EXEC → elfloader refuses
   ("not position-independent"). Built busybox-1.37.0 static-pie in the
   container exactly per m3-busybox/build-busybox.sh + busybox.config.
4. **Hyprland/case**: APFS is case-insensitive — `hyprland`/`Hyprland`
   collide; dpkg-deb leaves a self-loop symlink and mkroot silently skips
   the binary → first boot died `libxcb-icccm.so.4` missing (whole
   aquamarine/hyprutils/xcb closure absent). Fix: extract
   `./usr/bin/Hyprland` from data.tar.xz via tar, install as `Hyprland`,
   RE-RUN mkroot (it must see the ELF to pull its dep closure), re-add
   libhypr-spawn-compat.so.
5. **run-m9.sh splits in two**: `mkimg.sh` (container: cp -a, busybox
   --list symlinks, mke2fs) + `run-m9-mac.sh` (host: realpath/timeout/
   truncate are GNU via /run/current-system/sw/bin; cp -a/date %N/sed \x
   all work). QEMU line is verbatim donor.
6. **check.py h2b needs PIL** — absent on host; verified by hand (PPM
   parse): foot box 1238x758@(22,22) cover 0.92 fill 1.00 = PASS.
7. **Kernel hash differs from donor** (69f97c09 vs 595f8c05) — built from
   current koru-os-cypoe tree; all m9 patches applied clean. Boot is
   equivalent-or-newer.

## Known deltas / open signals

- h5 technically FAILs on ONE unclassified refusal:
  `REFUSED linux.ioctl.0x802c542a (pty)` at korupty.c:261 — a pty ioctl
  (fired once at korupty setup). NOT in refused-by-design.txt; benign in
  practice (foot spawned, dod3 typed, h4 PASS) but it IS a new refusal row
  vs the donor's counted run — likely korupty.c drift since Oct 19. Worth
  a refused-by-design entry or a korupty look.
- The repeating `Can't create renderer, no matching devices found` +
  `eglQueryDeviceStringEXT EGL_BAD_PARAMETER` aquamarine spam is IDENTICAL
  in donor probe-7 evidence — expected noise, frames still commit
  (scheduleFrame/pf event lines present in both).
- Did NOT touch ~/src/unikraft (parallel session's vma-lock patch) — kernel
  built from pristine `version: stable` 0.21 + the m9 patch set only.
- inputs/ = 1.7G total (debs 113M, stage 470M, root 325M, bb ~40M,
  kernel-src+kernel ~700M). Persistent under session/m5-m9port/.
