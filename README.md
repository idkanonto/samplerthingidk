# recompiler.dll

`recompiler.dll` is a JUCE VST3/standalone sampler instrument. It loads up to 20 WAV, AIFF/AIF, MP3, or FLAC sources, selects enabled sources by Weight, chooses a legal random start inside each editable source region, and plays through a fixed 16-voice POLY/MONO engine.

The visible product name intentionally contains `.dll`; the Windows plug-in is still distributed as the standards-compliant `recompiler.dll.vst3` bundle, not as a loose DLL.

## Build with GitHub Actions

Run **Build Windows VST3** in the repository Actions tab. The workflow configures a pinned JUCE 8.0.13/Signalsmith build, builds Release VST3, Standalone, and tests, runs CTest, verifies the bundle and executable, and uploads **recompiler-dll-Windows-VST3**.

Extract the artifact and copy the complete `recompiler.dll.vst3` directory to `C:\Program Files\Common Files\VST3\`, then rescan plug-ins in the DAW.

## Optional local build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RandomChopSampler_VST3 RandomChopSampler_Standalone RandomChopSamplerTests
ctest --test-dir build -C Release --output-on-failure
```

The target name remains `RandomChopSampler` to preserve build continuity; the host-visible product name is `recompiler.dll`.

## Current functional boundary

- Immutable 20-source pool, missing-source persistence, stable identity, deferred non-realtime reclamation.
- Weighted source selection; editable Start/End, Source Key, Transpose, Fine Tune, Gain, Weight, and 0/OFF or 1x–4x Stretch.
- Target Key, optional MIDI pitch/root, Random Start, Final Length, Attack, Release, POLY/MONO, Seed, and Output.
- Host-derived 1/8, 1/16, or 1/32 global grid with a safe 120 BPM fallback.
- Global FREEZE with grid-sized capture/hold and deterministic optional exact-octave flipping; global SCRAMBLE with bounded chunk rearrangement and Amount.
- Global FRACTURE with six controls and 30 real factory presets, plus bounded SMEAR and damaged-digital CODEC stages.
- The preserved 1x–64x Rate Reduction now lives inside CODEC; all three Gate C stages load neutral.
- The former Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, and Bit Crush systems are removed. Old state entries are ignored safely.

The approved global creative chain is `FREEZE → SCRAMBLE → FRACTURE → SPECTRAL DRAW → SMEAR → CODEC → OUTPUT`. See the project brain in [`docs/INDEX.md`](docs/INDEX.md) for gate status and realtime constraints. The current gate is functional engineering only; final visual design is intentionally deferred.
