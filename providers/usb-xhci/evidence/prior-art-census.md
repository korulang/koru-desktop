# usb-xhci scout — evidence ledger (tree 6e31b80, scout/usb-xhci)

## Sources read this session

| # | source | URL | license | what was read |
|---|---|---|---|---|
| 1 | NON-OS/nonos-micro-kernel, userland/capsule_driver_xhci | github.com/NON-OS/nonos-micro-kernel/blob/main/userland/capsule_driver_xhci/src/main.rs | AGPL-3.0+ (header, main.rs) | main.rs — module list (constants/contexts/controller/discover/dma/error/handles/protocol/regs/rings/server/setup/slots/trb) |
| 2 | NONOS USB driver docs | nonos.software/docs/kernel/drivers/usb/ | — | support matrix: xhci0 capsule, kbd/mouse/msc on root ports, no isoc, hubs "proven on models only" |
| 3 | suhteevah/xhci-nostd | github.com/suhteevah/xhci-nostd | Apache-2.0 | README — features/architecture (XhciController, rings, contexts, HID kbd) |
| 4 | qemu/qemu hw/usb/hcd-xhci.c | github.com/qemu/qemu/blob/master/hw/usb/hcd-xhci.c | LGPL-2.1+ | header (emulates Renesas NEC USB3), xhci_reset body (clears usbcmd/crcr/dcbaap/config, USBSTS_HCH) |
| 5 | qemu/qemu hw/usb/hcd-xhci-pci.c | qemu.googlesource.com/qemu/+/f5ab12caba4f1656479c1feb5248beac1c833243/hw/usb/hcd-xhci-pci.c | LGPL-2.1+ | OFF_MSIX_TABLE=0x3000, OFF_MSIX_PBA=0x3800, msix_vector_use per interrupter |
| 6 | QEMU commit 755fba11 | github.com/qemu/qemu/commit/755fba11fbcadb4ba27700a99f29ebdf9bb63c51 | — | qemu-xhci moved to hcd-xhci-pci.c (2020) |
| 7 | patchew: "xhci: cleanups, add qemu-xhci device" | patchew.org/QEMU/1486382139-30630-1-git-send-email-kraxel@redhat.com/1486382139-30630-2-git-send-email-kraxel@redhat.com/ | — | ER_FULL_HACK history: NEC quirk root-caused to QEMU event-queue irq bug 7da76e1; don't port the workaround |
| 8 | torvalds/linux drivers/usb/host/xhci* | github.com/torvalds/linux/tree/master/drivers/usb/host | GPL-2.0 | file inventory only (quirk oracle, not liftable) |

## Spec anchor

Intel/USB-IF xHCI 1.2 spec — section numbers cited in design.md
(§4.2 init, §4.3 enumeration, §4.7 doorbell, §4.9 TRB rings, §4.11.5
transfer types, §5.5.2 interrupter registers, §6.4 TRB formats).

## Not read this session (honest limits)

- NONOS regs.rs/rings.rs bodies — module layout only; AGPL makes them
  spec-commentary anyway.
- xhci-nostd source files — README/architecture only; single-author,
  narrow coverage (HID kbd only).
- QEMU hcd-xhci.c full file — header + reset + pci glue excerpts;
  port-model properties (p2/p3) cited D-grade, unconfirmed.
- hw/usb/dev-hid.c for the usb-kbd VID:PID — entry.md's 0627:0001
  carried as D until confirmed.

## Copyleft ledger (diligence clause)

- AGPL-3.0+: NONOS xhci capsule — commentary only, never a lift source.
- GPL-2.0: Linux xhci*, Xen usbif.h — quirk oracle / PV headers.
- Apache-2.0: xhci-nostd — cleanest lift candidate, still wants audit.
- LGPL-2.1+: QEMU — reference model only (not image code).
