---
title: Current Implementation State
aliases:
  - Current State
tags:
  - implementation
  - current-state
status: active
verified: 2026-09-12
---

# Current Implementation State

The creative-effects logic code head `d129a402b5db309c41428b4a0d1a39003f84fd5b` on [PR #18](https://github.com/idkanonto/samplerthingidk/pull/18) passed [Windows Release CI run #64](https://github.com/idkanonto/samplerthingidk/actions/runs/34676232538). The final section below is authoritative for this branch and supersedes the historical Gate B/C and earlier targeted-redesign internals. The verified pre-rework remote baseline and prior creative-quality evidence remain in history; they are not the current implementation contract.

Gate E PR #15 passed [Windows Release CI run #51](https://github.com/idkanonto/samplerthingidk/actions/runs/34164215738) at code head `027c57b8c0c30bf5450042de64f006c1c9e92fb0`. CTest passed 1/1 in 0.13 seconds. Artifact `10033734310` had GitHub SHA-256 `73cfed01c3e2cd781bd7ada26a185797fedad33751a994838ee3c3ca22ff9b3f`; the workflow verified the complete bundle, required notices, and the non-empty 7,465,472-byte Windows module at `recompiler.dll.vst3/Contents/x86_64-win/recompiler.dll.vst3`. Documentation-only successors and the squash-merged `main` remain subject to the same exact-head workflow gate.

## Verified Gate A implementation

- Visible product name and expected deliverables are `recompiler.dll`, `recompiler.dll.vst3`, and `recompiler.dll.exe`. The CMake target, bundle ID, manufacturer code, and plug-in code remain stable.
- The Gate A sampler core established the 20-source immutable pool; WAV/AIFF/AIF/MP3/FLAC; enabled/missing handling; Weight; Gain; stable IDs; Start/End and random legal starts; Source/Target keys; Transpose; Fine Tune; MIDI pitch/root; fixed 16-voice POLY/MONO; persistence; and deferred non-realtime reclamation. Later targeted redesign removed public Final Length and Attack/Release while retaining bounded internal envelopes.
- Source Stretch now stores `0` as OFF/original, treats `1x` as original, and permits extension through `4x`. Legacy values below `1x` restore as original. Signalsmith preparation remains on the background worker.
- A shared fixed-capacity Host Grid derives 1/8, 1/16-default, or 1/32 sample offsets from block-start BPM/PPQ. It detects discontinuities and falls back to the latest valid BPM or 120 BPM without allocation.
- Take History, Step Mask, and per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop were removed from the trigger, voice, UI, build, and tests.
- Bit Crush was removed. Gate A temporarily left Rate Reduction before Output; Gate C now places it inside CODEC while retaining its parameter ID and sound.
- State version 4 removes obsolete root properties/parameter children and legacy Step/Take nodes before restoring surviving APVTS state. Source state remains separate and preserved; absent temporal controls receive neutral defaults.
- The selected editor source remains keyed by stable source ID; random trigger highlighting does not change the editor selection.
- Gate A tests cover pool/state, weighted selection, pitch, regions, voice/envelope/polyphony, rate reduction, grid behavior, migration, and stretch semantics/publication.

## Historical Gate B implementation (superseded)

- FREEZE and SCRAMBLE process the combined mixed voices before later global stages.
- Both use two-second preallocated stereo rolling histories and reference captured ring ranges in place. Activation performs no heap allocation or large capture copy.
- FREEZE rolls only on shared grid boundaries, captures 1/4, 1/2, or one grid unit, and holds for 1, 2, 4, or 8 grid units. A 3 ms dry/wet edge fade bounds activation/release clicks.
- Freeze Octave Chance resolves once per event. The only pitch increments are 0.5 (-12 semitones), 1.0 (no flip), and 2.0 (+12 semitones); direction uses an equal fixed-seed branch.
- SCRAMBLE rolls only on the shared grid, captures at most one grid unit, divides it into at most eight chunks, and resolves bounded source-chunk/reverse mappings once per activation. Amount controls the probability of manipulating each chunk. Two-millisecond dry windows soften chunk/event edges.
- Each effect owns a separately salted deterministic RNG, so enabling temporal effects does not change seeded sampler-source selection.
- Transport discontinuities invalidate active capture/history in constant time; old ring contents are ignored without clearing a large buffer in the callback.
- Chance defaults are 0%, so fresh instances remain load-samples-and-play neutral.
- Gate B CTest coverage passed on both the PR head and squash-merged `main`.

## Historical Gate C implementation (superseded)

- FRACTURE processes the global mix after SCRAMBLE. Five waveshaper transitions and six stable tonal structures cover soft/asymmetric/folded/clipped/digital character plus low, band, notch, formant, hollow-comb, and metallic-comb filter territory.
- FRACTURE Drive, Character, Filter Morph, Frequency, Resonance, and Mix are automatable. Mix defaults to 0% for a sample-identical fresh-instance bypass.
- Thirty compiled factory presets vary every FRACTURE dimension. Functional previous/dropdown/next controls publish the chosen preset into the real APVTS parameters; no preset parsing reaches the callback.
- SMEAR is a bounded two-head granular short-buffer texture stage with complementary windows and persistent blur state. Its half-second stereo storage is allocated in `prepareToPlay`; Amount 0 is transparent.
- CODEC combines deterministic bandwidth loss, held predictive residuals, and watery/metallic reconstruction with the preserved 1x–64x sample-and-hold Rate Reduction. The old `rateReduction` parameter ID remains stable but is presented and processed only inside CODEC.
- Gate C adds neutral migration defaults and state version 5. Its focused CTest suite and complete build/package workflow passed on both the PR head and squash-merged `main`.

## Verified Gate D implementation

- SPECTRAL DRAW is inserted globally between FRACTURE and SMEAR. It uses a real 1024-point STFT, 256-sample hop, square-root Hann overlap-add, and reports 1024 samples of plug-in latency.
- A 128×64 attenuation-only canvas provides Draw, Erase, and Clear. Bilinear lookup smooths scanner/time and square-root frequency coordinates; Depth defaults to 0% for an exact latency-matched bypass.
- Scan Rate offers 2 beats, 1 bar, 2 bars, and 4 bars. The scanner aligns to finite host PPQ and BPM when available, otherwise continues from the safe tempo fallback.
- Canvas state is clamped, base64-encoded into state version 6, and transferred to the callback through fixed immutable snapshot slots. Audio rendering takes only an atomic read handle; it neither locks nor copies the canvas.
- Gate D tests cover publication immutability, hostile values, persistence, exact bypass latency, empty/full masks, STFT reconstruction, arbitrary block sizes, 44.1/48/96 kHz preparation, scanner wrap/alignment, discontinuity, and live canvas replacement. They passed on both the PR head and squash-merged `main`.

## Historical Gate E implementation (chain superseded)

- The complete runtime order is `FREEZE -> SCRAMBLE -> FRACTURE -> SPECTRAL DRAW -> SMEAR -> CODEC -> OUTPUT`; the surviving 29 APVTS parameter IDs each have exactly one functional editor attachment.
- Runtime and UI source audit found the removed Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, and Bit Crush systems only in migration/tests where legacy state is intentionally rejected.
- Restored and live-edited source Gain/Weight values are finite-clamped before immutable publication. Voice inputs, decoded samples, accumulated output, and envelope state are fixed-cost sanitized so hostile non-finite state cannot poison or indefinitely retain a voice.
- Gate E tests add hostile source/voice state, the exact complete chain under variable block sizes and transport discontinuity, stale stretch-result rejection, source removal while preparation is active, and old prepared-version lifetime coverage.
- Windows run #51 compiled the VST3, Standalone, and tests; passed the full CTest executable; verified packaging; and uploaded the release bundle.
- CodeRabbit was considered only as a second opinion and auto-skipped PR #15 because the repository does not meet its review threshold; it produced no actionable review findings.

## Verified compact editor

- The editor opens at 940×680 instead of 1050×1190 and is resizable from 840×640 through 1280×960.
- Rows divide their available width proportionally, vertical sections scale within bounded limits, rotary values sit beside their knobs, and the source waveform/list/Spectral Draw regions remain present at the minimum size.
- Minimum/default/maximum geometry review found positive bounds for every section. [Windows run #55](https://github.com/idkanonto/samplerthingidk/actions/runs/34175784343) compiled the editor at code head `4a2bae87d39e7997281b62c1dc538cc857f623c9`, passed CTest 1/1 in 0.16 seconds, verified the release bundle, and uploaded artifact `10037311950`.
- This is a layout-only change: parameter IDs, attachments, state, DSP, signal order, and realtime behavior are unchanged.

## Historical targeted-redesign baseline (superseded internally)

- The global chain is now `SCRAMBLE -> FRACTURE -> SPECTRAL DRAW -> SMEAR -> OUTPUT`. Standalone FREEZE and CODEC stages are removed; useful repeat/hold/octave gestures live inside SCRAMBLE, and predictive digital damage plus Rate Reduction live inside FRACTURE.
- The surface has 17 surviving APVTS parameters and 17 functional attachments. Exposed Seed, Freeze controls, SCRAMBLE Chance, technical FRACTURE Drive/Filter Morph/Frequency/Resonance, and CODEC Amount/Quality are removed. State version 7 migrates useful old intensity into the new macros and preserves an internal `creativeSeed`.
- SCRAMBLE gives each eligible grid a monotonic amount-derived slice budget. Fixed gesture decisions provide jumps, micro-loops/holds, reverse, earlier-buffer replay, and 0.5x/2x pitched fragments; adjacent slices can form a repeated motif. Event completion retains valid history instead of creating alternate empty grids.
- FRACTURE combines correlated dual oscillators, bounded smooth random drift, input-envelope movement, five nonlinear regions, six stable filter/formant/comb structures, predictive residual damage, and amount-moderated Rate Reduction. Its DC blocker is sample-rate invariant and feedback/state/output are bounded.
- SMEAR is replaced by six fixed pitched grains over a one-second history with safe read-behind, Hann windows, musically restricted intervals, scatter, stereo placement/crossfeed, high-frequency residual emphasis, and transient-aware wet suppression. Logical history invalidation avoids clearing the large ring on a callback seed change.
- Clicking a sample row can now pass through the row container to the ListBox selection model while its On and Remove child controls remain interactive, fixing the source-edit selection failure.
- Run #60 built VST3, Standalone, and tests; CTest passed 1/1 in 0.83 seconds; all 16 deterministic listening WAVs were non-empty. Artifact `10043755564` had GitHub SHA-256 `c0aedca89cb844dc7766434ed397f5b7da941ba6a9beb93ad30cf4543f285623` and contained the workflow-verified 7,460,352-byte Windows module. Listening artifact `10043756986` had SHA-256 `7bbea7ad4e18169661eadf50e7789390c2acc9c5fee83a6e50f69a49b6a67093`.
- Controlled-render difference RMS increased monotonically at 25/50/75/100: SCRAMBLE `0.0840/0.1329/0.2198/0.2737`, FRACTURE `0.1346/0.2767/0.4658/0.5855`, and SMEAR `0.0165/0.0254/0.0365/0.0437`. All three 0 renders were sample-identical. SMEAR preserved output RMS within about 0.4 dB at maximum while its above-4 kHz energy share rose from `0.00468` to `0.01171`; this is objective render evidence, not a substitute for DAW listening.
- CodeRabbit auto-skipped PR #17 because of its repository review threshold and produced no findings. It is a second opinion rather than the approval authority.

## Verified creative-effects logic hardening

- Host Grid now uses one half-sample ownership convention for host and fallback timing, rounds consistently, separates grid edits from transport discontinuities, and carries up to 4096 fixed boundaries with an explicit truncation flag. Partition-equivalence and long-offline-block tests cover the original lost-boundary failure.
- SCRAMBLE ends one/two-grid events from actual musical boundaries, not independently rounded duration samples. Hold gestures retain their chosen loop origin; fractional taps wrap inside that subregion; repeated seams receive bounded two-read blending; short edge fades cannot exceed half a slice. A 5 ms disable transition settles to exact bypass without arming early.
- FRACTURE is a focused true-2x-oversampled nonlinear/filter effect. Amount and FILTER MORPH use separate 10/20 ms smoothing and distinct mappings; only adjacent soft/asymmetric/folded/clipped shapes are interpolated. A prepared fixed dry delay matches JUCE's polyphase-IIR oversampler latency, which remains constant through bypass and is added to Spectral Draw's reported latency.
- SPECTRAL DRAW maintains its latency-matched dry and live STFT paths while smoothing Depth/bypass over 20 ms and bin targets between transform frames. Global Grid edits no longer clear its STFT or 1024-sample dry delay; real transport discontinuities still invalidate fixed state.
- SMEAR keeps six fixed grains while using lifetime-relative pitch/pan motion, ratio-selected raw/medium/high filtered histories, and smoothed overlap-energy normalization. Its 10 ms disable transition clears grains only after reaching settled exact bypass.
- Output gain uses a persistent 10 ms sample-time ramp. Existing compact effect visualizers now display once-per-block bounded atomic telemetry from actual Scramble selection/progress, Fracture morph/motion, and Smear activity/gain; they never read callback-owned buffers.
- Run #64 built the Release VST3, Standalone, and tests. CTest passed 1/1 in 0.86 seconds, all 16 deterministic listening WAVs were non-empty, and the workflow verified the complete `recompiler.dll.vst3` bundle plus notices and a non-empty 7,513,088-byte module at `Contents/x86_64-win/recompiler.dll.vst3`.
- Artifact `10291988433` (`recompiler-dll-Windows-VST3`, 3,280,376 bytes) has GitHub SHA-256 `482ff60db63a117c864eb7282f2b10be29dc5aa8ddb600012d4bef5e6c7a883c`. Listening artifact `10292008315` has SHA-256 `e354decf9a90ce279f20d73413d9da754c09901497965afd7926e0c8816486b5`.
- CodeRabbit auto-skipped the updated PR because the repository is below its automatic-review threshold. It supplied no finding; compilation, focused regression tests, source/realtime review, artifact verification, and the required post-merge `main` run remain the approval evidence.

## Remaining external verification

- An allocator hook/realtime profiler and hands-on DAW host stress/listening pass are not available in CI and remain explicit external release checks.
- Direct multi-source row-click interaction still requires a hands-on editor check even though the event-routing fix compiles and source identity/selection behavior remains covered structurally.
- Final visual redesign is explicitly outside the current delivery boundary.

See [[TEST_MATRIX]] for what is runtime-verified and [[REALTIME_AUDIT]] for the callback contract.
