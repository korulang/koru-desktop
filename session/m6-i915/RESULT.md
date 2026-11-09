# m6-i915 — feasibility analysis: minimal display-only driver for Kaby Lake-R (iGPU-630, T590 eDP)

VERDICT: **FEASIBLE — but the floor is ~110–150k LOC of reference, not a port.**
The display engine alone is half of i915; the KBL-R eDP-relevant slice is
~76k LOC of register-level code plus ~35k of platform/memory plumbing a Koru
driver must replace or reimplement. Tractable for an agent-driven team with
libgfxinit as a second reference and a static-scanout milestone first — not
tractable as a "port i915" or as a weekend extraction. Numbers below are
measured on linux `d487237` (7.4-rc1, 2026-11-06); KBL (`kbl_desc`,
DISPLAY_VER 9, gen9.5 = "Skylake H0" per the KBL bring-up series) is still
supported.

---

## 1. Composition map — `drivers/gpu/drm/i915/` @ linux d487237

425,497 LOC across 904 `.c`/`.h` files (measured, `wc -l`):

| region | LOC | files | what it is | display-only fate |
|---|---:|---:|---|---|
| `display/` | 208,154 | 345 | KMS engine: atomic commit, crtcs/planes/connectors, all outputs, DPLL/CDCLK, watermarks, IRQ | keep ~112k file-level → ~76k gen9/eDP-relevant |
| `gt/` | 94,268 | 226 | GT/compute: engines, GuC/HuC fw (`gt/uc/` 22.5k), submission, RC6 | **drop all** |
| top-level | 51,664 | 156 | probe/PCI glue, GEM facade, IRQ master, uncore MMIO, PM, debugfs, perf | keep ~24k |
| `gem/` | 30,610 | 67 | GEM objects, TTM backing, execbuf, contexts, dmabuf | keep ~14k (execbuf/context/userptr/lmem dropped) |
| `gvt/` | 23,847 | 40 | GVT-g PCI passthrough virtualization | **drop all** |
| `selftests/` | 14,301 | 47 | KUnit selftests | **drop all** |
| `pxp/` | 2,653 | 23 | Protected Xe Path (HDCP content prot) | **drop all** |

Droppable-at-file-level total: ~160k LOC (gt+gvt+selftests+pxp+execbuf-side
gem+non-gen9 display). What remains is the number that matters:

- `display/` keep-set: **112,191 LOC / 163 files** (exact, file-level)
- gen9+eDP-relevant inside that set: **~76,000 LOC** (per-file relevance
  fractions applied to the monoliths — ±15% honest error; see §2)
- top-level keep-set: **~24,000 LOC** (uncore, vma, driver probe, gem facade,
  irq, active/deps, sw_fence, runtime_pm, clock_gating, pcode, step,
  device_info, region/ttm glue — minus query/sysfs/ioc32/memcpy trims)
- gem display subset: **~14,000 LOC** (object/shmem/pages/tiling/stolen/
  region/ttm/buddy/mman/create/phys/wait/busy/domain/dmabuf/clflush/
  shrinker/internal — execbuffer 3.7k, context 3.4k, userptr, lmem, panic
  excluded)

**Reference corpus to master: ~114k LOC**, of which ~76k is the actual
display-engine register work.

Licensing (measured per-file SPDX): display/, gem/, gt/ are **MIT**
(~730 files) vs ~17 GPL-2.0 files concentrated in core top-level/selftests.
A display-only extraction is MIT-derived — clean for koru-desktop's
provenance rules (still record `license:`/`provenance:` in the entry).

## 2. The Kaby Lake-R floor — named subset

KBL = DISPLAY_VER 9, HAS_DDI + HAS_PCH_SPLIT (display logic on PCH).
Single eDP output on one DDI port, one pipe, universal planes, llvmpipe
(scanout only — no 3D/compute/Guc/HuC needed; DMC firmware optional —
absent ⇒ runtime-PM disabled, display still works, `intel_dmc.c:1504`).

### Must exist, by subsystem (i915 file → role)

**Platform/probe** — `i915_driver.c`, `i915_pci.c`, `intel_uncore.c`
(MMIO map + forcewake), `intel_step.c`, `intel_device_info.c` (partial),
`intel_runtime_pm.c`, `intel_wakeref.c`, `intel_clock_gating.c`,
`intel_pcode.c`, `intel_display_device.c` (only `kbl_desc` + platform
table — ~35% of file), `intel_display_driver.c`, `intel_vga.c`
(legacy VGA off), `intel_quirks.c` (partial).

