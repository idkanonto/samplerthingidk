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

The verified remote baseline is `main` at `4f2f31fc54895c5ba44366b095d80ea8e310c6b8`; [post-merge Windows Release CI run #45](https://github.com/idkanonto/samplerthingidk/actions/runs/34084650160) passed. Gate B PR #12 passed [PR run #44](https://github.com/idkanonto/samplerthingidk/actions/runs/34084189109). Its workflow-verified `recompiler-dll-Windows-VST3` artifact (`10004759928`) had GitHub SHA-256 `495ddd0d7e5f6591891afcdfaa1bd553fa65a38c99e8f3bd101948418bfa0462` and contained the 7,400,448-byte module at `recompiler.dll.vst3/Contents/x86_64-win/recompiler.dll.vst3`.

## Verified Gate A implementation

- Visible product name and expected deliverables are `recompiler.dll`, `recompiler.dll.vst3`, and `recompiler.dll.exe`. The CMake target, bundle ID, manufacturer code, and plug-in code remain stable.
- The sampler core remains: 20-source immutable pool; WAV/AIFF/AIF/MP3/FLAC; enabled/missing handling; Weight; Gain; stable IDs; Start/End and random legal starts; Source/Target keys; Transpose; Fine Tune; MIDI pitch/root; fixed 16-voice POLY/MONO; Final Length; Attack/Release; persistence; and deferred non-realtime reclamation.
- Source Stretch now stores `0` as OFF/original, treats `1x` as original, and permits extension through `4x`. Legacy values below `1x` restore as original. Signalsmith preparation remains on the background worker.
- A shared fixed-capacity Host Grid derives 1/8, 1/16-default, or 1/32 sample offsets from block-start BPM/PPQ. It detects discontinuities and falls back to the latest valid BPM or 120 BPM without allocation.
- Take History, Step Mask, and per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop were removed from the trigger, voice, UI, build, and tests.
- Bit Crush was removed. Gate A temporarily left Rate Reduction before Output; Gate C now places it inside CODEC while retaining its parameter ID and sound.
- State version 4 removes obsolete root properties/parameter children and legacy Step/Take nodes before restoring surviving APVTS state. Source state remains separate and preserved; absent temporal controls receive neutral defaults.
- The selected editor source remains keyed by stable source ID; random trigger highlighting does not change the editor selection.
- Gate A tests cover pool/state, weighted selection, pitch, regions, voice/envelope/polyphony, rate reduction, grid behavior, migration, and stretch semantics/publication.

## Verified Gate B implementation

- FREEZE and SCRAMBLE process the combined mixed voices before later global stages.
- Both use two-second preallocated stereo rolling histories and reference captured ring ranges in place. Activation performs no heap allocation or large capture copy.
- FREEZE rolls only on shared grid boundaries, captures 1/4, 1/2, or one grid unit, and holds for 1, 2, 4, or 8 grid units. A 3 ms dry/wet edge fade bounds activation/release clicks.
- Freeze Octave Chance resolves once per event. The only pitch increments are 0.5 (-12 semitones), 1.0 (no flip), and 2.0 (+12 semitones); direction uses an equal fixed-seed branch.
- SCRAMBLE rolls only on the shared grid, captures at most one grid unit, divides it into at most eight chunks, and resolves bounded source-chunk/reverse mappings once per activation. Amount controls the probability of manipulating each chunk. Two-millisecond dry windows soften chunk/event edges.
- Each effect owns a separately salted deterministic RNG, so enabling temporal effects does not change seeded sampler-source selection.
- Transport discontinuities invalidate active capture/history in constant time; old ring contents are ignored without clearing a large buffer in the callback.
- Chance defaults are 0%, so fresh instances remain load-samples-and-play neutral.
- Gate B CTest coverage passed on both the PR head and squash-merged `main`.

## Gate C branch implementation

- FRACTURE processes the global mix after SCRAMBLE. Five waveshaper transitions and six stable tonal structures cover soft/asymmetric/folded/clipped/digital character plus low, band, notch, formant, hollow-comb, and metallic-comb filter territory.
- FRACTURE Drive, Character, Filter Morph, Frequency, Resonance, and Mix are automatable. Mix defaults to 0% for a sample-identical fresh-instance bypass.
- Thirty compiled factory presets vary every FRACTURE dimension. Functional previous/dropdown/next controls publish the chosen preset into the real APVTS parameters; no preset parsing reaches the callback.
- SMEAR is a bounded two-head granular short-buffer texture stage with complementary windows and persistent blur state. Its half-second stereo storage is allocated in `prepareToPlay`; Amount 0 is transparent.
- CODEC combines deterministic bandwidth loss, held predictive residuals, and watery/metallic reconstruction with the preserved 1x–64x sample-and-hold Rate Reduction. The old `rateReduction` parameter ID remains stable but is presented and processed only inside CODEC.
- Gate C adds neutral migration defaults and state version 5. Its new DSP and UI remain unverified until Gate C Windows CI and post-merge `main` pass.

## Not yet implemented

- Gate D: real STFT overlap-add SPECTRAL DRAW and persistent canvas.
- Gate E: full-chain integration, compatibility/realtime audit, final functional UI cleanup, and shipping evidence.
- Final visual redesign is explicitly outside the current delivery boundary.

See [[TEST_MATRIX]] for what is runtime-verified and [[REALTIME_AUDIT]] for the callback contract.
