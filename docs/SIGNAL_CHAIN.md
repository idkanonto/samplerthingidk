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

## Gate A code path

The current foundation implements steps 1–7, temporary standalone Rate Reduction, and Output. It also computes the shared host-grid boundaries needed by later effects. Removed Take/Step/per-event processing and Bit Crush are not in the path. Later gates must insert effects only at their assigned location and keep bypass states transparent.
