---
title: Product Decisions
tags:
  - product
  - decisions
status: active
---

# Product Decisions

These decisions govern V2 together with [[PRODUCT_SPEC_V2]]. New reversals must be recorded here before implementation.

## Accepted

- Sampler-first product; maximum 20 samples; WAV, AIFF/AIF, MP3, and FLAC.
- Random enabled source per MIDI trigger and random start within manual Start/End.
- Manual Source Key, global Target Key, manual per-sample Transpose, and mandatory Fine Tune.
- Per-sample pitch-preserving Stretch, Gain, and Weight.
- Signalsmith Stretch is the preferred MIT-licensed stretch implementation; preparation is cached and kept off the realtime thread.
- MIDI pitch ON/OFF with a root note; 16 voices; Mono and Poly modes.
- Chance effects: Reverse, Retrigger, Skip, Reorder, Bend, and Drop.
- Step Mask lengths: 2, 4, 8, 16. Advance only on MIDI Note On. NORMAL bypasses Chance; FX allows Chance.
- One Take equals one complete Step Mask cycle. Retain the latest 8 Takes.
- History stores explicit decisions rather than rendered audio. LIVE generates; HISTORY replays deterministically.
- Final Length follows Chance processing. Attack/Release follow Final Length.
- Global Bit Crush, Sample Rate Reduction, and Output Gain use the fixed [[SIGNAL_CHAIN]].
- Automatic Source Key to Target Key correction uses the shortest signed tonic interval; a six-semitone tie resolves upward.
- Stretch is expressed as a duration multiplier: 2.0x produces twice the duration and 0.5x produces half the duration while preserving pitch.
- Reorder begins at the event's resolved random-start position, stays inside that source's Start/End region, and uses at most one second divided into four pieces.
- HISTORY loops the selected Take indefinitely and resets to its first event when another Take is selected.
- V2 phases ship through sequential pull requests. Each requires passing Windows CI, an inspected VST3 artifact, and disposition of CodeRabbit findings before squash merge.

## Rejected for V2

- Waveform magnifier.
- BPM detection and host tempo sync.
- Grid-based random starts.
- DAW bar or loop synchronization.

Do not resurrect these without an explicit decision change.

## Postponed

- Automatic key detection.
- Automatic detune/cents detection.
- Design and art direction until functionality is stable.

Postponed work is tracked in [[FUTURE_IDEAS]].
