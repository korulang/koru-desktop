# usb-xhci — design census (scout deliverable, slot stays OPEN)

Evidence grades: **V** = verified by build/run this session · **M** = measured
(source read directly, file:line or URL) · **C** = corroborated (spec +
independent prior art agree) · **D** = design assertion, not yet checked.

Measured against tree `6e31b80` on `main`.

## (a) Register model + init sequence

xHCI BAR0 layout, four regions by register offset (C — xHCI 1.2 §5:
capability/operational/runtime/doorbell; corroborated by NONOS
`capsule_driver_xhci/src/regs.rs` module and xhci-nostd's
capability/operational/runtime register layers):

- **Capability** (offset 0): `CAPLENGTH` (operational offset), `HCIVERSION`,
  `HCSPARAMS1-3` (max slots, interrupters, ports), `HCCPARAMS1-2` (64-bit
  addressing, scratchpad restore count, ext caps ptr), `DBOFF` (doorbell
  offset), `RTSOFF` (runtime offset).
- **Operational** (CAPLENGTH): `USBCMD` (RS run/stop, HCRST, IE/INTE),
  `USBSTS` (HCH halted, CNR controller-not-ready, HCE, PCD),
  `PAGESIZE`, `DNCTRL`, `CRCR` (command ring ptr + cycle bit),
  `DCBAAP` (device context base address array), `CONFIG` (max slots
  enabled), then the `PORTSC` array — one 32-bit port-status reg per port
  (USB2 ports then USB3 ports, per PROTOCOL registers).
- **Runtime** (RTSOFF): `MFINDEX`, one interrupter register set per
  interrupter — `IMAN` (IE + IP pending bit), `IMOD` (moderation counter +
  interval), `ERSTSZ` (segment table size), `ERSTBA`, `ERDP` (event ring
  dequeue ptr + EHB busy bit).
- **Doorbell** (DBOFF): one 32-bit doorbell per slot; writing the target
  ep/doorbell-id kicks command/transfer execution.

Init sequence (C — xHCI 1.2 §4.2; same shape in NONOS `setup.rs` and
xhci-nostd `XhciController` init):

1. Halt + reset: `USBCMD.RS=0`, `USBCMD.HCRST=1`, poll `HCRST` self-clear
   then `USBSTS.CNR=0`. QEMU models exactly this: its `xhci_reset` zeroes
   usbcmd/crcr/dcbaap/config and sets `USBSTS_HCH` (M —
   qemu/hw/usb/hcd-xhci.c, `xhci_reset`).
2. `CONFIG = min(HCSPARAMS1.maxslots, wanted)`; allocate the Device
   Context Base Address Array (64-byte aligned) and write `DCBAAP`.
3. Allocate the command ring (≥16-byte aligned TRB array + link TRB);
   write `CRCR` = ring base | cycle=1.
4. Allocate the event ring + one Event Ring Segment Table entry
   (base+size), write `ERSTSZ=1`, `ERSTBA`, `ERDP`; ack stale `IMAN.IP`
   by writing IP=1.
