# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Music producers and sound designers operating the instrument inside a DAW, where controls must scan quickly at compact plug-in dimensions and remain legible during long, low-light sessions.

## Product Purpose

A JUCE VST3 and standalone random-sample instrument that turns a pool of up to 20 audio files into playable, pitch-aware material with four direct creative macro systems. Success means that importing, selecting, shaping, and performing samples feels immediate while the audio engine remains authoritative.

## Positioning

Each note selects an enabled source with equal probability and routes it through a fixed creative chain of SCRAMBLE, MELT, SPECTRAL DRAW, SMEAR, and OUTPUT. The source-pool workflow and single-macro creative engines are the product's defining mechanism.

## Operating Context

The interface runs as an offline embedded WebView inside JUCE on Windows and is also used by the standalone build. It is operated primarily with mouse, keyboard, and DAW automation at a fixed logical 960×647 canvas with discrete whole-interface scale presets.

## Capabilities and Constraints

- Preserve the native C++ audio engine, parameter automation, project-state persistence, and existing JUCE bridge contract.
- Support WAV, AIFF/AIF, MP3, and FLAC sources, with source region, key, transpose, fine tune, gain, and enabled state.
- Preserve Play In Key, Chords, POLY/MONO, the four creative modules, stereo metering, output level, and mute.
- The UI ships completely offline with bundled fonts and assets.
- The composition is fixed at 960×647 logical pixels and scales uniformly only at 75%, 100%, 125%, and 150%.
- Do not reintroduce retired DSP or controls and do not begin a later development pass during this visual reconstruction.

## Brand Commitments

The host-visible product name and bundle identity remain `recompiler.dll`, but this interface intentionally reserves an empty branding zone and displays no product wordmark, subtitle, or placeholder logo. The approved reference geometry is the supplied concept image; the approved replacement identity is a dark, monochrome, engineered instrument panel rather than the prior light sketch treatment.

## Evidence on Hand

- Current UI screenshot supplied by the user.
- Approved geometry/reference screenshot supplied by the user.
- Existing React/WebView implementation in `frontend/src`.
- Native behavior and current feature boundary documented in `README.md` and `docs/`.

## Product Principles

- Audio behavior and DAW automation remain stable while presentation changes.
- Information density serves fast operation, not decorative complexity.
- The waveform and creative visualizers are primary feedback instruments.
- Every state must remain readable at each supported discrete scale.
- Visual identity comes from material, typography, proportion, and exact control craft rather than visible branding.
