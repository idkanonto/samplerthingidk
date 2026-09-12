---
title: Targeted Redesign Plan
tags:
  - product
  - redesign
  - implementation-plan
status: implemented-superseded-by-logic-hardening
date: 2026-09-08
---

# Targeted Redesign Plan

This pass follows hands-on feedback after the creative-logic release. It supersedes the affected control and FRACTURE decisions in [[PRODUCT_SPEC_V2]] while preserving the sampler, realtime, and state-compatibility contracts.

## Assessment

| System | Finding | Decision |
|---|---|---|
| Sampler core | Source loading, weighted selection, preparation, voices, and persistence are strong. | Keep. |
| Sample-row selection | Row background selection works, but the child On control can change a source without making it the editor target. | Fix and regression-test the selection policy. |
| Root note | The integer parameter is correct, but the editor exposes raw MIDI numbers. | Keep the parameter ID and state; display and parse note-plus-octave names. |
| SCRAMBLE | Ring/slice architecture and gestures are strong. Enable transitions need explicit arming, and the middle of the macro can be denser. | Tune, do not rewrite. |
| FRACTURE | Distortion, filter, formant, comb, predictor, packet, and rate stages compete instead of forming one legible effect. | Redesign around distortion plus continuous filter morph. |
| SMEAR | The fixed-grain cloud is bright and stable but its grains are too static after launch. | Improve with bounded pitch orbit, stereo motion, and high-passed crystalline feedback. |
| Spectral Draw | Distinct, strong, and already verified. | Keep unchanged. |
| Major-effect UI | Knobs communicate values but not timing, motion, or effect identity. | Add compact parameter-driven visual panels without a full theme redesign. |

## Control audit

| Control | Decision | Result |
|---|---|---|
| Source pool and per-source preparation | Keep | Unchanged. |
| Target Key, MIDI Pitch, Root Note, Voice Mode | Keep | Root Note gains musical naming. |
| Global Grid | Keep | Drives SCRAMBLE arming and timing. |
| Start Range | Remove | Random start always spans the legal source region. |
| Final Length | Remove | Voices play to note-off or the prepared region end. |
| Attack | Internal only | Fixed short click-safe attack. |
| Release | Internal only | Fixed short musical release. |
| Output | Keep | Required final gain. |
| SCRAMBLE | Keep macro | Explicit grid arming and denser progression. |
| FRACTURE | Keep macro | Main distortion intensity. |
| CHARACTER (`fractureCharacter`) | Keep ID, relabel | Becomes FILTER MORPH. |
| FRACTURE RATE | Remove | Digital rate reduction no longer defines the focused effect. |
| FRACTURE preset browser | Remove | Two direct axes are faster and clearer than presets over two values. |
| Spectral Draw controls | Keep | Unchanged. |
| SMEAR | Keep macro | Internals gain motion and high-frequency feedback. |
| Seed and random-start toggle | Internal/remove | Seed remains persisted internally; random start remains always active. |

## Research and licensing

- [Cytomic technical papers](https://cytomic.com/technical-papers/) publish the trapezoidal state-variable-filter derivations for public use. The simultaneous-output, modulation-stable structure informs the original continuous morph implementation; no third-party source file is copied.
- [DaisySP](https://github.com/electro-smith/DaisySP) is MIT. Its compact overdrive and SVF modules confirm the value of normalized pre/post gain and bounded filter state. The project remains dependency-free here because the required two-channel morph is smaller and clearer as an original implementation.
- [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils) has per-module licensing; the relevant `chowdsp_filters` and `chowdsp_waveshapers` modules are GPLv3. They are studied for modulation-safe filter and antiderivative waveshaping practices but not copied or linked.
- [Surge XT](https://github.com/surge-synthesizer/surge) is GPLv3. Its broad filter/waveshaper library is useful comparative research but is not reusable in this project without accepting GPL obligations.
- JUCE 8.0.13 remains the UI/plugin framework under AGPLv3 or a commercial JUCE licence. Signalsmith Stretch and Signalsmith Linear remain MIT dependencies for source preparation. Existing bundled notices continue to satisfy their notice requirements.
- Autochroma, FL Studio Transporter, and Rift are behavioral/visual references only. No proprietary code, layout, preset, or implementation detail is copied.

## Signature behavior

### SCRAMBLE

A rising macro edge enters an armed state. Audio remains dry until the next boundary supplied by the shared host grid, then a new deterministic gesture begins. Disabling cancels the active event; re-enabling continues the private random stream so each entry differs without changing average intensity. A sublinear density curve gives low values one restrained slice, makes the midpoint reliably affect most of the grid, and reaches all slices at maximum.

### FRACTURE

FRACTURE becomes a focused pre-emphasis -> normalized distortion morph -> modulation-safe state-variable filter morph -> DC blocker chain. Amount raises drive, wetness, resonance, and motion. FILTER MORPH continuously moves through low, band, notch, and high responses while correlated oscillators and the input envelope animate cutoff and morph position within bounded ranges. There are no comb, formant, packet, or rate-reduction sub-effects.

### SMEAR

The six preallocated grains remain. Each grain gains slow pitch orbit, stereo orbit, and an individual brightness weight. A small amount-dependent high-passed feedback path feeds only crystalline residual energy back into the history, adding evolving shimmer without a muddy broadband tail.

## Visualization approach

- SCRAMBLE: an eight-cell moving timeline with displaced active slices and a boundary scanner.
- FRACTURE: a distortion trace crossed by a continuously moving filter-morph contour.
- SMEAR: six orbiting crystalline streaks and glints whose density and spread follow the macro.

All animation runs on the message-thread timer and reads APVTS atomics only. It does not touch audio buffers or allocate in the callback.

## Implementation and verification

1. Update parameter/state policy and musical-note conversion.
2. Add explicit SCRAMBLE arming and progression tests.
3. Replace FRACTURE internals and retune deterministic renders.
4. Add bounded SMEAR motion/feedback and safety tests.
5. Fix child-control row selection and add focused selection-policy coverage.
6. Add the three compact visual panels and re-check minimum/default/maximum geometry.
7. Run Windows Release build, CTest, listening-render checks, bundle inspection, and post-merge `main` verification.