5. Scratchpad buffers if `HCSPARAMS2` demands them — xhci-nostd allocates
   scratchpads automatically when the controller requires (C — README
   "Scratchpad buffer support"; QEMU's NEC model does not require them, C).
6. `USBCMD.RS=1 | INTE=1`; wait for `USBSTS.HCH=0`.

## (b) Command / event / transfer rings + cycle bit

- All rings hold 16-byte TRBs: command ring carries Command TRBs
  (EnableSlot, AddressDevice, ConfigureEP, EvalContext, StopEP, ResetEP,
  SetTRDequeuePtr, NoOp…); the event ring carries Event TRBs
  (CommandCompletion, PortStatusChange, TransferEvent); transfer rings
  carry Normal/Setup/Data/Status stage TRBs per endpoint (C — xHCI 1.2
  §4.9/§6.4).
- **Cycle bit**: producer and consumer each track a cycle-state bit.
  The producer toggles its cycle when the ring wraps (via the Link TRB's
  TC bit); a TRB is consumable only while its C bit equals the consumer's
  current cycle state. Event-ring reads use the CCS side of the same
  rule; `ERDP` must be rewritten with `EHB` set to unstall event delivery
  (C — xHCI 1.2 §4.9.3; corroborated by xhci-nostd "Transfer ring
  management … proper cycle bit handling" and NONOS `rings.rs`/`trb.rs`
  modules).
- Doorbell: after enqueueing command/transfer TRBs, ring the slot's
  doorbell (host controller doorbell = target 0 for commands; per-ep
  doorbell id for transfers) (C — xHCI 1.2 §4.7).
- Completion ordering: each command produces exactly one
  CommandCompletionEvent with the enqueue-TRB pointer for correlation;
  slot ids come back in the event for EnableSlot (C — §6.4.2).

## (c) Device enumeration to first descriptor

C — xHCI 1.2 §4.3; the same walk exists in NONOS `discover.rs`/`slots.rs`
and xhci-nostd's device manager:

1. Port attach: `PORTSC.CCS=1` on a root port raises a
   PortStatusChangeEvent; read PORTSC to learn speed (speed field) and
   which port.
2. Port reset (PORTSC.PR) for USB2 ports; USB3 ports link-train on attach.
   A second PortStatusChangeEvent signals reset complete (PRC).
3. `EnableSlot` command → slot id in its completion event.
4. Build an Input Context: slot context (root hub port number, speed,
   context-entries=1) + EP0 endpoint context (control, max packet size —
   8 for low-speed, 64 for full/high, 512 for USB3) + EP0 transfer-ring
   dequeue pointer. `AddressDevice` with BSR=0, or the two-step
   BSR=1 → read descriptor → `EvaluateContext` (corrects EP0 max packet)
   → `AddressDevice` BSR=0 — the BSR dance exists because the real EP0
   max packet size is not known until the first 8 bytes of the device
   descriptor (C — §4.3.4; xhci-nostd does the evaluate-context fix-up).
5. `GET_DESCRIPTOR(Device)` on EP0's transfer ring → VID:PID (accept
   expects `0627:0001` usb-kbd, per entry.md).
6. `GET_DESCRIPTOR(Configuration)` → `ConfigureEP` command with input
   context covering the claimed interface's endpoints → `SetConfiguration`.

## (d) Interrupt / MSI-X wiring

- The irq path is per-interrupter: event lands on the event ring →
  `IMAN.IP` latches → interrupt asserts if `IMAN.IE` (and `USBCMD.INTE`).
  Acknowledge by writing `IMAN.IP=1`; then drain the event ring and
  write `ERDP` with `EHB` to release the handler (C — xHCI 1.2 §5.5.2).
- `IMOD` gives interrupt moderation (counter/interval) — needed to keep
  hid-class interrupt transfers from an irq storm (C).
- QEMU `qemu-xhci` is a PCI function: MSI-X table at offset `0x3000`,
  PBA at `0x3800` of its MSI-X BAR, one MSI-X vector per interrupter via
  `msix_vector_use(pci_dev, n)` (M — qemu/hw/usb/hcd-xhci-pci.c,
  `OFF_MSIX_TABLE`/`OFF_MSIX_PBA`, `xhci_pci_intr_update`). With MSI-X
  unavailable it raises INTx — MSI-X is the right surface given
  `irq-msix` is landed (V — CATALOG.md, needs-edge satisfied).
- Pin `koruos/irq` + `koruos/pci` as the arms the driver binds: MSI-X
  table programming + vector→interrupter mapping comes from those
  providers, not driver-local PCI pokes (D — design intent, consistent
  with `lane: donor-c`).

## (e) QEMU `qemu-xhci` specifics the driver must satisfy

