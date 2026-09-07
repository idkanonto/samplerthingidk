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
- Preserve global Target Key, optional MIDI pitch with root note, Random Start, Final Length, Attack, Release, Output, deterministic Seed, and fixed 16-voice POLY/MONO playback.
- The editor-selected source is independent of the most recently randomly triggered source.
- Prepared source audio is immutable. Stretch runs outside realtime; active voices retain their exact version; final reclamation is deferred to a non-realtime path.

## Global timing

- Global creative events use one shared grid: 1/8, 1/16 by default, or 1/32.
- Read BPM and PPQ from the host playhead once per audio block.
- When usable host timing is absent or transport is stopped, use a stable internal clock at the latest valid BPM or 120 BPM initially.
- Decisions must remain deterministic and bounded across tempo changes, seeks, loops, transport transitions, arbitrary block sizes, and offline rendering.

## Global creative chain

All creative processing occurs after the mixed sampler voices in this exact order:

1. FREEZE
2. SCRAMBLE
3. FRACTURE
4. SPECTRAL DRAW
5. SMEAR
6. CODEC
7. OUTPUT

Fresh instances load samples and play without enabling creative coloration.

### FREEZE

A click-safe, fixed/preallocated global captured-buffer effect. Chance is evaluated at grid boundaries. Size and Hold define capture/replay behavior. An optional octave flip resolves exactly once per activation to -12 or +12 semitones with equal probability.

### SCRAMBLE

A bounded global rolling/captured-chunk rearrangement effect. Grid activation uses Chance and Amount. Low settings remain controlled; high settings become strongly rearranged. Random decisions are deterministic for the same timing, seed, and parameters.

### FRACTURE

A global waveshaper followed by a morphing filter. User controls are Drive, Character, Filter Morph, Frequency, Resonance, and Mix. Provide 25–35 curated real presets with functional previous, dropdown, and next selection. Extremes must stay stable and finite.

### SPECTRAL DRAW

A real STFT/FFT overlap-add processor. The persistent canvas scans horizontally through time and maps vertically to frequency. User operations are Draw, Erase, Clear, Scan Rate, and Depth. Mask publication must be safe for realtime consumption. Report plug-in latency if required by the implementation.

### SMEAR

A global texture blur/grain/stretch-feel processor with minimal controls. It is transparent at zero.

### CODEC

A global low-bitrate/damaged-digital approximation with Codec Amount, Quality, and Rate Reduction. The existing 1x–64x sample-and-hold reducer belongs inside CODEC. Bit Crush is not part of the product.

## Removed systems and state compatibility

- Remove Take History completely.
- Remove the programmable Step Mask completely.
- Remove per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop completely.
- Remove Bit Crush completely.
- Old state must ignore their parameters and `STEP_MASK`/`TAKE_HISTORY` nodes without disturbing surviving parameters or sources.
- Persist all surviving parameters, source settings, new global-effect parameters, and the Spectral Draw canvas.

## Delivery boundary

Ship functionality through Gates A–E. Do not begin the final visual redesign. Each gate batches code, focused tests, documentation, one meaningful Windows CI pass, review disposition, artifact inspection when packaging changes, squash merge, and post-merge `main` verification.