**Modeset/commit core** — `intel_display.c` (**the monolith**, 8,624 LOC;
gen9-relevant ≈45% ≈3.9k: transcoder enable/disable, pipe config,
atomic check, commit tail sequencing, encoder hooks — joiner/ultrajoiner/
DSB/VRR/fastset/audio-infoframe paths droppable), `intel_atomic.c`,
`intel_crtc.c` (904, ~85%), `intel_plane.c` (2,040, ~80%),
`skl_universal_plane.c` + regs (3.3k, ~75% — gen9 uses universal planes
for primary+sprites, confirmed `intel_crtc.c:344-362`), `intel_cursor.c`,
`intel_fb.c` (2.4k, ~80%), `intel_global_state.c`, `intel_frontbuffer.c`,
`intel_modeset_setup.c`, `intel_modeset_verify.c`, `intel_modeset_lock.c`,
`intel_encoder.c`, `intel_connector.c`, `intel_initial_plane.c`
(firmware-fb handover — useful if GOP/coreboot left a mode),
`intel_parent.c`, `intel_display_params.c`, `intel_display_utils.c`.

**Display engines** —
- clocks: `intel_dpll_mgr.c` (5.4k, **~40%** ≈2.2k gen9 shared-DPLL path —
  hardest arithmetic in the driver) + `intel_dpll.c` (2.3k, ~45%) +
  `intel_cdclk.c` (4.4k, ~45%) + `intel_pch_refclk.c` + `intel_dram.c`
  (867, DRAM timings feed WM/BW).
- output: `intel_ddi.c` (5.6k, ~55% — gen9 DDI port+PHY; TC/snps/cx0/lt/dkl
  dispatch droppable) + `intel_ddi_buf_trans.c` (1.9k, ~60% — contains the
  literal drive/voltage lookup tables; these tables ARE the missing docs).
- DP/eDP: `intel_dp.c` (7.6k, ~55% — MST/dual-mode/infoframe droppable),
  `intel_dp_aux.c`, `intel_dp_link_training.c` (2.8k, ~85%),
  `intel_dp_link_caps.c`, `intel_pps.c` (eDP power sequencing, ~90%),
  `intel_panel.c`, `intel_backlight.c` (~85%; AUX-backlight variant
  `intel_dp_aux_backlight.c` conditional on T590 panel), `intel_dpcd.h`.
- pipes/planes/WM: `skl_watermark.c` (4.1k, ~80% — gen9 uses skl WM;
  `i9xx_wm.c` 4.2k dropped), `intel_bw.c` + `intel_link_bw.c` +
  `intel_dbuf_bw.c` (~70-80% — bandwidth check prevents underrun flicker),
  `skl_scaler.c` (needed for non-native modes), `intel_pfit.c` (panel
  fitter), `intel_dsb.c` dropped (DISPLAY_VER 13/14 only — verified).
- power/irq: `intel_display_power.c` + `_well.c` + `_map.c` (~80-85%),
  `intel_display_clock_gating.c`, `intel_display_irq.c` (~60%),
  `intel_vblank.c`, `intel_flipq.c`, `intel_fifo_underrun.c` (~90% —
  underrun recovery is load-bearing for stability), `intel_hotplug_irq.c`/
  `intel_hotplug.c`/`intel_dip.c`/`intel_load_detect.c` (~50-60% — HPD
  for DP detect; eDP itself is always-connected), `intel_gmbus.c` (~50% —
  eDP EDID normally via AUX/VBT; GMBUS only if a DDC path is needed),
  `intel_pch.c`, `intel_pch_display.c`, `intel_sbi.c` (conditional).
- VBT/BIOS: `intel_bios.c` (4.1k, ~80%) + `intel_vbt_defs.h` +
  `intel_rom.c` — parses the video BIOS tables: panel timings, port
  map, backlight, DPCD caps. Unavoidable; mostly gen-agnostic.
- dropped display features: PSR (4.7k), FBC (2.5k), HDCP, VDSC, VRR, DRRS,
  color pipeline, CASF/CMTG, DPT/DE/DSB/HTI, audio, fbdev, overlay,
  debugfs/snapshot/trace, all non-gen9 outputs (HDMI/SDVO/DVO/TV/CRT/LVDS/
  DSI/VLV/CHV/BXT/GLK) and all newer PHYs (DPIO/DKL/CX0/LT/SNPS/Combo/TC/
  LSPCON/MST).

