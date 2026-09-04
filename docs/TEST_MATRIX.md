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
| Build | Clean Release VST3 and Standalone builds | [Windows Release CI run #26](https://github.com/idkanonto/samplerthingidk/actions/runs/33901896198) at `239fc77` built the Release VST3 and test targets, passed CTest, verified the bundle/module and license resources, and uploaded the inspected artifact; Standalone was not built in this run |
| Formats | Mono/stereo WAV, AIFF/AIF, MP3, FLAC at 44.1/48/96 kHz | Run #5 decoded a mono 44.1 kHz WAV fixture; AIFF/AIF, MP3, FLAC, stereo, and other rates remain code-inspected but not runtime-tested |
| Pool | 20 accepted, 21st rejected; remove/clear; missing-file restore | Run #5 tested 20 accepted, the 21st rejected, and clear/restore; remove and missing-file restore remain code-inspected |
| Selection | Disabled never selected; Weight distribution is plausible; seeded repeatability | Run #5 tested that disabled/missing sources are never selected and exercised a 1:9 weighting over 10,000 choices; seeded repeatability remains code-inspected |
| Random region | Starts always within manual Start/End | Run #10 tested marker clamping/order, collapsed-marker recovery, invalid restore, Random Start at 0/50/100%, extreme regions, interpolation bounds, natural endings, final-boundary fade-to-zero, and forward/reverse region reads; UI compiled but was not DAW interaction-tested |
| Pitch | Source/Target keys, Transpose, Fine Tune, MIDI pitch/root behavior | Run #13 tested `NONE`, shortest signed tonic intervals, the upward tritone tie, Transpose, Fine Tune, MIDI Pitch off/on, the C5/MIDI 72 root, combined semitone/ratio math, finite bounds, persisted per-source settings, and voice increments; the editor and global APVTS controls compiled but were not DAW interaction/save-reopen tested |
| Stretch | Pitch preservation, cache reuse/invalidation, off-audio-thread preparation | Run #19 tested 0.5x/1.0x/2.0x duration, approximate 440 Hz pitch preservation, finite/bounded output, 1.0x buffer reuse, persisted/clamped ratios, explicitly gated stale-revision rejection, removal during an active job, playback across replacement, and deferred old-version reclamation; the single-worker and immutable-publication paths compiled, but no DAW listening test or maximum-size teardown timing test was run |
| Voices | 16 voices, oldest stealing, Mono/Poly, Note Off, rapid notes/chords | Run #23 tested a full 16-note POLY chord, fixed-capacity oldest stealing, MONO replacement through the steal crossfade, release of prior POLY voices when entering MONO, and Note Off release; DAW MIDI interaction was not run |
| Chance/Mask | All six effects; 2/4/8/16; Note-On-only advance; NORMAL bypass | Run #26 tested each Chance effect independently, 0% without RNG drift, 100% activation, fixed-seed explicit decision equality, Repeat values, legal signed Skip, non-identity four-piece Reorder from the resolved start with a one-second cap, bounded signed Bend, eight-slot Drop smoothing, and finite/bounded combinations; Step Mask is not implemented |
| Takes | Full-cycle boundary, latest-8 eviction, deterministic HISTORY replay | Not implemented |
| Length/envelope | Chance → Final Length → Attack/Release ordering | Run #26 exercised combined Chance rendering through Final Length and its Attack/Release boundary; run #23 separately tested FULL/natural continuation, the exact forced boundary, envelope overlap, and silence after the event |
| Global FX | Bit Crush → Sample Rate Reduction → Output Gain | Only Output Gain implemented |
| State | Automation and project save/reopen, moved/missing sources | Run #26 compiled stable appended APVTS IDs for the six Chance percentages and Repeat Size/Count in addition to Phase 5 controls; source-state serialization has automated coverage, while global DAW automation/save-reopen and moved/missing files remain without a host test |
| Realtime | No I/O, blocking, allocation/reclamation, or stretch prep in callback | Fixed-size Chance decisions, bounded resolution, zero render-time RNG, fixed voice storage, immutable prepared data, atomic parameter reads, background stretch publication, and deferred control-thread reclamation were code-inspected at `239fc77`; applicable behavior passed run #26, but a profiler/stress test was not run |

When a feature passes, update [[CURRENT_STATE]] with the exact build, host, sample set, or automated test used.
