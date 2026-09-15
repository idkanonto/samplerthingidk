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
- Each MIDI Note On selects an enabled, non-missing source with equal probability and chooses a legal random start inside its manual Start/End region.
- Source controls are enabled state, Start, End, Source Key, Transpose, Fine Tune, and Gain.
- Preserve global Play In Key, a Chords toggle, Output, and fixed 16-voice POLY/MONO playback. Chords off keeps triggers in Play In Key; Chords on follows incoming MIDI notes relative to an automatically derived central root.
- Random source selection and a legal random start across the manual source region are core instrument behavior. Click-safe attack/release are bounded implementation details rather than public controls.
- A persisted internal random seed keeps a restored session coherent without exposing a technical Seed control in the producer workflow.
- The editor-selected source is independent of the most recently randomly triggered source.
- Prepared source audio is immutable; active voices retain their exact version and final reclamation is deferred to a non-realtime path.

## Global timing

- Global creative events use one hidden shared grid selected automatically from 1/8, 1/16, or 1/32 to keep slices near 125 ms while remaining tempo-synchronized.
- Read BPM and PPQ from the host playhead once per audio block.
- When usable host timing is absent or transport is stopped, use a stable internal clock at the latest valid BPM or 120 BPM initially.
- Decisions must remain deterministic and bounded across tempo changes, seeks, loops, transport transitions, arbitrary block sizes, and offline rendering.

## Control audit

| Surface | Classification | Reason |
|---|---|---|
| Drag-and-drop, per-source On/Remove, and row selection | Essential | Minimal sample-pool management and editing target. |
| Waveform Start/End | Essential | Defines the playable source region and legal random-start range. |
| Source Key, Transpose, Fine Tune, Gain | Essential | Source preparation, harmonic placement, correction, and balance. |
| Play In Key, Chords | Essential | Straightforward harmonic normalization and optional keyboard tracking. |
| POLY/MONO | Essential | Voice overlap policy presented as one two-position switch. |
| Output | Essential | Final gain with a short sample-time ramp for automation safety. |
| SCRAMBLE | Macro | Collapses density, selection, repeats, holds, reverse, jumps, pitch, and event span into one perceptual progression. |
| MELT | Macro | Controls automatic slice selection, pitch-preserving stretch depth, slice density, internally derived reversal probability, and wetness through one progression. |
| Spectral canvas, Reset, Depth | Essential | Dragging draws; timing is automatic and one Reset action clears the canvas. |
| SMEAR | Macro | Collapses grain density, length, pitch range, scatter, stereo behavior, brightness, and wetness into one control. |

Internal-only values include the persisted creative seed and every technical probability, buffer size, fade time, modulation phase/depth, cutoff, resonance, grain, filtered-history, and transient-suppression setting.

## Global creative chain

All creative processing occurs after the mixed sampler voices in this exact order:

1. SCRAMBLE
2. MELT
3. SPECTRAL DRAW
4. SMEAR
5. OUTPUT

Fresh instances load samples and play without enabling creative coloration.

### SCRAMBLE

A signature progressive-chaos macro over a bounded rolling buffer. A rising macro edge arms the effect and leaves audio dry until the next shared-grid boundary. Every eligible boundary has a deterministic amount-derived manipulation budget, so randomness chooses the gesture rather than deciding whether the effect exists. One-grid gestures end on enumerated musical boundaries rather than rounded sample counts. Low values touch a small part of each phrase; medium values produce clearly rearranged rhythmic material; high values increase density, pitch/reverse/hold activity, and event span. Freeze-style holds, micro-loops, seam-blended subregions, and octave fragments are internal SCRAMBLE gestures rather than separate effects or controls. Disable transitions use a short bounded fade before exact bypass.

### MELT