**Memory/scanout** — the part with no clean boundary: display code calls
`i915_bo`/`i915_vma`/GEM objects everywhere. Minimum viable: object alloc
(shmem pages), tiling (`i915_gem_tiling.c` — X/Y tile + swizzle), GGTT PTE
programming (`i915_vma.c` 2.3k + `i915_gem_gtt.c` + `gem/i915_gem_region.c`
+ `intel_region_ttm.c` + `i915_ttm_buddy_manager.c`), mmap
(`i915_gem_mman.c`), dumb-buffer create (`i915_gem_create.c` +
`i915_gem.c` dumb path), wait/busy fences (`i915_gem_wait.c`,
`i915_gem_busy.c`), domain tracking (`i915_gem_domain.c`), clflush
(`i915_gem_clflush.c` — CPU-written scanout buffers need it), shrinker,
dmabuf (`i915_gem_dmabuf.c` — required later for real Wayland clients,
deferrable), plus `i915_active.c`/`i915_deps.c`/`i915_sw_fence*.c` (atomic
commit dependency tracking) and `i915_irq.c` (master IRQ dispatch —
display IRQs share the GT interrupt line).

**Userspace ABI the driver must serve** (what Hyprland/wlroots actually
issue — independent of i915 internals): ~20-25 DRM ioctls —
GETRESOURCES/GETCRTC/GETPLANE/GETCONNECTOR/GETFB, GETPROPERTIES/
GET_PROPERTY, CREATE_DUMB/MAP_DUMB/DESTROY_DUMB, ADDFB2 (or atomic-only
fbs), ATOMIC (+ ATOMIC_NONBLOCK), PAGE_FLIP, OBJ_GET_PROPERTIES,
PRIME fd↔handle, GET_CAP/GET_PARAM/VERSION, VBLANK event. This is the
real "driver surface" — the KMS object model + atomic commit engine +
these ioctls, NOT i915's struct layout.

## 3. Docs reality

- Intel gen9 display PRMs are **NDA**. There is no public register manual
  for KBL. The code IS the reference: `intel_display_regs.h` (3.3k lines
  of named registers) + 86 `Bspec`/`B-spec` citations across 41 display
  files pointing at section numbers that only exist under NDA.
- What IS public: VESA DisplayPort/eDP specs (link training, DPCD, AUX —
  `intel_dp_link_training.c` implements a public protocol), the Intel
  **VBT spec** (public PDF — `intel_bios.c` parses it), EDID spec.
- Self-documenting vs not: register *names* and many bitfields are
  self-describing in headers; the hard parts are the **algorithms** —
  DPLL/CDCLK clock-tree solve (`intel_dpll_mgr.c`: SSC, port-clock
  derivation, bypass rules — ~40% of a 5.4k file is gen9), DDI PHY
  drive-strength selection vs link rate (the `buf_trans` tables carry
  literal values — good), link-training retry/quirk paths, watermark/
  bandwidth correctness (wrong ⇒ flicker/underrun, not a clean error),
  power-well enable ordering (panel well vs DDI well vs transcoder).
- **Second reference exists and is small**: coreboot `libgfxinit`
  (cloned, `session/m6-i915/libgfxinit`) — SPARK/Ada, **30,478 LOC** total;
  `common/skylake/` (1,410 LOC platform delta) covers Kaby Lake/Coffee/
  Whiskey Lake on one code path (verified: coreboot gerrit KBL support,
  "only differences to Skylake in the DDI output buffer tuning"). It does
  boot-time modeset→static framebuffer (no atomic/pageflip/multi-buffer)
  with zero Linux dependencies — the ideal first milestone and a
  cross-check oracle for every init sequence i915 performs. GNAT
  (gcc-based) toolchain; port-by-hand to Zig/C is also viable at 30k LOC.
- Conditional unknowns for T590 specifically: panel backlight type
  (PWM vs AUX — `intel_backlight.c` vs `intel_dp_aux_backlight.c`),
  whether eDP EDID comes from VBT or AUX, eDP port letter (code is
  port-generic), and whether firmware leaves a framebuffer to inherit
  (`intel_initial_plane.c` path) or we init clean.

## 4. Feasibility verdict + roadmap

