---
name: audio-hda
iface: koruos/audio.pcm
arms: [unikraft/*]
lane: donor-c
donor: unikraft-fork
needs: [pci-enumeration]
accept: accept.sh — guest opens PCM capture+playback; loopback pattern verified
tier: qemu
status: landed
home: this-repo
donor: unikraft-fork
evidence: D
license: BSD-3-Clause (own driver code over unikraft-staging BSD-3 — no copyleft, no blobs)
provenance: hda.c written from Intel HDA spec 097a0384 (register/verb vocabulary is the spec’s); unikraft-staging 3fdffba8 in origins.lock; pci cfg32 helper pattern follows in-repo xhci.c (BSD-3)
---

# audio-hda

Audio: Intel HDA (or AC97) controller + codec, PCM capture/playback.
**Nothing exists** — no sound driver anywhere in the Unikraft tree or
lib-* catalog (verified). On the gap list because every desktop needs
it: notification sounds, voice (omni's mic grammar), media.

## Acceptance

`accept.sh` boots with `-device intel-hda -device hda-duplex` plus a
QEMU audiodev backend (`-audiodev none,id=snd0 -device
hda-duplex,audiodev=snd0` for headless, or wav capture backend so the
host gets a real file). Guest driver enumerates the codec, opens a PCM
stream, and runs a loopback/tone test. Host-side oracle: the captured
`.wav` (or the guest's own injected-then-read-back pattern over a
virtual loopback cable) contains the expected signal — a spectrogram/
energy check, not just "opened successfully."

## Notes

- QEMU audio is the fiddliest part: `-audiodev wav` gives a host file —
  the honest headless oracle; `none` backend still exercises the guest
  driver but can't verify audio content.
- HDA is a command-ring + codec-widget architecture (CORB/RIRB,
  widget routing) — substantially more complex than a blob device;
  budget the claim accordingly. AC97 (`-device AC97`) is the simpler
  fallback device if HDA stalls — entry can note a lane decision.
- tier:hw rung: real laptops are mostly HDA-compatible (Intel/Realtek
  codecs) — the codec layer is where hw variance lives.
