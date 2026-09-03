---
title: Current Implementation State
aliases:
  - Current State
tags:
  - implementation
  - current-state
status: active
verified: 2026-09-03
---

# Current Implementation State

Evidence: source inspection at local commit `723b9da` on 2026-09-03. The worktree was clean and `main` was one commit ahead of `origin/main`. No fresh build or runtime audio test was performed during this documentation setup.

## Implemented in code

- JUCE 8.0.13 CMake project producing VST3 and Standalone targets; Windows VST3 GitHub Actions workflow.
- Stereo instrument output, MIDI input, 16 fixed voices, and oldest-voice stealing.
- WAV, AIFF/AIF, MP3, and FLAC extensions are accepted and passed to JUCE decoding; source audio is loaded fully into RAM.
- Maximum 20 stored sources; multi-file picker/drop, remove, clear, enable/disable, enable all, and disable all.
- Random selection among enabled, playable sources using per-source Weight.
- Per-source Gain affects playback. Per-source Gain and Weight are editable in the UI and persisted.
- Random start uses a global Random Start amount over the physical file range. It does **not** yet honor the stored manual Start/End region.
- Sample-accurate MIDI event handling within each block; Note Off starts the global release envelope.
- Global Attack, Release, Output Gain, and deterministic Seed parameters with host state persistence.
- Path and source-setting persistence, missing-file representation, immutable pool snapshots, and retained retired snapshots for realtime-safe removal.
- Linear source-rate conversion, mono-to-stereo playback, physical-end fade, and a short voice-steal tail crossfade.

## Stored scaffolding, not functional behavior

`SampleSettings` persists manual Start/End, Source Key, Transpose, Fine Tune, and Stretch Ratio, but these fields are not exposed in the editor and do not affect audio. Their presence must not be reported as feature completion.

## Not implemented

- Random start constrained to manual Start/End.
- Global Target Key; functional Source Key, Transpose, Fine Tune, or MIDI pitch/root-note behavior.
- Signalsmith or any pitch-preserving stretch processing/cache.
- Mono/Poly mode selection; current behavior is polyphonic.
- Step Mask, Chance effects, Takes, Take History, LIVE/HISTORY replay.
- Final Length.
- Global Bit Crush and Sample Rate Reduction.
- Automated tests. The current workflow builds and checks that a VST3 bundle/module exists, but does not run DSP or behavior tests.

See [[TEST_MATRIX]] before changing an item from planned to implemented.

