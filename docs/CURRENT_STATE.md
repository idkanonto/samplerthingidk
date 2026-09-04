---
title: Current Implementation State
aliases:
  - Current State
tags:
  - implementation
  - current-state
status: active
verified: 2026-09-04
---

# Current Implementation State

Evidence: source inspection plus [Windows Release CI run #13](https://github.com/idkanonto/samplerthingidk/actions/runs/33793508182) at commit `7ef3b9223c53edc1874b3384a059f182b89a8e3d`. The Windows Server 2022 Release workflow built `RandomChopSampler_VST3` and `RandomChopSamplerTests`, passed `ctest --test-dir build -C Release --output-on-failure`, verified the bundle, and uploaded it. The inspected archive matched GitHub's SHA-256 `c9ec5e48b27cb506ee6544b41e7f4ddf0164c7496c89eff6f470c63d6d711027` and contained a 7,266,816-byte module at `Random Chop Sampler.vst3/Contents/x86_64-win/Random Chop Sampler.vst3`. No DAW/host audio test or full format/sample-rate matrix was run.

## Implemented in code

- JUCE 8.0.13 CMake project producing VST3 and Standalone targets; Windows VST3 GitHub Actions workflow.
- Stereo instrument output, MIDI input, 16 fixed voices, and oldest-voice stealing.
- WAV, AIFF/AIF, MP3, and FLAC extensions are accepted and passed to JUCE decoding; source audio is loaded fully into RAM.
- Maximum 20 stored sources; multi-file picker/drop, remove, clear, enable/disable, enable all, and disable all.
- Random selection among enabled, playable sources using per-source Weight.
- Per-source Gain affects playback. Per-source Gain and Weight are editable in the UI and persisted.
- The selected source displays an immutable, decode-time peak envelope with draggable normalized Start/End markers and percentage, seconds, and sample-position feedback.
- Random Start expands proportionally from Start and remains inside the manual Start/End region with interpolation and boundary-fade margins.
- Per-source Source Key (`NONE` or chromatic tonic), Transpose (-24 to +24 semitones), and Fine Tune (-100 to +100 cents) are editable and persisted. Global Target Key, MIDI Pitch, and Root MIDI Note are automatable host-state parameters; MIDI Pitch defaults off and the root defaults to C5/MIDI 72.
- Note On resolves the shortest signed tonic correction (with the six-semitone tie upward), manual Transpose, Fine Tune, and optional MIDI offset into one finite playback ratio. This is a uniform pitch shift and does not transform chord quality.
- Sample-accurate MIDI event handling within each block; Note Off starts the global release envelope.
- Global Attack, Release, Output Gain, deterministic Seed, Target Key, MIDI Pitch, and Root MIDI Note parameters with host state persistence.
- Path and source-setting persistence, missing-file representation, immutable pool snapshots, and deferred control-thread reclamation after realtime references drain.
- Linear source-rate conversion combined with the resolved pitch ratio, mono-to-stereo playback, source-region boundary fade, and a short voice-steal tail crossfade.
- `RandomChopSamplerTests` CTest coverage for weighted playable-source selection, the 20-source cap, persistent source identity/settings, parameter-only state replacement, region clamping/random bounds, forward/reverse boundary reads, tonic correction, manual/MIDI pitch math, finite ratio handling, and pitched voice increments; the Windows workflow builds and runs the test target.

## Stored scaffolding, not functional behavior

`SampleSettings` persists Stretch Ratio, but stretching is not exposed in the editor and does not affect audio. Its presence must not be reported as feature completion.

## Not implemented

- Signalsmith or any pitch-preserving stretch processing/cache.
- Mono/Poly mode selection; current behavior is polyphonic.
- Step Mask, Chance effects, Takes, Take History, LIVE/HISTORY replay.
- Final Length.
- Global Bit Crush and Sample Rate Reduction.

See [[TEST_MATRIX]] before changing an item from planned to implemented.
