---
title: Signal Chain
tags:
  - architecture
  - dsp
status: approved
---

# Signal Chain

## Fixed product order

1. MIDI Note On and POLY/MONO voice policy.
2. Weighted selection from enabled, playable sources.
3. Random start inside the source's manual Start/End region.
4. Source/Target tonic correction, Transpose, Fine Tune, and optional MIDI pitch/root offset.
5. Cached pitch-preserving Stretch and source Gain.
6. Final Length, Attack/Release, boundary fade, and voice-steal crossfade.
7. Mix up to 16 voices.
8. SCRAMBLE, including repeat/hold/octave gestures.
9. FRACTURE, including predictive digital damage and Rate Reduction.
10. SPECTRAL DRAW.
11. SMEAR crystalline grain cloud.
12. Output Gain.

## Current code path

The creative-quality branch implements all 12 steps in this order. Temporal rearrangement precedes nonlinear/digital destruction so FRACTURE can animate the chopped gestures; SPECTRAL DRAW then sculpts that result; SMEAR adds a final pitched crystalline layer without feeding it back through the destructive stages. Removed FREEZE and CODEC headline stages, Take/Step/per-event processing, and Bit Crush are not in the path. Every creative macro defaults transparent. See [[CURRENT_STATE]] and [[TEST_MATRIX]] for the exact verified status.
