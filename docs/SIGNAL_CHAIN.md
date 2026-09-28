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
4. TUNE, DRIFT, global PITCH, and optional STACK MIDI offset from neutral note 72. Source/target key metadata is inert.
5. Source TRIM.
6. Internal click-safe envelope, boundary fade, and voice-steal crossfade.
7. Mix up to 16 voices.
8. FAULT deterministic tempo-aligned PULL, DUST, or BEND mutation.
9. ETCH spectral mask.
10. BLEED crystalline grain cloud.
11. MASTER: sample-smoothed perceptual VOL gain (0–125%, with 0% true silence) and mute ramp.

## Current code path

The local final-pass implementation follows this order. FAULT defaults transparent because PRESSURE is zero; ETCH and BLEED keep their existing DSP under the new names. SCRAMBLE and MELT are retired from the live processor path. See [[CURRENT_STATE]] and [[TEST_MATRIX]] for the exact verification boundary.
