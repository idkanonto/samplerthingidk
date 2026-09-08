---
title: recompiler.dll Product Specification
aliases:
  - Product Spec
tags:
  - product
  - specification
status: approved
---

# recompiler.dll Product Specification

This specification supersedes the earlier Random Chop Sampler V2 feature plan. Implementation status belongs in [[CURRENT_STATE]].

## Identity and sampler core

- The host-visible product is `recompiler.dll`, packaged as a valid VST3 bundle.
- Load at most 20 WAV, AIFF/AIF, MP3, or FLAC sources.
- Each MIDI Note On selects an enabled, non-missing source by Weight and chooses a legal random start inside its manual Start/End region.
- Source controls are enabled state, Start, End, Source Key, Transpose, Fine Tune, pitch-preserving Stretch, Gain, and Weight.
- Stretch uses `0 = OFF/original`; `1x` is also original; active duration multipliers are `>1x` through `4x`. Legacy values below 1x restore as original.
- Preserve global Target Key, optional MIDI pitch with root note, Start Range, Final Length, Attack, Release, Output, and fixed 16-voice POLY/MONO playback.
- Random source selection and a legal random start are core instrument behavior. Start Range controls how far the start may travel; there is no separate enable switch.
- A persisted internal random seed keeps a restored session coherent without exposing a technical Seed control in the producer workflow.
- The editor-selected source is independent of the most recently randomly triggered source.
- Prepared source audio is immutable. Stretch runs outside realtime; active voices retain their exact version; final reclamation is deferred to a non-realtime path.

## Global timing

- Global creative events use one shared grid: 1/8, 1/16 by default, or 1/32.
- Read BPM and PPQ from the host playhead once per audio block.
- When usable host timing is absent or transport is stopped, use a stable internal clock at the latest valid BPM or 120 BPM initially.
- Decisions must remain deterministic and bounded across tempo changes, seeks, loops, transport transitions, arbitrary block sizes, and offline rendering.

## Control audit

| Surface | Classification | Reason |
|---|---|---|
| Add, Clear, Enable All, Disable All, per-source On/Remove, and row selection | Essential | Direct sample-pool management and editing target. |
| Waveform Start/End | Essential | Defines the playable source region and legal random-start range. |
| Source Key, Transpose, Fine Tune, Gain, Weight, Stretch | Essential | Source preparation, harmonic placement, balance, selection probability, and duration. |
| Target Key, MIDI Pitch, Root MIDI Note | Essential | Global/manual harmonic behavior and keyboard tracking. |
| POLY/MONO, Global Grid | Essential | Voice policy and the musical timing framework used by SCRAMBLE/Spectral Draw. |
| Start Range, Final Length, Attack, Release, Output | Essential | Immediate performance shape without exposing implementation details. |
| SCRAMBLE | Macro | Collapses density, selection, repeats, holds, reverse, jumps, pitch, and event span into one perceptual progression. |
| FRACTURE | Macro | Collapses drive, wetness, modulation depth, filter/resonator motion, resonance, and digital damage into one progression. |
| CHARACTER | Macro | Selects a broad nonlinear/tonal/digital personality without exposing a modulation matrix. |
| FRACTURE RATE | Essential | Keeps the musically distinctive 1x–64x rate-reduction choice while FRACTURE controls its effective severity. |
| Fracture preset previous/dropdown/next | Essential | Fast access to 30 curated personalities; preset parsing remains outside realtime. |
| Spectral Draw/Erase/Clear, canvas, Depth, Scan Rate | Essential | A distinct intentional spectral role that does not duplicate the signature macros. |
| SMEAR | Macro | Collapses grain density, length, pitch range, scatter, stereo behavior, brightness, and wetness into one control. |

Internal-only values include the persisted creative seed and every technical probability, buffer size, modulation phase/depth, cutoff, resonance, predictor, grain, and transient-suppression setting.

## Global creative chain

All creative processing occurs after the mixed sampler voices in this exact order:

1. SCRAMBLE
2. FRACTURE
3. SPECTRAL DRAW
4. SMEAR
5. OUTPUT

Fresh instances load samples and play without enabling creative coloration.

### SCRAMBLE

A signature progressive-chaos macro over a bounded rolling buffer. Every eligible grid has a deterministic amount-derived manipulation budget, so randomness chooses the gesture rather than deciding whether the effect exists. Low values touch a small part of each phrase; medium values produce clearly rearranged rhythmic material; high values increase density, pitch/reverse/hold activity, and event span. Freeze-style holds, micro-loops, and octave fragments are internal SCRAMBLE gestures rather than separate effects or controls.

### FRACTURE

A signature self-moving destruction macro. FRACTURE couples nonlinear shaping, stable filter/formant/comb structures, input-envelope response, correlated slow motion, controlled random drift, predictive digital damage, and sample-rate reduction. The user controls FRACTURE amount, broad CHARACTER, and FRACTURE RATE; technical drive, cutoff, resonance, morph, and codec settings are internal. Provide 30 curated presets with functional previous, dropdown, and next selection. Increasing the macro must increase both transformation and movement while remaining finite, DC-controlled, and musically legible.

### SPECTRAL DRAW

A real STFT/FFT overlap-add processor. The persistent canvas scans horizontally through time and maps vertically to frequency. User operations are Draw, Erase, Clear, Scan Rate, and Depth. Mask publication must be safe for realtime consumption. Report plug-in latency if required by the implementation.

### SMEAR

A crystalline pitched-grain cloud, not a blur or reverb substitute. Fixed preallocated grains read safely behind the write head, use tapered windows, scatter, musically selected pitch intervals, stereo placement, bright residual emphasis, and transient-aware wet suppression. The single SMEAR macro moves from subtle texture to an obvious icy fragmented layer and is sample-identical at zero.

## Removed systems and state compatibility

- Remove Take History completely.
- Remove the programmable Step Mask completely.
- Remove per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop completely.
- Remove Bit Crush completely.
- Remove FREEZE as a separate stage and fold its useful repeat/hold/octave behavior into SCRAMBLE.
- Remove CODEC as a separate stage and fold its useful predictive damage and Rate Reduction into FRACTURE.
- Remove the exposed Seed, SCRAMBLE Chance, Freeze Size/Hold/Chance/Octave Chance, FRACTURE Drive/Filter Morph/Frequency/Resonance, and CODEC Amount/Quality controls.
- Old state must ignore their parameters and `STEP_MASK`/`TAKE_HISTORY` nodes without disturbing surviving parameters or sources.
- Migrate useful old Freeze/Scramble/Fracture/Codec intensity into the new macros, then ignore removed entries without disturbing surviving parameters or sources.
- Persist all surviving parameters, the internal creative seed, source settings, and the Spectral Draw canvas.

## Delivery boundary

This creative-quality pass changes sound design and control semantics without adding unrelated features or beginning a visual-art-direction redesign. Completion requires Windows compilation/CTest, deterministic 0/25/50/75/100 listening renders, artifact inspection, review disposition, DAW listening guidance, and a green post-merge `main`.
