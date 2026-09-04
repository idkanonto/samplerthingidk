---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Update evidence when tests actually run. “Code-inspected” is not a runtime pass.

| Area | Minimum verification | Current evidence |
|---|---|---|
| Build | Clean Release VST3 and Standalone builds | [Windows Release CI run #23](https://github.com/idkanonto/samplerthingidk/actions/runs/33899202348) at `0f9b4fd` built the Release VST3 and test targets, passed CTest, verified the bundle/module and license resources, and uploaded the inspected artifact; Standalone was not built in this run |
| Formats | Mono/stereo WAV, AIFF/AIF, MP3, FLAC at 44.1/48/96 kHz | Run #5 decoded a mono 44.1 kHz WAV fixture; AIFF/AIF, MP3, FLAC, stereo, and other rates remain code-inspected but not runtime-tested |
| Pool | 20 accepted, 21st rejected; remove/clear; missing-file restore | Run #5 tested 20 accepted, the 21st rejected, and clear/restore; remove and missing-file restore remain code-inspected |
| Selection | Disabled never selected; Weight distribution is plausible; seeded repeatability | Run #5 tested that disabled/missing sources are never selected and exercised a 1:9 weighting over 10,000 choices; seeded repeatability remains code-inspected |
| Random region | Starts always within manual Start/End | Run #10 tested marker clamping/order, collapsed-marker recovery, invalid restore, Random Start at 0/50/100%, extreme regions, interpolation bounds, natural endings, final-boundary fade-to-zero, and forward/reverse region reads; UI compiled but was not DAW interaction-tested |
| Pitch | Source/Target keys, Transpose, Fine Tune, MIDI pitch/root behavior | Run #13 tested `NONE`, shortest signed tonic intervals, the upward tritone tie, Transpose, Fine Tune, MIDI Pitch off/on, the C5/MIDI 72 root, combined semitone/ratio math, finite bounds, persisted per-source settings, and voice increments; the editor and global APVTS controls compiled but were not DAW interaction/save-reopen tested |
| Stretch | Pitch preservation, cache reuse/invalidation, off-audio-thread preparation | Run #19 tested 0.5x/1.0x/2.0x duration, approximate 440 Hz pitch preservation, finite/bounded output, 1.0x buffer reuse, persisted/clamped ratios, explicitly gated stale-revision rejection, removal during an active job, playback across replacement, and deferred old-version reclamation; the single-worker and immutable-publication paths compiled, but no DAW listening test or maximum-size teardown timing test was run |
| Voices | 16 voices, oldest stealing, Mono/Poly, Note Off, rapid notes/chords | Run #23 tested a full 16-note POLY chord, fixed-capacity oldest stealing, MONO replacement through the steal crossfade, release of prior POLY voices when entering MONO, and Note Off release; DAW MIDI interaction was not run |
| Chance/Mask | All six effects; 2/4/8/16; Note-On-only advance; NORMAL bypass | Not implemented |
| Takes | Full-cycle boundary, latest-8 eviction, deterministic HISTORY replay | Not implemented |
| Length/envelope | Chance → Final Length → Attack/Release ordering | Run #23 tested FULL/natural continuation, an exact forced-length boundary, the Release fade inside that boundary, overlapping Attack/Release shaping, and silence after the event; Chance ordering remains pending Phase 6 |
| Global FX | Bit Crush → Sample Rate Reduction → Output Gain | Only Output Gain implemented |
| State | Automation and project save/reopen, moved/missing sources | Run #23 compiled the appended stable `finalLength` and `voiceMode` APVTS parameters; run #19 tested source UUID, enabled state, Start, End, Source Key, Gain, Transpose, Fine Tune, Stretch, and Weight serialization plus parameter-only pool replacement; global DAW automation/save-reopen and moved/missing files were not host-tested |
| Realtime | No I/O, blocking, allocation/reclamation, or stretch prep in callback | Immutable waveform/prepared data, snapshots, atomic global parameter reads, Note-On pitch/length/mode resolution, fixed voice storage, background stretch publication, active-voice ownership, and deferred control-thread reclamation were code-inspected at `0f9b4fd`; applicable non-UI behavior passed run #23, but a profiler/stress test was not run |

When a feature passes, update [[CURRENT_STATE]] with the exact build, host, sample set, or automated test used.
