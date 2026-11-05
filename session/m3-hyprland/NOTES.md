# session M3 — Hyprland on card0 (DESIGN.md ladder)

VERDICT: receipt-1 GREEN (wlroots dep tree measured + mounted), receipt-2
RED (Hyprland process alive) — the dep chain resolves end-to-end and the
binary exec's, but Hyprland segfaults during startup and the fault
*delivery* blows the unikraft kernel stack. Next debugging front named.

## Receipt 1 — dep tree GREEN

217 Debian forky debs (m9-hyprland/inputs-debs.txt shopping list,
apt-resolved closure: hyprland 0.56 + aquamarine + hyprland libs +
mesa/GBM/EGL + glib stack + xkb + fonts + foot) resolved to current
forky URLs via the binary-amd64 Packages index, sha-fetched, extracted
to a 471MB stage, merged into the m6 linuxu rootfs.

The whole dynamic-link chain resolves: glibc 2.43 + ld.so.conf triarch
dirs + every NEEDED .so down to leaf (libxcb-icccm, libhyprutils,
libjxl, libselinux). No missing-library refusals after the fixups
below — the loader gets all the way to running Hyprland's code.

## Dep-tree fixes measured

- `cp -a stage/ rootfs/` nests as `rootfs/stage/` not a merge — contents-level
  copy required (`stage/.`).
- Hyprland's real ELF is `usr/bin/Hyprland` (the `hyprland`->`Hyprland`
  symlink pair is circular); bsdtar skips it — extract via python tarfile.
- exec bit: `chmod +x` (python-extracted file lands 0644 → EPERM).
- glibc 2.43 must shadow at `/lib/{libc,libm}.so.6` AND /lib64/ld-linux —
  hand-placed 2.41 stubs version-refuse `GLIBC_2.43`.
- /etc/ld.so.conf + ld.so.conf.d/{libc,x86_64-linux-gnu}.conf — the
  triarch dirs are how forky libs resolve; missing conf = "cannot open
  shared object file" even with files present.
- envp: XDG_RUNTIME_DIR=/tmp/xdg, LIBSEAT_BACKEND=noop, HOME=/tmp
  (ENVP4-6), PATH=/bin:/usr/bin.

## Receipt 2 — RED, root cause named

Hyprland launches far enough to link, then a userspace page fault ->
ukvmem tries to deliver SIGSEGV -> the *delivery path itself* overflows
the kernel stack (vma_stack.c:74 guard page at rip 0x132531 inside
sys_error_handler_except). Two stacked findings:

1. Hyprland segfaults early (likely an ENOSYS syscall the unikraft
   posix shim doesn't serve — the log shows brk/set_robust_list/rseq
   ENOSYS before the crash; glibc 2.43 + aquamarine probe card0 need
   more surface than modetest/libinput did).
2. The fault-*delivery* path exhausting the kernel stack is a platform
   bug independent of the app: signal delivery should never OOM the
   kernel stack. Worth a koru-os fix + regression regardless of the
   compositor outcome.

## Next

Boot Hyprland under a syscall-trace (uk_syscall6 shim already prints
ENOSYS at ERR) to name the faulting syscall; the stack-overflow on
delivery is a separate upstream fix in koru-os-cypoe.

## Evidence

- evidence/m3-hyprland-crash.raw — full boot: extract, sysfs 60 entries,
  dep resolution, ENOSYS log, the SIGSEGV-delivery stack crash.
