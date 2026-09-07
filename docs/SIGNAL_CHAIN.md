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

The current Gate C branch implements steps 1–10 and 12–14. FRACTURE, SMEAR, and CODEC are in their final relative order; the Gate D SPECTRAL DRAW stage will be inserted only at step 11. Rate Reduction is now part of CODEC rather than a standalone module. Removed Take/Step/per-event processing and Bit Crush are not in the path, and every new creative stage defaults transparent.
