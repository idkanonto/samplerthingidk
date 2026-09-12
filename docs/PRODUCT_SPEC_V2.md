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
- Preserve global Target Key, optional MIDI pitch with musically named root note, Output, and fixed 16-voice POLY/MONO playback.
- Random source selection and a legal random start across the manual source region are core instrument behavior. Click-safe attack/release are bounded implementation details rather than public controls.
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
| Output | Essential | Final gain with a short sample-time ramp for automation safety. |
| SCRAMBLE | Macro | Collapses density, selection, repeats, holds, reverse, jumps, pitch, and event span into one perceptual progression. |
| FRACTURE | Macro | Controls distortion intensity, wetness, resonance, and bounded autonomous motion through one progression. |
| FILTER MORPH (`fractureCharacter`) | Macro | Retains the stable parameter ID while continuously selecting low, band, notch, and high response territory. |
| Spectral Draw/Erase/Clear, canvas, Depth, Scan Rate | Essential | A distinct intentional spectral role that does not duplicate the signature macros. |
| SMEAR | Macro | Collapses grain density, length, pitch range, scatter, stereo behavior, brightness, and wetness into one control. |

Internal-only values include the persisted creative seed and every technical probability, buffer size, fade time, modulation phase/depth, cutoff, resonance, grain, filtered-history, and transient-suppression setting.

## Global creative chain

All creative processing occurs after the mixed sampler voices in this exact order:

1. SCRAMBLE
2. FRACTURE
3. SPECTRAL DRAW
4. SMEAR
5. OUTPUT

Fresh instances load samples and play without enabling creative coloration.

### SCRAMBLE

A signature progressive-chaos macro over a bounded rolling buffer. A rising macro edge arms the effect and leaves audio dry until the next shared-grid boundary. Every eligible boundary has a deterministic amount-derived manipulation budget, so randomness chooses the gesture rather than deciding whether the effect exists. One-grid gestures end on enumerated musical boundaries rather than rounded sample counts. Low values touch a small part of each phrase; medium values produce clearly rearranged rhythmic material; high values increase density, pitch/reverse/hold activity, and event span. Freeze-style holds, micro-loops, seam-blended subregions, and octave fragments are internal SCRAMBLE gestures rather than separate effects or controls. Disable transitions use a short bounded fade before exact bypass.

### FRACTURE

A focused self-moving destruction macro. FRACTURE uses true 2x oversampling around the nonlinear and filtering path, a continuously morphed state-variable response, input-envelope response, correlated slow motion, controlled random drift, DC blocking, and bounded output saturation. FRACTURE Amount controls the distortion progression while FILTER MORPH controls low-to-band-to-notch-to-high tonal position, keeping the two public axes perceptually distinct. The latency-compensated dry path remains aligned and plug-in latency remains constant through bypass automation. Increasing Amount must increase both transformation and movement while remaining finite, DC-controlled, and musically legible.

### SPECTRAL DRAW

A real STFT/FFT overlap-add processor. The persistent canvas scans horizontally through time and maps vertically to frequency. User operations are Draw, Erase, Clear, Scan Rate, and Depth. Mask publication must be safe for realtime consumption. Report plug-in latency if required by the implementation.

### SMEAR

A crystalline pitched-grain cloud, not a blur or reverb substitute. Fixed preallocated grains read safely behind the write head, use tapered windows, scatter, musically selected pitch intervals, lifetime-relative pitch/pan orbits, stereo placement, bright residual emphasis, and transient-aware wet suppression. Faster readers select preallocated progressively filtered history, and overlap energy is smoothed before gain normalization. The single SMEAR macro moves from subtle texture to an obvious icy fragmented layer; disable transitions fade briefly and settled zero is sample-identical.

## Removed systems and state compatibility

- Remove Take History completely.
- Remove the programmable Step Mask completely.
- Remove per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop completely.
- Remove Bit Crush completely.
- Remove FREEZE as a separate stage and fold its useful repeat/hold/octave behavior into SCRAMBLE.
- Remove CODEC as a separate stage. The focused FRACTURE redesign also removes the inherited predictive-damage and Rate Reduction sub-effects.
- Remove Start Range, Final Length, public Attack/Release, FRACTURE RATE, and the FRACTURE preset browser.
- Remove the exposed Seed, SCRAMBLE Chance, Freeze Size/Hold/Chance/Octave Chance, FRACTURE Drive/Frequency/Resonance, and CODEC Amount/Quality controls. Keep the stable `fractureCharacter` parameter ID and present it as FILTER MORPH.
- Old state must ignore their parameters and `STEP_MASK`/`TAKE_HISTORY` nodes without disturbing surviving parameters or sources.
- Migrate useful old Freeze/Scramble/Fracture/Codec intensity into the new macros, then ignore removed entries without disturbing surviving parameters or sources.
- Persist all surviving parameters, the internal creative seed, source settings, and the Spectral Draw canvas.

## Delivery boundary

This creative-quality pass changes sound design and control semantics without adding unrelated features or beginning a visual-art-direction redesign. Completion requires Windows compilation/CTest, deterministic listening renders, fixed-latency and boundary-partition checks, artifact inspection, review disposition, DAW listening guidance, and a green post-merge `main`.
