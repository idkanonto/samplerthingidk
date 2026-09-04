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

Evidence: source inspection plus [Windows Release CI run #19](https://github.com/idkanonto/samplerthingidk/actions/runs/33880973821) at commit `e7748683a3440821ac2816b000648287002d7ca4`. The Windows Server 2022 Release workflow built `RandomChopSampler_VST3` and `RandomChopSamplerTests`, passed `ctest --test-dir build -C Release --output-on-failure`, verified the bundle and license resources, and uploaded it. The inspected archive matched GitHub's SHA-256 `5caee71a6cdc494b7982bf3b81f61e47c184000af675e6e33223a23fd8a7da91` and contained a 7,359,488-byte module at `Random Chop Sampler.vst3/Contents/x86_64-win/Random Chop Sampler.vst3`. No DAW/host audio test or full format/sample-rate matrix was run.

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
- Per-source pitch-preserving Stretch is editable and persisted as a 0.5x–2.0x duration multiplier. Signalsmith Stretch is pinned at `57b93f4e9206a089a45387eaa39bdc9f310d3308`; 1.0x reuses decoded PCM, while other ratios are prepared by one background worker and published as immutable revisioned versions.
- Repeated stretch edits coalesce queued work. Source runtime identity and revision checks reject stale or removed-source results; active voices retain superseded prepared versions, and final prepared-buffer reclamation occurs during non-realtime maintenance.
- Sample-accurate MIDI event handling within each block; Note Off starts the global release envelope.
- Global Attack, Release, Output Gain, deterministic Seed, Target Key, MIDI Pitch, and Root MIDI Note parameters with host state persistence.
- Path and source-setting persistence, missing-file representation, immutable pool snapshots, and deferred control-thread reclamation after realtime references drain.
- Linear source-rate conversion combined with the resolved pitch ratio, mono-to-stereo playback, source-region boundary fade, and a short voice-steal tail crossfade.
- `RandomChopSamplerTests` CTest coverage for weighted playable-source selection, the 20-source cap, persistent source identity/settings, parameter-only state replacement, region clamping/random bounds, forward/reverse boundary reads, tonic correction, manual/MIDI pitch math, finite ratio handling, pitched voice increments, 0.5x/1.0x/2.0x stretch behavior, approximate pitch preservation, stale-result rejection, removal during preparation, and superseded-version lifetime; the Windows workflow builds and runs the test target.

## Not implemented

- Mono/Poly mode selection; current behavior is polyphonic.
- Step Mask, Chance effects, Takes, Take History, LIVE/HISTORY replay.
- Final Length.
- Global Bit Crush and Sample Rate Reduction.

See [[TEST_MATRIX]] before changing an item from planned to implemented.