**Verdict: feasible, effort class "new DRM driver" — not "port a folder".**
The minimal subset is ~114k LOC of reference yielding an estimated
**20–35k LOC of new Koru-side code** (modeset engine + atomic/ioctl layer
+ register sequences + memory/GGTT + platform glue). That is large but not
falsifying: it is 1/4 of i915, the display half is the *tractable* half
(no scheduler, no command submission, no GuC/HuC firmware, no memory
compression), and two independent references exist.

Dependency order (what must exist before what):

```
PCI probe + MMIO map + IRQ + power wells          [platform]
        │
        ├── VBT parse + connector/encoder/crtc/plane objects   [needs platform]
        │
        ├── memory: dumb-buffer alloc + GGTT PTE + mmap        [needs platform]
        │
        └── atomic commit engine (state check → commit tail)   [needs objects+memory]
                    │
                    └── HW engines: transcoder/plane regs,
                        DPLL+CDCLK, DDI PHY/BUF, DP AUX+link train,
                        eDP PPS, panel, backlight, WM/BW, vblank/flip IRQ
```

Parallel workstreams (natural seams, ~5):

1. **platform/** — PCI, BAR/MMIO, uncore-lite, IRQ dispatch, power wells,
   runtime-PM-lite. Independent of everything.
2. **memory/** — page allocator for scanout, GGTT PTE/tiling, GEM-lite
   object, dumb-buffer ioctls, mmap. Independent of display engines.
3. **kms/** — crtc/plane/connector objects, atomic state + commit tail
   sequencing, vblank/pageflip. Depends on 1+2 interfaces only.
4. **clock/** — DPLL/CDCLK/PCH-refclk solve + program. Self-contained;
   hardest arithmetic.
5. **output/** — DDI/DP-AUX/link-train/eDP-PPS/panel/backlight/PFIT/WM/BW.
   Self-contained register work; splits further into link-training vs
   panel/power if needed.

VBT/EDID parsing (6th, small) feeds 3+5.

Honest effort estimate (agent-sessions, grounded in the measured corpus —
not "months"):

- Mapping/extraction of the ~76k relevant display LOC into per-function
  notes: this session did the *file-level* map; per-function read is
  ~2–4 sessions of dense reading.
- Implementation across 5 streams: each stream is 3–8k LOC of new code
  against a hard reference — est. **4–8 sessions per stream**, streams
  4–5 dominating.
- **Hardware bring-up on T590 is the wildcard**: serial/blank-screen/
  flicker/underrun debugging cycles on real silicon, no QEMU i915 to
  iterate against (QEMU's i915 is a stub — there is no hw-model for
  gen9 display). Budget ~30–50% overhead vs the code estimate.
- Total: **~20–40 sessions** for modeset+scanout+pageflip with a
  llvmpipe-backed compositor (Hyprland). dmabuf import for real Wayland
  clients adds ~2–4 sessions after.

Recommended milestone decomposition (each independently landable):

- **M0 static scanout** — port libgfxinit's KBL path (or i915's init
  sequence) to light the panel at boot with a firmware/simple-framebuffer
  style dumb buffer. Proves pipe/transcoder/plane/DDI/DP-init/backlight on
  real HW with ~zero KMS complexity. Cheapest possible de-risk of the
  hardest register work.
- **M1 dumb-KMS** — PCI probe → objects → one crtc/plane/connector →
  dumb create/map → legacy SETCRTC (or atomic) → first pixels via Koru.
- **M2 atomic** — full atomic commit + pageflip/vblank events → Hyprland
  can modeset (this is the commission's actual goal).
- **M3 dmabuf** — client buffer import → real compositor session.

Risks that could inflate the estimate: T590 panel/backlight quirks,
link-training retry behavior at 1080p/120Hz eDP, power-well sequencing
hangs (display hangs often need GT-side resets we won't have), and any
place where i915 silently depends on GT state (e.g., uncore forcewake,
GSB, PMIC). Mitigation: libgfxinit cross-check at every step; keep the
i915 commit-tail ordering verbatim; instrument with register readback
diffs vs a known-good Linux boot (T590 can dual-boot or kexec).

## Evidence / artifacts

- `session/m6-i915/linux` — sparse checkout of linux `d487237`
  (7.4-rc1), `drivers/gpu/drm/i915` + DRM core + UAPI headers.
- `session/m6-i915/libgfxinit` — coreboot libgfxinit HEAD (192 files,
  ~30.5k LOC Ada/SPARK).
- LOC figures: measured by `wc -l` over `*.c`/`*.h` in this session;
  per-file relevance fractions for monoliths are engineering estimates
  (flagged ±15%), file-level keep/drop sets are exact.
