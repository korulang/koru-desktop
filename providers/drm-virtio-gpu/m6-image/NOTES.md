# m6-linuxu on this Mac — recipe + what differed from the Windows-box recipes

M1 PROVEN: `hello-linuxu` on serial (evidence/m1-hello-serial.log) — a
Linux static-PIE ELF run by unikraft app-elfloader, image built end-to-end
on macOS + QEMU TCG.

Workdir: /tmp/m6-linuxu (scratch), branch provider/m6-linuxu,
Kraftfile.hello here. The recipe is m3-busybox-shaped: `rootfs: ./rootfs`
embedded as EINITRD, app-elfloader + libelf libraries.

## What a Mac run actually needs (all measured, all non-obvious)

1. **Build**: kraft build once (fetches + generates config), then drive
   gmake directly — kraft's regen loops on this machine:
   `gmake -C .unikraft/build CONFIG_UK_BASE=$PWD/.unikraft/unikraft \
    C=$PWD/.unikraft/build/config A=$PWD \
    L="$PWD/.unikraft/apps/elfloader:$PWD/.unikraft/libs/libelf" \
    UK_CFLAGS='-std=gnu17'`
   - `L=` is REQUIRED — without it app-elfloader's objects never link
     (image boots to `weak main()`); same failure as liblwip earlier.
   - `C=` must point at `.unikraft/build/config`.
   - `LIBUKNETDEV` must be added to config — elfloader's
     `autogen/etc.c` needs `uk/netdev.h`.
   - Hand-edits to `config` do NOT propagate to
     `build/include/uk/bits/config.h` — the generated header is what the
     C code reads. Don't fight config; fix the Kraftfile instead.
2. **argv convention**: kernel name is auto argv[0]; argv[1] is the
   program NAME resolved via `$PATH` (= `/bin` via ENVP0). Boot with
   `-append 'hello'` — NOT `elfloader hello` (that made argv[1] =
   "elfloader", the error said `stat /bin/elfloader` — off by one).
3. **The initrd IS embedded** (CI_EINITRD): CI0 mounts ramfs at `/`,
   CI1 runs `extract` driver on the embedded cpio into it. Both succeed
   — fstab entries from config.h: CI0=ramfs@/, CI1=embedded→`/`→extract.
4. **The guest binary must be position-independent** — elfloader rejects
   ET_EXEC ("ELF executable is not position-independent"). No Linux
   cross-linker exists on this Mac — hello is a Python-emitted ET_DYN
   ELF, RIP-relative raw syscalls (see gen-hello.py below). For M2,
   modetest/kmscube are glibc+dynamic — fetch Debian DEBs + ld-linux +
   set `LD_LIBRARY_PATH` via env, same as run-m6.sh but downloading
   binaries instead of a local glibc root.
5. **Debug printing**: `KLVL_WARN`/`KLVL_CRIT` unset by default — mount/
   extract failures print nothing. KLVL_INFO+DEBUG on gave visibility.
   Serial logs contain NUL bytes — use `strings`, not `grep`.

## Status
M1 done. M2 (kmscube/modetest in image) next: extend rootfs with
Debian libdrm + kmscube + glibc + ld.so (dynamic exec path is the
untested part — static-PIE proven, dynamic ELF loading not yet).