- The model emulates a Renesas NEC USB3 xHCI — the same part Linux
  quirks `XHCI_NEC_HOST` for (M — hcd-xhci.c header comment "emulates
  Renesas NEC USB 3.0").
- PCI class `0x0c/0x03/0x30` (serial-bus/USB/xHCI prog-if) — this is the
  signature `pci-enumeration` hands down (C — PCI class codes; same class
  signature NONOS matches, per its docs table).
- `usb-kbd`/`usb-tablet` present as USB-2.0 devices on root ports; the
  usb-kbd VID:PID is `0627:0001` (D — from entry.md accept text; confirm
  against QEMU's `hw/usb/dev-hid.c` before the driver commits to it).
- No scratchpad, no NEC firmware errata needed at QEMU: the historical
  `ER_FULL_HACK` workaround was root-caused to a QEMU event-queue irq
  bug fixed in QEMU 7da76e1 — do not port the workaround (M —
  patchew.org/QEMU/1486382139, "xhci: cleanups, add qemu-xhci device").
- Port count / interrupter count come from `HCSPARAMS` — do not
  hardcode; `qemu-xhci` exposes `p2`/`p3` port properties (D — QEMU
  device properties; confirm in hcd-xhci.c `Property` table).
- Falsifiable negative per entry.md: `qemu-xhci` with no devices attached
  must enumerate zero slots — the port scan path must tolerate all-CSC
  -clear PORTSC rows (C).

## (f) Prior-art census

| source | license | what it is | read grade |
|---|---|---|---|
| NON-OS/nonos-micro-kernel `userland/capsule_driver_xhci/` | **AGPL-3.0+** | working no_std xhci capsule: kbd/mouse/msc on root ports, no isoc | M — `src/main.rs` header + module list, nonos.software docs |
| suhteevah/xhci-nostd | Apache-2.0 | no_std Rust xHCI + HID keyboard, ~single-author | M — README + repo |
| torvalds/linux `drivers/usb/host/xhci*.c` | GPL-2.0 | the reference implementation (xhci.c/-ring/-mem/-hub/-ext-caps) | C — file list, quirk source |
| Xen `usbif.h` headers | GPL-2.0 | PV protocol headers, **not a driver** (entry.md correct) | M |
| QEMU `hw/usb/hcd-xhci{,-pci}.c` | LGPL-2.1+ | the tier:qemu target model | M — header + reset + pci glue |

**AGPL flag (diligence clause):** the NONOS capsule is the most complete
prior art and it is **AGPL-3.0+** — one rung more contagious than GPL.
Reading it for facts is fine; deriving or closely mirroring its code
would drag AGPL into the image. The entry records it *now* per the
clause — treat NONOS as spec-commentary only, never a lift source. The
structural takeaway it does legitimately confirm (C): the module split
`regs / rings / trb / slots / contexts / dma / discover / setup` is a
proven decomposition — it matches the xHCI spec's own layer names.

**xhci-nostd (Apache-2.0)** is the cleanest lift candidate: single-file
per-concern Rust, `no_std`+`alloc` only, explicitly targets
kernels/unikernels — but it is single-author, 2-star, and only covers
HID keyboard; its ring/context code still wants a spec audit before any
close mirror (C — README; D — quality).

**Linux xhci** is the quirk oracle — `xhci-ring.c`'s cycle handling and
`xhci-mem.c`'s context management are the canonical answer to edge cases
the spec leaves ambiguous — but GPL makes it read-only as well (C —
license; the driver structure itself is the teaching).

## Open questions for the claim

- Does the provider ABI (`koruos/usb.host`) already declare the surface
  (slot/endpoint/transfer verbs) or is this slot also defining it? (D —
  unmeasured, first question for the claimant.)
- `usb-hid`/`usb-storage` need the endpoint-transfer ABI stable — the
  claimant should pin the transfer-ring API shape before either child
  slot starts (C — needs edges in CATALOG.md).
- Interrupt transfers for HID poll rings vs. control/bulk for MSC — the
  ring layer must handle both pacing models (C — spec §4.11.5).
