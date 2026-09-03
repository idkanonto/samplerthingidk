---
title: Product Specification V2
aliases:
  - V2 Spec
tags:
  - product
  - specification
status: approved
---

# Product Specification V2

This note defines approved V2 behavior. It is not an implementation checklist; see [[CURRENT_STATE]].

## Product identity and limits

- The product is a sampler first.
- Load at most 20 samples.
- Support WAV, AIFF/AIF, MP3, and FLAC.
- Design and art direction remain postponed until functionality is stable.

## Sources and pitch

- Each MIDI trigger randomly selects an enabled source, influenced by per-sample Weight.
- Choose a random start within that source's manual Start/End region.
- Each sample has manual Start, End, Source Key, Transpose, Fine Tune, pitch-preserving Stretch, Gain, Weight, and enabled state.
- Fine Tune is mandatory.
- A global Target Key defines the harmonic target.
- MIDI pitch can be ON or OFF and has a root note.
- Automatic key detection and automatic detune/cents detection are postponed.
- Prefer the MIT-licensed Signalsmith Stretch implementation for pitch-preserving stretch. Prepare and cache stretch data outside the realtime audio thread.

## Triggering, voices, and chance

- Provide 16 voices with Mono and Poly modes.
- Chance effects are Reverse, Retrigger, Skip, Reorder, Bend, and Drop.
- Step Mask lengths are 2, 4, 8, or 16.
- Advance the Step Mask on MIDI Note On only.
- A NORMAL step bypasses Chance effects; an FX step allows them.

## Takes and replay

- One Take is one complete Step Mask cycle.
- Keep the latest 8 Takes.
- Take History stores explicit random and event decisions, not rendered audio.
- LIVE mode generates new Takes.
- HISTORY mode deterministically replays stored decisions.

## Output processing

- Final Length occurs after Chance processing.
- Attack and Release occur after Final Length.
- Apply global Bit Crush, global Sample Rate Reduction, and global Output Gain.
- V2 uses the fixed order documented in [[SIGNAL_CHAIN]].

## Explicit exclusions

- No waveform magnifier.
- No BPM detection or host tempo sync.
- No grid-based random starts.
- No DAW bar or loop synchronization.

Decision status and rationale belong in [[DECISIONS]]. Test expectations belong in [[TEST_MATRIX]].

