---
title: Creative Effects Logic Review and Sol Implementation Brief
aliases:
  - Creative Effects Improvement Plan
tags:
  - dsp
  - review
  - implementation-plan
status: implemented-verified
date: 2026-09-11
implementation-model: gpt-5.6-sol
---

# Creative Effects Logic Review and Implementation Record

## Start here

The review findings below were implemented on PR #18 without expanding the approved public surface. The implementation preserves fixed storage, stable surviving parameter IDs, immutable source ownership, and deferred non-realtime reclamation. Exact Windows CI, artifact, and merge evidence belongs in [[CURRENT_STATE]] and [[TEST_MATRIX]].

Implemented in the quality pass:

- H1/S1/X1: shared half-sample boundary ownership, a fixed 4096-entry boundary list with truncation reporting, musical-boundary event completion, and separate grid-change versus transport-discontinuity signals.
- X2/S2: sample-time smoothing and bounded bypass fades, correct hold-region origin, wrapped interpolation taps, and bounded loop-seam blending.
- F1/F2: true prepared 2x polyphase-IIR oversampling, fixed dry-path latency compensation, and distinct Amount versus FILTER MORPH axes.
- M1/M2/M3: smoothed overlap-energy normalization, ratio-selected filtered histories, and lifetime-relative grain motion.
- V1: bounded atomic DSP telemetry for the existing compact visual panels.

Read `AGENTS.md`, [[TARGETED_REDESIGN_PLAN]], and this note before implementing. Preserve the four-effect identity and order:

`mixed sampler voices -> SCRAMBLE -> FRACTURE -> SPECTRAL DRAW -> SMEAR -> OUTPUT`

The highest-value work is timing correctness and smooth transitions, followed by antialiasing and grain gain consistency. Further random modulation is lower priority until these foundations are reliable.

### Baseline and evidence

