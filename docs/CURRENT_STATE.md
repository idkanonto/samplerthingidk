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

Evidence: source inspection plus [Windows Release CI run #29](https://github.com/idkanonto/samplerthingidk/actions/runs/33936111067) at commit `78121046420497f7a95e2e6517a6d42dc8e9e6cc`. The Windows Server 2022 Release workflow built `RandomChopSampler_VST3` and `RandomChopSamplerTests`, passed `ctest --test-dir build -C Release --output-on-failure`, verified the bundle and license resources, and uploaded it. The inspected archive matched GitHub's SHA-256 `b8378fde5a016696f1219f75437621c9b8f2b0e0841de317384a6a872752beae` and contained a 7,392,256-byte module at `Random Chop Sampler.vst3/Contents/x86_64-win/Random Chop Sampler.vst3`. No DAW/host audio test or full format/sample-rate matrix was run.

## Implemented in code

- JUCE 8.0.13 CMake project producing VST3 and Standalone targets; Windows VST3 GitHub Actions workflow.
- Stereo instrument output, MIDI input, and 16 fixed voices. POLY supports overlap and chords with oldest-voice stealing; MONO replaces the current primary voice through the existing 3 ms tail crossfade and releases any remaining POLY voices.
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
- Final Length defaults to FULL and otherwise constrains an event to 10–5000 ms. Its Release fade is fitted inside the forced boundary, while Attack and Note Off Release continue to shape the final event.
- Chance resolves once per Note On in the fixed Reverse → Retrigger → Skip → Reorder → Bend → Drop order. A fixed-size `EventDecision` stores all applied flags, repeat values, signed jump, four-piece permutation/span, signed bend depth, and eight-slot drop mask; voices consume no additional randomness.
- Reverse mirrors the event start and reads backward within Start/End. Retrigger repeats a 10–500 ms segment 1–8 times; Skip applies one bounded signed start jump; Reorder permutes four pieces of a fragment that starts at the resolved event position and is capped at one second; Bend ramps playback rate by up to ±12 semitones; Drop applies a deterministically stored eight-slot mute mask with 1 ms edge smoothing.
- The event-driven Step Mask supports 2, 4, 8, or 16 steps and defaults to eight FX steps. Every MIDI Note On consumes exactly one step in MIDI-buffer order, including triggers without a playable source; NORMAL bypasses Chance without consuming its RNG calls, while FX permits the fixed Chance chain.
- Individual NORMAL/FX toggles, All NORMAL, All FX, Randomize, and the automatable length selector are functional. Mask bits persist in a versioned state child, legacy states default to all FX, and every length change atomically resets the audio-thread event cursor without using time, tempo, or transport state.
- Global Attack, Release, Final Length, Voice Mode, all six Chance percentages, Repeat Size/Count, Output Gain, deterministic Seed, Target Key, MIDI Pitch, and Root MIDI Note parameters with host state persistence. All Chance percentages default to 0% and consume no RNG state while disabled.
- Path and source-setting persistence, missing-file representation, immutable pool snapshots, and deferred control-thread reclamation after realtime references drain.
- Linear source-rate conversion combined with the resolved pitch ratio, mono-to-stereo playback, source-region boundary fade, and a short voice-steal tail crossfade.
- `RandomChopSamplerTests` CTest coverage for the prior pool, region, pitch, stretch, lifetime, voice, envelope, and Chance behavior plus all Step Mask lengths, exact cycle boundaries, deterministic chord-order advancement, length/reset generation behavior, NORMAL/FX routing without RNG drift, defaults, editing, and persistence; the Windows workflow builds and runs the test target.

## Not implemented

- Takes, Take History, LIVE/HISTORY replay.
- Global Bit Crush and Sample Rate Reduction.

See [[TEST_MATRIX]] before changing an item from planned to implemented.
