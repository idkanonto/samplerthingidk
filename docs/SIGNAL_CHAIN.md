---
title: Signal Chain
tags:
  - architecture
  - dsp
status: approved-v2
---

# Signal Chain

## Fixed V2 order

1. MIDI Note On and Mono/Poly voice policy.
2. Step Mask decision: NORMAL bypasses Chance; FX permits Chance.
3. Select a random enabled source using Weight.
4. Select a random start inside manual Start/End.
5. Apply source pitch controls: Source Key/Target Key behavior, MIDI pitch/root-note mode, Transpose, and Fine Tune.
6. Apply cached pitch-preserving Stretch.
7. Apply per-sample Gain.
8. Apply allowed Chance processing: Reverse, Retrigger, Skip, Reorder, Bend, Drop.
9. Apply Final Length.
10. Apply Attack/Release.
11. Mix up to 16 voices.
12. Apply global Bit Crush.
13. Apply global Sample Rate Reduction.
14. Apply global Output Gain.

Take History records the decisions that drive this path; it does not store rendered audio. See [[PRODUCT_SPEC_V2#Takes and replay]].

## Current code path

The current implementation is shorter: MIDI event → weighted playable-source selection → region-bounded random start → Source/Target tonic correction + Transpose + Fine Tune + optional MIDI pitch/root offset → linear source-rate conversion and per-source Gain → per-voice Attack/Release/region-boundary fade → voice mix → global Output Gain. See [[CURRENT_STATE]] for omissions.

The approved chain fixes stage order; individual Chance-effect algorithms must remain consistent with [[PRODUCT_SPEC_V2]] and [[DECISIONS]].