A focused one-knob automatic slice stretcher. A rising MELT edge arms the effect and leaves audio dry until the next shared-grid boundary. MELT captures the preceding grid interval, divides the next interval into two through four automatically selected slices, and renders each through fixed-overlap Hann grains. Each grain reads at normal sample speed while the analysis hop advances more slowly than the synthesis hop, extending time without Scramble-style octave resampling. MELT Amount increases wetness, slice density, grain duration, a bounded `1.08x` through `4x` stretch range, and an internally derived chance that each slice reverses without changing its stretch ratio. Energy-aware overlap normalization, six-millisecond equal-power slice-edge tapers, and an eight-millisecond bypass transition control level and seams; settled zero is sample-identical. Transport discontinuities and grid edits invalidate logical history without clearing or reallocating the prepared ring.

### SPECTRAL DRAW

A real STFT/FFT overlap-add processor. The persistent canvas scans horizontally through time and maps vertically to frequency. Dragging draws, Reset clears the canvas, and Depth controls intensity. A hidden tempo-derived 2-beat/1/2/4-bar choice keeps the scan near two seconds. Mask publication must be safe for realtime consumption. Report plug-in latency if required by the implementation.

### SMEAR

A crystalline pitched-grain cloud, not a blur or reverb substitute. A fixed preallocated pool reads safely behind the write head, uses tapered windows, scatter, musically selected pitch intervals, lifetime-relative pitch/pan orbits, stereo placement, bright residual emphasis, and transient-aware wet suppression. The single SMEAR macro progressively raises the target overlap while introducing a broader population of shorter grains, moving from a few long fragments to a crowded field of fine icy particles. Faster readers select preallocated progressively filtered history, and overlap energy is smoothed before gain normalization. Disable transitions fade briefly and settled zero is sample-identical.

## Removed systems and state compatibility

- Remove Take History completely.
- Remove the programmable Step Mask completely.
- Remove per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop completely.
- Remove Bit Crush completely.
- Remove FREEZE as a separate stage and fold its useful repeat/hold/octave behavior into SCRAMBLE.
- Remove CODEC and FRACTURE as separate stages, including their predictive-damage, rate-reduction, distortion, filter-morph, and modulation paths.
- Remove Start Range, Final Length, public Attack/Release, FRACTURE RATE, and the FRACTURE preset browser.
- Remove the exposed Seed, SCRAMBLE Chance, Freeze Size/Hold/Chance/Octave Chance, every FRACTURE control, and CODEC Amount/Quality controls. `fractureCharacter` and `fractureMix` are retired rather than reinterpreted as MELT.
- Remove the exposed MELT Reverse Chance. `meltReverseChance` is retired rather than mapped onto the new one-knob macro; reversal remains an internal amount-derived slice decision.
- Remove Selection Weight and manual per-source Stretch. Old source properties are ignored rather than reinterpreted.
- Remove Add Samples, Clear All, Enable All, and Disable All; loading is drag-and-drop and source actions are individual.
- Remove Root MIDI Note, Global Grid, and Spectral Scan Rate. Retire those parameter IDs rather than mapping old values onto surviving controls.
- Remove separate Spectral Draw/Erase modes and keep only direct drawing plus Reset.
- Do not add a Loop/One-shot mode.
- Old state must ignore their parameters and `STEP_MASK`/`TAKE_HISTORY` nodes without disturbing surviving parameters or sources.
- Migrate useful old Freeze behavior into SCRAMBLE, then ignore removed Fracture/Codec entries without disturbing surviving parameters or sources. MELT starts neutral when an older session is restored because it is a different sound and contract.
- Persist all surviving parameters, the internal creative seed, source settings, and the Spectral Draw canvas.

## Delivery boundary

This pass simplifies control semantics without adding unrelated features. Future passing Windows builds ship the raw complete VST3 bundle rather than a new unsigned installer executable. Completion requires Windows compilation/CTest, deterministic listening renders, fixed-latency and boundary-partition checks, artifact inspection, DAW listening guidance, and a green post-merge `main`.
