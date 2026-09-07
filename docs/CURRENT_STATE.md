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

The verified remote baseline is `main` at `a5e1f09df740bb4ef6de17547d0f0e1874194617`; [post-merge Windows Release CI run #47](https://github.com/idkanonto/samplerthingidk/actions/runs/34144904944) passed. Gate C PR #13 passed [PR run #46](https://github.com/idkanonto/samplerthingidk/actions/runs/34140666576) at `08b4ffc638926610c45835bdf1a1187e4b61185f`. Its `recompiler-dll-Windows-VST3` artifact (`10025978696`) had GitHub SHA-256 `f6655154f4d40b91c4ab26e0460f25068fa86f6fa493a843c592b90c6aa9239e` and contained the workflow-verified 7,433,216-byte Windows module at `recompiler.dll.vst3/Contents/x86_64-win/recompiler.dll.vst3`.

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

## Verified Gate C implementation

- FRACTURE processes the global mix after SCRAMBLE. Five waveshaper transitions and six stable tonal structures cover soft/asymmetric/folded/clipped/digital character plus low, band, notch, formant, hollow-comb, and metallic-comb filter territory.
- FRACTURE Drive, Character, Filter Morph, Frequency, Resonance, and Mix are automatable. Mix defaults to 0% for a sample-identical fresh-instance bypass.
- Thirty compiled factory presets vary every FRACTURE dimension. Functional previous/dropdown/next controls publish the chosen preset into the real APVTS parameters; no preset parsing reaches the callback.
- SMEAR is a bounded two-head granular short-buffer texture stage with complementary windows and persistent blur state. Its half-second stereo storage is allocated in `prepareToPlay`; Amount 0 is transparent.
- CODEC combines deterministic bandwidth loss, held predictive residuals, and watery/metallic reconstruction with the preserved 1x–64x sample-and-hold Rate Reduction. The old `rateReduction` parameter ID remains stable but is presented and processed only inside CODEC.
- Gate C adds neutral migration defaults and state version 5. Its focused CTest suite and complete build/package workflow passed on both the PR head and squash-merged `main`.

## Gate D branch implementation

- SPECTRAL DRAW is inserted globally between FRACTURE and SMEAR. It uses a real 1024-point STFT, 256-sample hop, square-root Hann overlap-add, and reports 1024 samples of plug-in latency.
- A 128×64 attenuation-only canvas provides Draw, Erase, and Clear. Bilinear lookup smooths scanner/time and square-root frequency coordinates; Depth defaults to 0% for an exact latency-matched bypass.
- Scan Rate offers 2 beats, 1 bar, 2 bars, and 4 bars. The scanner aligns to finite host PPQ and BPM when available, otherwise continues from the safe tempo fallback.
- Canvas state is clamped, base64-encoded into state version 6, and transferred to the callback through fixed immutable snapshot slots. Audio rendering takes only an atomic read handle; it neither locks nor copies the canvas.
- Gate D tests cover publication immutability, hostile values, persistence, exact bypass latency, empty/full masks, STFT reconstruction, arbitrary block sizes, 44.1/48/96 kHz preparation, scanner wrap/alignment, discontinuity, and live canvas replacement. This branch remains unverified until its Windows CI and post-merge `main` gate pass.

## Not yet implemented

- Gate E: full-chain integration, compatibility/realtime audit, final functional UI cleanup, and shipping evidence.
- Final visual redesign is explicitly outside the current delivery boundary.

See [[TEST_MATRIX]] for what is runtime-verified and [[REALTIME_AUDIT]] for the callback contract.
