# session M4 — Hyprland past fault-delivery to renderer init (DESIGN.md ladder)

VERDICT: compositor progresses further than M3 — renderer fully
initializes (shaders compiled, framebuffers created, GBM/EGL llvmpipe
up, card0 via libseat, event0 via libinput) — then a userspace fault
on the render path walks a CORRUPTED vma_list. Instrumented
diagnostic, no crash-laundering.

## What the instrument proved (evidence/m4-vma-corruption.raw)

vmem_vma_find gained a canonical-address guard (kernel-side patch, same
upstream lane). It fires three times before death:

    ukvmem: corrupt vma_list node 0xff111111ff1110f1 at find 0x1017b26000-0x1017b26001
    ukvmem: corrupt vma_list node 0xff111111ff1110f1 at find 0x1006bfb000-0x1006bfb001
    ukvmem: corrupt vma_list node 0xff111111ff1110f1 at find 0x0-0x1

- The corrupt "node" is a pointer into the faulting render VA region
  (0x1017b26000) with 0xff pixel bytes — a VMA node whose memory was
  overwritten by llvmpipe's own buffers, i.e. a vma freed without
  unlink (use-after-free) or a link field scribbled by a wild write.
- It poisons EVERY subsequent list walk, including find(0) — the
  corruption is durable in the list, not a one-off deref.

## Fixes carried from M3 (all in this image)

- unikraft-0.21-signal-deliver-terminate.patch (koru-os-cypoe 3895785)
- STACK_SIZE_PAGE_ORDER 4->10 (4MB thread stacks)
- VGPU_VRAM_SIZE 16->4MiB (post-extract buddy has no order-12)
- --i-am-really-stupid (privileges check)

## Next wall (named)

Audit lib/ukvmem/vma.c unlink paths for a free-without-unlink (the
find's guard proves the node is still linked while holding user data),
or add vas-level locking if the writer is a concurrent mmap/munmap —
Hyprland runs render threads against the same vas. The instrument's
guard prints the corrupt node address next run; from there it's a
list_del audit, not a search.
