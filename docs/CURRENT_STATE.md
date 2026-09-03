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

Evidence: source inspection plus [Windows Release CI run #10](https://github.com/idkanonto/samplerthingidk/actions/runs/33791050526) at commit `6b1b209a5062626c31e6887f8bf7ce00af655a2b` on 2026-09-03. The workflow configured a Windows Server 2022 Release build, built `RandomChopSampler_VST3` and `RandomChopSamplerTests`, passed `ctest --test-dir build -C Release --output-on-failure`, verified that the VST3 bundle contained a module, and uploaded the bundle artifact. No DAW/host audio test or full format/sample-rate matrix was run.

## Implemented in code

- JUCE 8.0.13 CMake project producing VST3 and Standalone targets; Windows VST3 GitHub Actions workflow.
- Stereo instrument output, MIDI input, 16 fixed voices, and oldest-voice stealing.
- WAV, AIFF/AIF, MP3, and FLAC extensions are accepted and passed to JUCE decoding; source audio is loaded fully into RAM.
- Maximum 20 stored sources; multi-file picker/drop, remove, clear, enable/disable, enable all, and disable all.
- Random selection among enabled, playable sources using per-source Weight.
- Per-source Gain affects playback. Per-source Gain and Weight are editable in the UI and persisted.
- The selected source displays an immutable, decode-time peak envelope with draggable normalized Start/End markers and percentage, seconds, and sample-position feedback.
- Random Start expands proportionally from Start and remains inside the manual Start/End region with interpolation and boundary-fade margins.
- Sample-accurate MIDI event handling within each block; Note Off starts the global release envelope.
- Global Attack, Release, Output Gain, and deterministic Seed parameters with host state persistence.
- Path and source-setting persistence, missing-file representation, immutable pool snapshots, and deferred control-thread reclamation after realtime references drain.
- Linear source-rate conversion, mono-to-stereo playback, source-region boundary fade, and a short voice-steal tail crossfade.
- `RandomChopSamplerTests` CTest coverage for weighted playable-source selection, the 20-source cap, persistent source identity/settings, parameter-only state replacement, region clamping/random bounds, and forward/reverse boundary reads; the Windows workflow builds and runs the test target.

## Stored scaffolding, not functional behavior

`SampleSettings` persists Source Key, Transpose, Fine Tune, and Stretch Ratio, but these fields are not exposed in the editor and do not affect audio. Their presence must not be reported as feature completion.

## Not implemented

- Global Target Key; functional Source Key, Transpose, Fine Tune, or MIDI pitch/root-note behavior.
- Signalsmith or any pitch-preserving stretch processing/cache.
- Mono/Poly mode selection; current behavior is polyphonic.
- Step Mask, Chance effects, Takes, Take History, LIVE/HISTORY replay.
- Final Length.
- Global Bit Crush and Sample Rate Reduction.

See [[TEST_MATRIX]] before changing an item from planned to implemented.
