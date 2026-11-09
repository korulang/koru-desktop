# Commission: i915 feasibility analysis — minimal display-only driver for Kaby Lake-R

## Mission

Produce a REAL feasibility assessment for driving the ThinkPad T590's
Intel iGPU-630 (Kaby Lake-R, Gen9.5, eDP panel) from Koru — not a guess.
Clone the Linux i915 driver, map its composition, isolate the minimal
subset needed for modeset + dumb-buffer scanout, and deliver a roadmap
with an honest effort estimate and parallelization seams.

## Why this exists

Hyprland renders via llvmpipe (software GL) — it needs ONLY the display
engine: pipe/transcoder/plane programming, DPLL clock setup, DDI/DP/eDP
PHY init, panel backlight, and dumb-buffer scanout. It does NOT need the
3D/compute engine, Guc submission, or memory manager beyond scanout
buffers. The question is whether that display-only floor is small enough
to be tractable — answer it with the actual code, not estimates.

i915 is ~300k+ lines across drivers/gpu/drm/i915/, deeply coupled to
Linux DRM/GEM/scheduler/IRQ/firmware. It is NOT portable as a unit — the
value is in extracting the minimal display path, not porting the driver.

## What to produce

1. **Composition map** — clone linux `drivers/gpu/drm/i915/` (shallow is
   fine) and break it into: GT/compute engine, display/KMS engine,
   memory management (GGTT/GEM/TTM), platform glue (PCI, IRQ, power),
   firmware. Line counts + file lists per region. Show where the display
   engine actually lives (display/, dpll/, psr/, dp/, ddi/, transcoder,
   plane code) vs the rest.

2. **The Kaby Lake-R floor** — identify exactly which files/functions a
   Gen9.5 eDP modeset+scanout needs: GT power well, CDCLK/DPLL, transcoder
   + plane config, DDI buffer/PHY lane setup for eDP, panel/backlight.
   Name them; estimate the residual line count AFTER deleting everything
   else. Be honest if the floor is still thousands of lines of
   register-level work.

3. **Docs reality** — Intel display register docs are largely NDA; the
   kernel code IS the reference. Note which register sequences are
   self-documenting in i915 vs which need external docs. Flag the
   hardest-to-reverse-engineer pieces.

4. **Feasibility verdict + roadmap** — is a minimal display driver
   tractable for a small agent-driven team? Give: the dependency order
   (what must exist before what), natural parallel workstreams (display
   engine vs platform glue vs memory), and an honest effort estimate in
   agent-hours/session-count, NOT "months" hand-waving — ground it in
   the actual code volume you measured.

## Constraints

- Work in a scratch/analysis dir — this is research, not a build. Do NOT
  modify ~/src/unikraft, ~/src/koru, or any live tree.
- llvmpipe is the renderer — do NOT scope 3D/compute. Display engine only.
- If the honest answer is "infeasible / still too big," SAY SO with the
  numbers — a truthful no is a valid deliverable.

## Report back

Composition map, the KBL-R minimal subset (named files/functions +
residual LOC), docs gaps, and the feasibility verdict with the roadmap.
