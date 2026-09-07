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
8. FREEZE.
9. SCRAMBLE.
10. FRACTURE.
11. SPECTRAL DRAW.
12. SMEAR.
13. CODEC, including Rate Reduction.
14. Output Gain.

## Current code path

The current Gate D branch implements all 14 steps in this order. SPECTRAL DRAW is a real global STFT stage after FRACTURE and before SMEAR. Rate Reduction is part of CODEC rather than a standalone module. Removed Take/Step/per-event processing and Bit Crush are not in the path, and every new creative stage defaults transparent. Gate D Windows runtime verification is still pending; see [[TEST_MATRIX]].