- Local repository: `C:\Users\Anonto\Documents\Codex\2026-09-02\x20-role-you-are-a-senior\outputs\RandomChopSampler`.
- Reviewed clean local branch `rework/targeted-redesign`, commit `3a18c585f73127f6a625903e03e37b4a4727360c`.
- [PR #18](https://github.com/idkanonto/samplerthingidk/pull/18) was still open on September 11, 2026, with remote head `f8cafede525b6fec9e347c1aa9340117cdc32bfc` and base `8726eadf5fb8c2143b89c56234c68d27ad157acd`.
- Compared the local HostGrid, TemporalEffects, CreativeEffects, SpectralDraw, PluginProcessor, PluginEditor, and Phase1Tests source text against that exact remote head: all seven matched after normalizing line endings and trailing whitespace. Local and remote commit/tree identities differ; do not force-push the local history.
- Earlier [Windows run #63](https://github.com/idkanonto/samplerthingidk/actions/runs/34305983948) passed for the redesign. That is baseline evidence, not verification of these proposals. This review used source inspection and small arithmetic reproductions, not a new C++ build, profiler, or DAW listening pass.
- CodeRabbit auto-skipped PR #18. Treat any later review as supporting evidence; do not wait indefinitely or substitute it for engineering judgment.

The project brain has now been reconciled to [[TARGETED_REDESIGN_PLAN]] plus this implemented hardening pass. Historical Gate sections in [[CURRENT_STATE]] are explicitly labeled rather than treated as current behavior. Do not restore Codec, comb/formant stages, Rate, presets, Start Range, Final Length, or public Attack/Release from old commits. The plan's earlier mention of FRACTURE pre-emphasis was not implemented and has not been added as an unrequested stage.

### Confidence and priority

**Confirmed** identifies the original code-path evidence. **Risk** identified a mechanism requiring a focused test; **Candidate** identified comparative tuning rather than a product commitment. The implementation dispositions above are authoritative; remaining listening/profiling boundaries are retained below.

| Priority | Item | Evidence | Expected benefit |
|---|---|---|---|
| P1 | H1: preserve every grid boundary across blocks | Confirmed arithmetic | Consistent timing in realtime and offline renders |
| P1 | S1: end SCRAMBLE by musical boundary | Confirmed arithmetic | Remove accidental empty grids |
| P1 | X1: distinguish timing edits from audio discontinuities | Confirmed path | Avoid a dry-path dropout on Grid changes |
| P1 | X2: smooth parameter and bypass transitions | Confirmed abrupt paths; audible severity unmeasured | Remove control-induced clicks and level jumps |
| P1 | S2: correct hold-region offsets and loop joins | Confirmed offset mapping; seam risk | More reliable, cleaner repeats |
| P1 | M1: stabilize grain overlap gain | Confirmed arithmetic | Stop unrelated grains jumping in gain |
| P2 | F1: properly band-limit FRACTURE nonlinear processing | Confirmed weak filtering; alias level unmeasured | Cleaner aggressive distortion |
| P2 | M2: band-limit pitched grain reads | Confirmed unfiltered speed-up; alias level unmeasured | Crystalline highs with fewer folded artifacts |
| P2 | F2/M3/S3: calibrate macros and motion | Candidates grounded in code | More consistent intensity and useful movement |
| P2 | V1: show actual effect state | Confirmed parameter-only animation | Trustworthy timing and activity feedback |

## Shared timing, transitions, and integration

### H1 — grid boundaries can be dropped at block edges

**Where:** `Source/HostGrid.h:91–105`; fallback branch at `:120–141`.

The host path chooses a boundary by its continuous PPQ, rounds its offset, then rejects an offset equal to `numSamples`. The next block starts beyond the original PPQ boundary and never emits it. The fallback path instead clamps such a rounded event into the previous block. These are inconsistent quantization policies.

Arithmetic reproduction of the current formulas at 48 kHz, 123 BPM, 1/16:

- Ideal spacing: `5853.658536585366` samples.
- One 24,000-sample block emits `[0, 5854, 11707, 17561, 23415]`.
- Blocks of `[5854, 18146]` emit `[0, 11707, 17561, 23415]`: the first nonzero boundary disappears.

**Implementation:** define one absolute sample-quantization rule for host and fallback clocks. Carry a rounded event into the next block or enumerate with the appropriate half-sample ownership interval, and deduplicate by musical boundary identity. Do not fix this by clamping into the previous block or by widening epsilon without an ownership rule. Preserve phase across contiguous blocks and reset that continuity only on a real timing discontinuity.

The 64-entry array also intentionally loses later events in sufficiently large blocks. Keep fixed memory, but process bounded subspans or use a bounded-memory boundary iterator so a long offline block observes every event. Loops may scale with block length; no allocation or unbounded retry is needed. Validate finite PPQ before converting it to an integer index.

**Acceptance:** identical absolute event indices for one block and partitions of 1/17/64/511/5854 samples, fractional tempos 123/137/173, negative PPQ, all grids, host/fallback clocks, and blocks containing more than 64 boundaries. No duplicate, early, or missing event; explicit seek/reset expectations.

### X1 — Global Grid edits unnecessarily clear Spectral Draw audio

**Where:** `HostGrid.h:81,114`; `PluginProcessor.cpp:203–209`; `SpectralDraw.cpp:241–242`.

Changing division sets `transportDiscontinuity`. The processor forwards that flag to Spectral Draw, which resets the STFT and the 1024-sample dry delay. Even at Depth 0, a held note can therefore be interrupted by 1024 zero samples (about 21.3 ms at 48 kHz). This is independent of any artistic spectral attenuation.

**Implementation:** represent grid-reconfiguration and actual transport/audio discontinuity separately. Re-grid SCRAMBLE without resetting Spectral Draw for a division edit. Decide seek, loop, stop/start, and host/fallback behavior explicitly for each stage. Smear currently keeps its tail across these transitions; Fracture keeps filter/modulation state. Preserve that where intentional, and test it rather than adding a blanket reset.

**Acceptance:** sustained input, Spectral Depth 0, Global Grid changes: output remains the exact continuous 1024-sample delayed reference. Separate seek tests verify the chosen stale-audio policy.

### X2 — add audio-rate smoothing and bounded bypass fades

**Where:** `CreativeEffects.cpp:117–155,357–385`; `TemporalEffects.cpp:250–264`; `SpectralDraw.cpp:243–275`; `PluginProcessor.cpp:212–213`.

Parameters are sampled once per host block and applied immediately. Fracture resets filter/DC state immediately at zero, Smear kills all grains, Scramble cancels an event, Spectral Draw switches abruptly to delayed dry, and Output changes gain in one step. Continuous internal LFOs do not smooth those external transitions.

**Implementation:** use persistent sample-time smoothers for continuous controls. Initial tuning targets: 5–10 ms for wet/output gains, 10–30 ms for drive/morph/frequency targets; verify by listening. Do not restart a ramp every block if the target is unchanged. Use a linear smoother when zero must be reachable. Preserve Scramble's next-boundary arming: smoothing its wet output must not start a gesture early. Latch structural decisions per event/grain, while smoothing gain and motion changes independently.

Distinguish *already bypassed* from *transitioning to bypass*. An already-zero instance stays exactly dry; a newly disabled effect may fade over a short fixed duration, then enter exact bypass. Clear/invalidate internal state only after the fade. Update the existing immediate-cancel tests to reflect this deliberately documented transition, without weakening exact steady-state bypass.

For Spectral Draw, the OLA ring contains frames generated with earlier Depth/mask values. Simply smoothing Depth at frame creation does not fix the immediate output-path switch. Crossfade latency-matched dry/wet outputs and allow overlap state to settle. Smooth successive bin-gain targets over hops for sharp canvas edits; keep the existing immutable canvas publication. Define the scanner phase as input/frame-centered or output-audible time and test that convention before adjusting any phase offset.

**Acceptance:** sustained sine, transient, and stereo music with off/on, 0–100 steps, rapid automation, Output ramps, mask Clear/replace, and scan-rate changes. Compare partitions with the same absolute control-change positions. Bound added transition residuals relative to a reference; do not use a global adjacent-sample ceiling that rejects intentionally distorted material.

## SCRAMBLE

Keep the two-second ring, explicit arm state, separately salted RNG, bounded eight-slice plans, and integrated reverse/pitch/hold gestures. They are a sound foundation.

### S1 — rounded duration and host grid disagree

**Where:** `TemporalEffects.cpp:149–168,271–275,307–309`.

An event uses `round(samplesPerGrid)` or twice that duration. At the next boundary, `beginEvent` refuses to start if the previous event is still active. At 48 kHz/123 BPM, an event starting at 5854 with length 5854 stays active through sample 11707. The valid boundary at 11707 arrives before `endEvent` runs, so it is missed. This is separate from H1 and survives fixing the enumerator.

**Implementation:** retain event length in grid units and terminate on the intended future boundary before deciding the next activation. Do not let an independently rounded countdown veto a boundary. Handle one-grid and two-grid gestures, tempo changes, event fades, and same-block boundaries. Recheck `recordDuringEvent` against the maximum actual lifetime: extending an event must never allow the writer to overwrite its captured history. The simplest safe response to a major retime may be to end/re-arm, without copying audio.

**Acceptance:** low Amount (one-grid events) with continuously primed history activates on every eligible boundary at fractional tempos; high Amount skips only boundaries intentionally covered by a two-grid event. Instrument event identities and captured-range ownership. Test re-enable at several offsets and after seeks.

### S2 — hold offset selects phase instead of the intended subregion

**Where:** `TemporalEffects.cpp:124–130,292–297`; `readCaptured` at `:222`.

The hold gesture chooses an offset within the available slice, then wraps `offset + localRead` by the *short loop length*. For available length 750, loop length 105, offset 200, playback starts at 95 and loops inside the first 105 frames, instead of looping the intended `[200,305)` subregion.

**Implementation:** distinguish source-region origin, loop start, and read phase. Add the chosen loop start outside the phase modulo. Ensure both fractional interpolation taps wrap inside the selected loop, not merely the whole capture. Add a short bounded two-read crossfade at internal loop seams; the current dry taper covers slice edges only and cannot smooth repeated wraps inside a slice. Keep both channels phase-linked. Avoid full capture copies and keep the shortest loops bounded.

**Acceptance:** ramp/index-coded capture tests prove the chosen subregion and wrap taps; unrelated samples outside the selected loop cannot leak in. Use discontinuous loop endpoints to test the seam fade while retaining recognizable high-intensity repeats. Include reverse and octave reads.

### S3 — tune perceived amount only after timing fixes

The count changes from four to eight slices as Amount rises. More selected slices does not guarantee more *time* affected: just below 25%, 2/4 slices are selected; at 25%, 2/5 are selected. Tiny positive Amount also starts with roughly 20% slice wetness, while zero is dry. At 400 BPM, 1/32, maximum Amount, a slice is about 113 samples at 48 kHz but the fixed fade is 120 samples; its maximum envelope is only about 0.47.

**Candidate:** use a consistent eight-slot planning domain or a fractional time-coverage budget; maintain musical variation without relying on probability to make the effect audible. Map wetness continuously from zero and cap edge-fade length relative to slice duration. Measure wet time/energy as well as selected count. Avoid rebuilding the entire engine or making all seeds sound alike. Related tuning remains tracked in [[FUTURE_IDEAS]].

## FRACTURE

Keep the focused distortion plus low/band/notch/high morph, TPT filter, shared stereo modulation, envelope response, seeded drift, and DC control. Do not reintroduce competing sub-effects.

### F1 — two substeps are not sufficient antialias filtering

**Where:** `CreativeEffects.cpp:85–97,205–233`.

The current path linearly interpolates two input points and averages their nonlinear/filter outputs. This has only weak resampling filtering; high-pass/notch morph positions cannot serve as a general antialias low-pass. Folding/clipping generates harmonics, and the final base-rate `tanh` adds another nonlinear stage after the substeps. Aliasing severity has not been measured in this review.

**Implementation:** compare properly filtered 2x and 4x oversampling using the existing pinned JUCE version. Place all substantial nonlinear shaping inside the oversampled domain; run filters with the correct internal sample rate. Prefer the smallest factor meeting the quality target. Preallocate for the prepared maximum block size and safely subdivide unexpectedly large blocks. If `juce::dsp::Oversampling` is used, link the existing JUCE DSP module explicitly; no unrelated dependency is needed.

Account for oversampler latency in the internal dry/wet mix and host-reported total latency, including Spectral Draw. Keep latency constant across bypass/Amount changes. Verify impulse and mixed-path alignment; never toggle oversampling modes in the callback. ADAA is an alternative only if measurements show a clear benefit and its additional implementation/testing cost is justified.

**Acceptance:** fixed sustained tones and two-tone tests at 44.1/48/96 kHz, quiet through hot levels, across morph positions and amounts, compared with a higher-rate offline reference. Separate wanted harmonics from folded components. Record alias reduction, latency, peak/RMS/DC, and worst callback time. A useful initial target is at least 12 dB reduction in the chosen worst-case alias metric without losing the intended fold/clipping character; revise only with documented evidence.

### F2 — calibrate the two axes and gain

`fractureCharacter`, labelled FILTER MORPH, simultaneously changes waveshape, base cutoff (`260 * 20^character`), and filter response. Consequently a sweep changes distortion and bass content as well as morph. This is confirmed coupling, not automatically a bug. The compensation formula and fixed response multipliers cannot establish consistent perceived loudness across input levels or responses.

**Candidate:** first smooth the existing mapping, then compare it against a mapping where Amount owns distortion progression and Filter Morph primarily owns tonal response, with mild secondary coupling only if it sounds better. Compare level-matched drums, bass, voice, and sustained chords. Preserve useful dynamics; do not add an automatic loudness follower, limiter, or extra controls to conceal poor calibration. The modulation engine already has coherent motion; add no further LFOs unless a concrete listening problem remains.

## SMEAR

Keep six fixed grains, safe read-behind history, Hann windows, restricted musical intervals, transient suppression, bright emphasis, and bounded feedback.

### M1 — instantaneous grain count introduces gain steps

**Where:** `CreativeEffects.cpp:410–454`.

Normalization is `0.96 / sqrt(activeGrains)`. A newborn grain is counted while its Hann window is zero, so the existing texture jumps down despite receiving no new signal. Three to four grains changes gain from 0.554256 to 0.48, a 13.4% step. Grain retirement produces the reverse problem.

**Implementation:** use smoothly varying gain based on expected overlap or a smoothed overlap-energy estimate with a safe floor and ceiling. A calibrated constant overlap gain is a simpler candidate. Do not divide directly by instantaneous window energy in a way that cancels the individual grain fade near silence. Preserve headroom before brightness/feedback and avoid increasing the six-grain limit.

**Acceptance:** controlled constant-tone grains with known birth/death times show no count-induced gain discontinuity. Verify sparse/dense overlap, startup, shutdown, and the nonlinear output path. Compare against the current normalization on an identical schedule.

### M2 — upward grains need filtering before faster reads

**Where:** `CreativeEffects.cpp:283–293,312–325,425–434`.

Pitch choices reach +24 semitones (4x speed), with pitch orbit on top. Linear interpolation alone does not band-limit that read. At 48 kHz a 9 kHz source component read at 4x appears at 36 kHz and folds to 12 kHz. The bright emphasis can accentuate folded energy. Cubic interpolation alone would not solve this either.

**Implementation:** evaluate a fixed-tap, ratio-aware band-limited reader or a small preallocated filtered-history bank for the existing pitch families. Choose cutoff from the maximum instantaneous read ratio, including orbit. Keep filter/kernel preparation outside realtime, stereo timing identical, and reads within valid history with room for all interpolation taps. Preserve the current conservative read-behind safety; this review did not demonstrate an out-of-bounds read.

**Acceptance:** isolated pitched sine/chirp tests, alias-energy comparison to a high-quality offline resample, stereo/mono fold-down, short history, fastest ratio, live Amount change, and multiple sample rates. Preserve the airy texture through listening rather than compensating lost alias energy with more gain.

### M3 — make motion and feedback consistent across sample rates

Grains last roughly 24–76 ms, but their pitch orbit runs at 0.18–0.90 Hz and pan orbit at 0.10–0.54 Hz. Much of the perceived change may therefore be a randomized initial detune/pan rather than travel within each grain. `brightness` is currently a gain multiplier, not a per-grain spectral tilt. These are opportunities to test, not reasons to add controls.

**Candidate:** use a restrained grain-lifetime-relative pitch/pan arc, or shared continuously evolving cloud motion sampled coherently by grains. Keep pitch intervals and stereo mono-compatibility. Compare against the existing slow orbit before changing the sound.

The feedback smoother uses fixed coefficients 0.82/0.18 per sample (`:460–462`), so its time constant changes with sample rate (about 0.105 ms at 48 kHz). Derive it from seconds, then measure impulse/burst decay. The host advertises a two-second tail (`PluginProcessor.h:28`), while history plus feedback needs a measured decay bound. Test 10 seconds of silence after a burst and reconcile the reported tail if needed; do not assume coefficient magnitude alone proves an adequate tail report or perceptual stability.

## Visual feedback and realtime cost

### V1 — visualizers currently illustrate parameters, not live DSP

**Where:** `PluginEditor.cpp:313–420,953–966`.

The UI invents a timer phase and reads only parameter values. SCRAMBLE's scanner does not show the host boundary or armed/active state; its cells are not the chosen slices. FRACTURE's contour is not the actual cutoff/morph, and SMEAR draws six motifs regardless of active grain count.

**Implementation:** publish small bounded telemetry from the audio thread: Scramble armed/active/phase and selected-slice bits; Fracture actual morph/cutoff and a level statistic; Smear grain activity and compact positions/energy if useful. Use lock-free scalar atomics for independent indicators, or a fixed snapshot mechanism for correlated data. Existing plain-float DSP getters are not safe for concurrent UI reads. Publish once per block or bounded cadence, keep painting on the message thread, and do not copy audio/canvas buffers or allocate in the callback. Retain the compact layout.

Profile before optimizing. Fracture evaluates all four shapes for each substep, and Smear performs trigonometric/power operations for every live grain sample; these are CPU candidates, not measured bottlenecks. After correctness, consider adjacent-shape-only evaluation, prepared window tables, and interpolated control-rate updates with explicit error bounds. Preserve audio-rate envelopes and modulation quality. Measure worst-case callbacks around STFT hops at small buffers, not just average render speed.

## Verification evidence and remaining limits

`Tests/Phase1Tests.cpp` now adds direct H1/S1/X1/M1 coverage alongside its bypass, finite-state, seed-repeatability, STFT reconstruction, and broad integration suite. It checks block-partition boundary equivalence, long fixed-capacity enumeration, grid-edit separation, fractional-tempo Scramble activation, bounded disable transitions, fixed Fracture bypass latency, Spectral smoothing settlement, and Smear overlap-gain bounds. `bufferFiniteAndBounded` still accepts peaks up to 64; that is a corruption guard, not a usable-level claim. Difference RMS can increase because of attenuation, delay, level, or aliasing, and synthetic renders do not establish subjective musical quality.

Remaining external work is comparative DAW listening across drums, bass, voice, sustained harmonics, impulses, and high-frequency tones; dedicated alias-energy comparison to a higher-rate reference; and callback profiling around STFT hops at small buffers. Preserve raw floating-point measurements and level-matched A/B renders when doing that work. Inspect sample/true peaks, RMS, DC, mono level, transient changes, spectral aliasing, and decay; never infer brightness improvement solely from first-difference RMS.

## Implementation disposition

H1/S1/X1, X2/S2/M1, F1/M2, the accepted F2/M3 tuning, and V1 were implemented together on the preserved PR #18 ancestry. S3 remains a listening-led candidate in [[FUTURE_IDEAS]]. Documentation is reconciled in the same implementation batch. CodeRabbit auto-skip is recorded as a missing second opinion, not a quality gate; compilation, CTest, artifact integrity, source review, and post-merge CI remain authoritative.

## Primary engineering references

- [JUCE SmoothedValue](https://docs.juce.com/master/classjuce_1_1SmoothedValue.html): existing framework support for persistent gain/control ramps; multiplicative smoothing cannot reach zero.
- [JUCE Oversampling](https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html): filtered up/downsampling and latency facilities. Verify API availability against this repository's pinned JUCE 8.0.13 before using it.
- [Julius O. Smith, Delay-Line and Signal Interpolation](https://dsprelated.com/freebooks/pasp/Delay_Line_Signal_Interpolation.html) and [Filtering and Downsampling](https://ftp.dsprelated.com/freebooks/sasp/Filtering_Downsampling.html): engineering basis for interpolation and antialias filtering around faster reads.

These references inform the recommendations; they do not prove this plugin's audio quality. No external source was copied in this review. Prior repository research is recorded in [[TARGETED_REDESIGN_PLAN#Research and licensing]]; verify a specific file's license before future reuse.
