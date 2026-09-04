# Random Chop Sampler

A 16-voice JUCE VST3/standalone sampler instrument. Every MIDI note chooses a weighted random source and a random legal start point. The current V2 build adds manual source/target tonic correction, per-source Transpose and Fine Tune, and optional MIDI-note pitch around a configurable root for WAV, AIFF/AIF, MP3, and FLAC sources.

## Build without local development tools (recommended)

1. Create an empty GitHub repository and upload/push this project so `CMakeLists.txt` is at the repository root.
2. Open the repository's **Actions** tab, select **Build Windows VST3**, and choose **Run workflow**. It also runs automatically on pushes and pull requests to `main`.
3. When the run finishes, open it and download **Random-Chop-Sampler-Windows-VST3** from the Artifacts section.
4. Extract the downloaded archive. Copy the complete `Random Chop Sampler.vst3` directory to `C:\Program Files\Common Files\VST3\` and rescan plug-ins in the DAW.

The workflow uses GitHub's `windows-2022` runner with Visual Studio 2022 and CMake. CMake FetchContent downloads pinned JUCE 8.0.13 automatically, builds the Release VST3 and test targets, runs CTest, verifies that the bundle contains its module binary, and uploads the complete `.vst3` bundle. Nothing needs to be installed locally.

## Optional local build

If you already have Visual Studio 2022 with **Desktop development with C++**, Git, and CMake, JUCE is still downloaded automatically:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RandomChopSampler_VST3
```

The built bundle is normally at `build/RandomChopSampler_artefacts/Release/VST3/Random Chop Sampler.vst3`. Copy the entire `.vst3` bundle to:

`C:\Program Files\Common Files\VST3\`

Then rescan VST3 plug-ins in the DAW, create an instrument track, insert **Random Chop Sampler**, drag WAV/AIFF files into its panel, and play MIDI notes. The Standalone target is useful for initial UI/audio-device testing.

## Architecture and real-time safety

- `SampleManager` decodes files only from message/host state threads and publishes immutable pool snapshots atomically.
- `SampleData` is reference-counted. Voices retain their own reference, so removing or clearing a sample cannot invalidate active playback. Superseded snapshots and sources are reclaimed only from control-thread maintenance after realtime references have drained.
- `RandomSamplerVoice` performs linear sample-rate conversion combined with the Note-On-resolved pitch ratio, mono-to-stereo routing, per-voice attack/release, a source-region boundary fade, and a 3 ms tail crossfade when a voice is stolen.
- `PluginProcessor` owns 16 fixed voices and an allocation-free xorshift64* randomizer. Voice stealing selects the oldest voice.
- MIDI rendering is sample-accurate within each host block. No disk access, decoding, blocking mutex, logging, or explicit allocation occurs in the audio callback.

## MVP checklist

- [x] VST3 instrument and standalone CMake targets; stereo output and MIDI input
- [x] Multi-file drag/drop and file picker for WAV, AIFF/AIF, MP3, and FLAC
- [x] Hard 20-source cap with visible count and graceful rejection
- [x] Visible scrollable list, per-file Enable/Remove, Enable All, Disable All, Clear All, and status feedback
- [x] Weighted enabled-source selection plus per-source gain
- [x] Selected-source waveform with draggable Start/End markers and region-bounded Random Start
- [x] Source Key/Target Key tonic correction, per-source Transpose and Fine Tune
- [x] MIDI Pitch on/off with configurable Root MIDI Note (default C5/MIDI 72)
- [x] Reproducible Seed sequence (sequence restarts after prepare or a Seed change)
- [x] 16-voice polyphony with oldest-voice stealing and clean Note Off release
- [x] Source-rate conversion via linear interpolation; mono and stereo playback
- [x] Per-voice attack/release plus 3 ms source-region boundary fade
- [x] Random Start, Attack, Release, Output, and Seed automation/state
- [x] Sample path persistence and graceful missing-file handling
- [x] Safe removal/clear while voices are active

## Known MVP limitations

- Samples are decoded fully into RAM; very large libraries are not streamed.
- Linear interpolation favors low CPU use over premium resampling quality.
- Restoring sample paths is synchronous because JUCE host state restoration provides no completion callback; it never occurs in `processBlock`, but an unusually large library can briefly delay project loading.
- Pitch-preserving Stretch, event processing, Step Mask, Take History, and master digital processing remain in later gated V2 phases.
- A reproducible Seed gives a deterministic trigger sequence for the same pool/order and parameter/MIDI event sequence; changing the pool changes the results.

## Practical test pass

Test in the standalone build first, then at least one VST3 DAW. Exercise mono/stereo WAV and AIFF at 44.1/48/96 kHz, rapid repeated notes, chords, Note Off, automation, Remove/Clear during playback, project save/reopen, and reopening after moving one source file.
