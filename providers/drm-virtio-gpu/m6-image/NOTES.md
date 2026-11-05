# m6-linuxu on this Mac — what differed from the Windows-box recipes

Workdir: /tmp/m6-linuxu (scratch), branch provider/m6-linuxu.
Kraftfile: m3-busybox's, minus the busybox rootfs — `rootfs: ./rootfs`,
`cmd:` unused on this path (argv comes from QEMU `-append`).

## Mac deltas vs the /workspace recipes

1. **Cross-compile a static Linux ELF on macOS**: no gcc/lld/ld.lld here —
   clang links Mach-O only. Worked around by writing the ELF directly
   (Python-emitted ELF64 headers + raw-syscall x86_64 code). For M2
   (modetest/kmscube, glibc+dynloader) the Debian binaries + `ld-linux`
   must be fetched prebuilt — same shape as run-m6.sh but fetching DEBs
   rather than a local glibc root.
2. **Direct-gmake recipe** (kraft regen loops on this machine):
   `gmake -C .unikraft/build CONFIG_UK_BASE=$PWD/.unikraft/unikraft \
    C=$PWD/.unikraft/build/config A=$PWD \
    L="$PWD/.unikraft/apps/elfloader:$PWD/.unikraft/libs/libelf" \
    UK_CFLAGS='-std=gnu17'`
   - `L=` is REQUIRED — without it app-elfloader's objects never link
     (image boots to `weak main()`), same failure as liblwip earlier.
   - `C=` must point at `.unikraft/build/config`, not `$PWD/config` —
     the wrong path makes the sub-make fall into `kmenuconfig` (curses).
   - `LIBUKNETDEV` must be added to config — elfloader's
     `autogen/etc.c` needs `uk/netdev.h`.
3. **argv convention**: `argv[0]` = app name, `argv[1]` = program NAME
   resolved via `$PATH` (=/bin via ENVP0), not a path.
4. **QEMU**: `-append 'elfloader hello'`; `-initrd` optional (einitrd
   embedded).

## OPEN — exec lookup fails (M1 not yet green)

Image boots, elfloader runs, env/argv resolve — then
`locate_exec` → ENOENT for /bin/hello. The einitrd section is embedded
(544B = cpio size, `vfscore_einitrd_start/end`), `AUTOMOUNT_EINITRD=y`,
fstab CI1 = embedded→`/`→extract — but no Mount/Extract output (uk_pr_info
suppressed by MINIMAL banner) and the file never appears. Verified with a
getdents64 probe (probe can't even run — exec lookup fails first). Next
probe: enable info-level printk or instrument ukcpio_extract to see
whether extraction runs and what it writes.
