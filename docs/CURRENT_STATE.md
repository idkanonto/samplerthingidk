---
title: Current Implementation State
aliases:
  - Current State
tags:
  - implementation
  - current-state
status: active
verified: 2026-09-07
---

# Current Implementation State

The last verified remote baseline is `main` at `85f970522b5a8416b0a58268103e224dba7f5ea6`; [Windows Release CI run #41](https://github.com/idkanonto/samplerthingidk/actions/runs/33950109493) passed. Gate A is implemented on `gate/a-cleanup-foundation` but remains unverified until its Windows CI, artifact, merge, and post-merge gate complete. Do not present branch-only work as shipped.

## Gate A branch implementation

- Visible product name and expected deliverables are `recompiler.dll`, `recompiler.dll.vst3`, and `recompiler.dll.exe`. The CMake target, bundle ID, manufacturer code, and plug-in code remain stable.
- The sampler core remains: 20-source immutable pool; WAV/AIFF/AIF/MP3/FLAC; enabled/missing handling; Weight; Gain; stable IDs; Start/End and random legal starts; Source/Target keys; Transpose; Fine Tune; MIDI pitch/root; fixed 16-voice POLY/MONO; Final Length; Attack/Release; persistence; and deferred non-realtime reclamation.
- Source Stretch now stores `0` as OFF/original, treats `1x` as original, and permits extension through `4x`. Legacy values below `1x` restore as original. Signalsmith preparation remains on the background worker.
- A shared fixed-capacity Host Grid derives 1/8, 1/16-default, or 1/32 sample offsets from block-start BPM/PPQ. It detects discontinuities and falls back to the latest valid BPM or 120 BPM without allocation.
- Take History, Step Mask, and per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop were removed from the trigger, voice, UI, build, and tests.
- Bit Crush was removed. Rate Reduction remains temporarily functional before Output and moves inside CODEC in Gate C.
- State version 3 removes obsolete parameter children and legacy Step/Take nodes before restoring surviving APVTS state. Source state remains separate and preserved.
- The selected editor source remains keyed by stable source ID; random trigger highlighting does not change the editor selection.
- Gate A tests cover pool/state, weighted selection, pitch, regions, voice/envelope/polyphony, rate reduction, grid behavior, migration, and stretch semantics/publication.

## Not yet implemented

- Gate B: global FREEZE and SCRAMBLE.
- Gate C: global FRACTURE presets, SMEAR, and CODEC integration.
- Gate D: real STFT overlap-add SPECTRAL DRAW and persistent canvas.
- Gate E: full-chain integration, compatibility/realtime audit, final functional UI cleanup, and shipping evidence.
- Final visual redesign is explicitly outside the current delivery boundary.

See [[TEST_MATRIX]] for what is runtime-verified and [[REALTIME_AUDIT]] for the callback contract.
