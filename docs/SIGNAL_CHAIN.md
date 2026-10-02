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
4. Per-source pitch-preserving STRETCH (0.25×–2× speed), prepared outside the callback.
5. TUNE, DRIFT, global PITCH, and optional STACK MIDI offset from neutral note 72. Source/target key metadata is inert.
6. Source TRIM.
7. Internal click-safe envelope, boundary fade, and voice-steal crossfade.
8. Mix up to 16 voices.
9. FAULT deterministic tempo-aligned RESAMPLE (exactly −12 or +12 varispeed), BITCRUSH, or slice-local REVERSE.
10. ETCH spectral mask.
11. BLEED crystalline grain cloud.
12. MASTER: sample-smoothed perceptual VOL gain (0–125%, with 0% / `−∞` true silence).

## Current code path

The local final-pass implementation follows this order. FAULT defaults transparent because PRESSURE is zero; ETCH and BLEED keep their existing DSP under the new names. SCRAMBLE and MELT are retired from the live processor path. See [[CURRENT_STATE]] and [[TEST_MATRIX]] for the exact verification boundary.
