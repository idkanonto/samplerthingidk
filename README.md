# recompiler.dll

`recompiler.dll` is a JUCE VST3/standalone sampler instrument. It loads up to 20 WAV, AIFF/AIF, MP3, or FLAC sources, selects enabled sources by Weight, chooses a legal random start inside each editable source region, and plays through a fixed 16-voice POLY/MONO engine.

The visible product name intentionally contains `.dll`; the Windows plug-in is still distributed as the standards-compliant `recompiler.dll.vst3` bundle, not as a loose DLL.

## Build with GitHub Actions

Run **Build Windows VST3** in the repository Actions tab. The workflow configures a pinned JUCE 8.0.13/Signalsmith build, builds Release VST3, Standalone, tests, and `recompiler-dll-Windows-Setup.exe`, runs CTest, silently install/uninstall-tests the package, and uploads **recompiler-dll-Windows-Installer**, the raw **recompiler-dll-Windows-VST3** fallback, and deterministic creative-effect listening renders.

For normal Windows installation, download **recompiler-dll-Windows-Installer**, extract the artifact, and run `recompiler-dll-Windows-Setup.exe` as administrator. It installs the complete plug-in bundle to `C:\Program Files\Common Files\VST3\recompiler.dll.vst3`, installs the Standalone app under `C:\Program Files\recompiler.dll\`, and creates an uninstaller. Rescan plug-ins in the DAW after setup. The installer is not code-signed yet, so Windows may show a SmartScreen warning.

For a manual/portable install, extract **recompiler-dll-Windows-VST3** and copy the complete `recompiler.dll.vst3` directory to `C:\Program Files\Common Files\VST3\`.

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
- Target Key, optional MIDI pitch with musically named root note, POLY/MONO, and smoothed Output. Random source/start behavior is always part of the instrument; reproducibility uses a persisted internal seed rather than a technical Seed control.
- Host-derived 1/8, 1/16, or 1/32 global grid with stable block-edge ownership and a safe 120 BPM fallback.
- Signature SCRAMBLE macro with next-boundary arming, a consistent per-grid manipulation budget, bounded rearrangement, seam-blended micro-holds/repeats, reverse, jumps, and integrated pitched fragments.
- MELT is one automatic slice-stretch macro with pitch-preserving overlap-add grains, amount-scaled `1.08x`–`4x` expansion, and internally latched per-slice reversal.
- SMEAR is a pitched, scattered stereo crystalline texture whose single macro introduces progressively more and smaller grains, with lifetime-relative motion, ratio-selected filtered history, overlap-energy gain control, and transient-aware wet control rather than a blur stage.
- Real global SPECTRAL DRAW with a persistent 128×64 attenuation canvas, Draw/Erase/Clear tools, four tempo-derived scan rates, Depth, and reported 1024-sample latency.
- The former standalone FREEZE, CODEC, and FRACTURE stages, FRACTURE controls/presets, Start Range, Final Length, public Attack/Release, Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, and Bit Crush systems are removed. Useful Freeze gestures are absorbed into SCRAMBLE; retired Fracture state is ignored safely rather than reinterpreted as MELT.

The approved global creative chain is `SCRAMBLE → MELT → SPECTRAL DRAW → SMEAR → OUTPUT`. See the project brain in [`docs/INDEX.md`](docs/INDEX.md) for verification status and realtime constraints. Final visual art direction remains separate from this sound-and-behavior pass.
