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
| Build | Clean Release VST3 and Standalone builds | [Windows Release CI run #35](https://github.com/idkanonto/samplerthingidk/actions/runs/33938782477) at `a87842b` built the Release VST3 and test targets, passed CTest, verified the bundle/module and license resources, and uploaded the inspected artifact with SHA-256 `66cf70419ad16c131cea1859288e3325661fdf70b9ba92c2148239b70ac6d466`; Standalone was not built in this run |
| Formats | Mono/stereo WAV, AIFF/AIF, MP3, FLAC at 44.1/48/96 kHz | Run #5 decoded a mono 44.1 kHz WAV fixture; AIFF/AIF, MP3, FLAC, stereo, and other rates remain code-inspected but not runtime-tested |
| Pool | 20 accepted, 21st rejected; remove/clear; missing-file restore | Run #5 tested 20 accepted, the 21st rejected, and clear/restore; remove and missing-file restore remain code-inspected |
| Selection | Disabled never selected; Weight distribution is plausible; seeded repeatability | Run #5 tested that disabled/missing sources are never selected and exercised a 1:9 weighting over 10,000 choices; seeded repeatability remains code-inspected |
| Random region | Starts always within manual Start/End | Run #10 tested marker clamping/order, collapsed-marker recovery, invalid restore, Random Start at 0/50/100%, extreme regions, interpolation bounds, natural endings, final-boundary fade-to-zero, and forward/reverse region reads; UI compiled but was not DAW interaction-tested |
| Pitch | Source/Target keys, Transpose, Fine Tune, MIDI pitch/root behavior | Run #13 tested `NONE`, shortest signed tonic intervals, the upward tritone tie, Transpose, Fine Tune, MIDI Pitch off/on, the C5/MIDI 72 root, combined semitone/ratio math, finite bounds, persisted per-source settings, and voice increments; the editor and global APVTS controls compiled but were not DAW interaction/save-reopen tested |
| Stretch | Pitch preservation, cache reuse/invalidation, off-audio-thread preparation | Run #19 tested 0.5x/1.0x/2.0x duration, approximate 440 Hz pitch preservation, finite/bounded output, 1.0x buffer reuse, persisted/clamped ratios, explicitly gated stale-revision rejection, removal during an active job, playback across replacement, and deferred old-version reclamation; the single-worker and immutable-publication paths compiled, but no DAW listening test or maximum-size teardown timing test was run |
| Voices | 16 voices, oldest stealing, Mono/Poly, Note Off, rapid notes/chords | Run #23 tested a full 16-note POLY chord, fixed-capacity oldest stealing, MONO replacement through the steal crossfade, release of prior POLY voices when entering MONO, and Note Off release; DAW MIDI interaction was not run |
| Chance/Mask | All six effects; 2/4/8/16; Note-On-only advance; NORMAL bypass | Run #29 retained the full run #26 Chance suite and tested all four Step Mask lengths, exact cycle boundaries, sequential same-position chord events, reset on length changes including away-and-back edits, the eight-step all-FX default, individual/all edits, state round-tripping and legacy defaulting, NORMAL bypass without RNG drift, and FX allowance; controls compiled but were not DAW interaction-tested |
| Takes | Full-cycle boundary, latest-8 eviction, deterministic HISTORY replay | Run #32 tested current-length full-pass boundaries, incomplete-pass discard on length change, latest-eight capacity and oldest replacement, exact stable ID/prepared version/region/start/Chance replay, repeated HISTORY wrapping, selecting another Take from event one, capture suppression in HISTORY, returning to a fresh LIVE pass, session clearing, and prepared-data lifetime through source clear, Take ownership, eviction, and non-realtime collection; browser controls compiled but were not DAW interaction-tested |
| Length/envelope | Chance → Final Length → Attack/Release ordering | Run #26 exercised combined Chance rendering through Final Length and its Attack/Release boundary; run #23 separately tested FULL/natural continuation, the exact forced boundary, envelope overlap, and silence after the event |
| Global FX | Bit Crush → Sample Rate Reduction → Output Gain | Run #35 tested OFF/1x identity, deterministic 4-bit quantization, independent stereo 4x holds, exact Bit Crush-before-rate-reduction ordering, reduction-phase continuity across buffers, 4–24-bit and 1x–64x mappings, deterministic equality, and finite output bounded to the documented safety range after maximum tested gain |
| State | Automation and project save/reopen, moved/missing sources | Run #35 compiled the appended `bitDepth` and `rateReduction` APVTS IDs in addition to prior stable parameters; Step Mask and source-state serialization have automated coverage, while global DAW automation/save-reopen and moved/missing files remain without a host test |
| Realtime | No I/O, blocking, allocation/reclamation, or stretch prep in callback | The master stage adds only two held floats and bounded scalar loops with no allocation, locks, or RNG. Fixed Take/Step/Chance arrays, atomic handoffs, immutable prepared references, retirement roots, and deferred control-thread reclamation were code-inspected at `a87842b`; applicable behavior passed run #35, but a profiler/stress test was not run |

When a feature passes, update [[CURRENT_STATE]] with the exact build, host, sample set, or automated test used.
