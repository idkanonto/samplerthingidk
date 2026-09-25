---
title: Current Implementation State
aliases:
  - Current State
tags:
  - implementation
  - current-state
status: active
verified: 2026-09-25
---

# Current Implementation State

The final workflow-cleanup implementation at code head `b98a7203c164198fe6789db4fde929489db36a14` passed [Windows Release CI run #103](https://github.com/idkanonto/samplerthingidk/actions/runs/36178190471). The workflow completed the Windows VST3/Standalone/test build, tests, render checks, artifact verification, and uploads. Artifact [`recompiler-dll-Windows-VST3`](https://github.com/idkanonto/samplerthingidk/actions/runs/36178190471/artifacts/10883850282) is 3,463,518 bytes with GitHub SHA-256 `7538ab6f47e9570151127d7266476d2359f9b1c1bb1d87beb26cefa6d4f36f4a`.

## Verified final workflow cleanup

- The embedded React/WebView interface uses the bundled Geist Pixel Square face and renders its visible interface copy in lowercase while preserving the established monochrome pixel direction.
- Source Key and Play In Key are removed from the active workflow. Playback pitch is Transpose + Fine Tune + global Pitch plus the MIDI offset from neutral C5/MIDI 72; legacy key fields remain inert for state compatibility.
- Output is a 0–125% level control with exact mute at 0%, unity at 100%, and bounded gain above unity. The compact Output card also owns the global -12 to +12 semitone Pitch control.
- Browser file drops use WebView2 additional objects to pass real Windows paths into the existing importer. The JUCE 8.0.13 patch is deterministic, checks the embedded-resource origin, and the embedded browser rejects navigation away from the packaged UI.
- The global strip now contains only Chords and POLY/MONO. Spectral Reset is promoted to the Spectral Draw header, and source controls are limited to Transpose, Fine Tune, and Gain.

The monochrome-reference code head `833a0429a4f3de5c50eb8e90e3e7ab0cb3a371e2` on [PR #26](https://github.com/idkanonto/samplerthingidk/pull/26) passed [Windows Release CI run #91](https://github.com/idkanonto/samplerthingidk/actions/runs/35163505680). The workflow compiled the VST3, Standalone, and tests; CTest passed 1/1; all 16 deterministic listening renders were non-empty; and the runner verified the complete raw VST3 bundle. Artifact `10474097579` is 3,251,014 bytes with GitHub/upload SHA-256 `7cc62359c9373502a46ec0d8b95c8dee49c1238b99e528d412bd36b5b489e67b`. Its verified bundle contains a non-empty 7,477,248-byte Windows module at `recompiler.dll.vst3/Contents/x86_64-win/recompiler.dll.vst3`. The PR remains unmerged pending a hands-on DAW visual/interaction pass.

## Verified monochrome-reference implementation

- The complete editor now uses the supplied monochrome desktop hierarchy: header with source navigation, sample browser, source waveform/tools, global harmony and voice switches, SCRAMBLE/MELT/SMEAR/SPECTRAL DRAW/OUTPUT cards, tabs, and footer. It defaults to 1024×683 and supports 900×600 through 1536×1024. Code and Windows compilation are verified; exact visual matching still needs a DAW screenshot comparison.
- Add opens a multi-file chooser, while drag-and-drop remains. Sample rows offer enable and audition actions, with individual removal in the sample menu. Source waveform zoom, region focus, fit, and Start/End edits are functional.
- Effect Mode menus check individual SCRAMBLE, MELT, and SMEAR gestures. Each knob still sets intensity. All gestures are on by default, and focused CTest checks exact default/all-on equivalence, zero-mask dry behavior, and audible partial-mask differences. Gesture masks and effect power states persist without changing existing parameter IDs.
- Chords and POLY/MONO switches, random source selection, creative-seed regeneration, Spectral Reset, output mute, and output meter are wired to real behavior. `SEQ` explains automatic timing instead of introducing a sequencer. The information dialog retains the requested `test` text.
- Source preview uses a dedicated prepared voice, bounded selection from the immutable pool, and deferred source reclamation. Spectrum and meter displays publish bounded atomic telemetry; the callback performs no editor painting, file I/O, or heap allocation.

## Historical complete XP editor

The earlier XP-editor head `1742a2d45832f9816d6a94054e60634805b676c9` on [PR #24](https://github.com/idkanonto/samplerthingidk/pull/24) passed [Windows Release CI run #85](https://github.com/idkanonto/samplerthingidk/actions/runs/35026852505). It has since been superseded by the monochrome-reference editor above.

### XP editor details (historical)

- The full editor—not only its information popup—uses a reusable code-native Windows XP visual system across the application bar, source counter, sample browser, source waveform and markers, source controls, global harmony/voice strip, creative-effect cards, Spectral Draw display, Output, buttons, toggles, combo boxes, sliders, and rotary controls.
- The compact responsive layout remains 880×600 by default and supports the existing 760×520 through 1180×820 range. At the default size it keeps six sample rows visible and reserves the lower half for dedicated SCRAMBLE, MELT, SMEAR, SPECTRAL DRAW, and OUTPUT cards.
- The far-right header keeps the source counter beside an XP-style information icon. The icon opens the requested themed modal containing `test` and a Close button.
- This is an editor-only redesign. The sampler controls, parameter IDs, attachments, state compatibility, DSP chain, immutable source ownership, and realtime-audio behavior are unchanged.

The straightforward-sampler head `3dfbc5d55c717275d44ffb6798388d28bb3abbed` on [PR #22](https://github.com/idkanonto/samplerthingidk/pull/22) passed [Windows Release CI run #79](https://github.com/idkanonto/samplerthingidk/actions/runs/34983220916). CTest passed 1/1 in 1.54 seconds, all 16 deterministic listening renders were non-empty, and the workflow verified both the Standalone and complete VST3 bundle. It remains the authoritative behavior baseline; the later sections retain historical evidence only.

## Verified straightforward sampler workflow

- Loading is drag-and-drop only. The pool keeps individual On/Remove actions and stable row selection; Add Samples, Clear All, Enable All, and Disable All are absent.
- Every enabled playable source has equal selection probability. Per-source Weight and manual Stretch are removed from runtime, UI, and rewritten session state; source audio is prepared directly as immutable playback data.
- Per-source Start/End, Source Key, Transpose, Fine Tune, and Gain remain. Global harmony is reduced to Play In Key plus Chords; Chords derives a central MIDI root from Play In Key. POLY/MONO is a two-position switch.
- Global Grid is automatic, choosing 1/8, 1/16, or 1/32 nearest a 125 ms musical slice. Spectral Draw scanning automatically chooses a 2/4/8/16-quarter cycle nearest two seconds. Neither technical timing selector is exposed.
- Spectral Draw uses direct drawing and one Reset action. The editor opens at 880×600 and resizes from 760×520 through 1180×820.
- State version 11 removes retired Root MIDI Note, Global Grid, and Spectral Scan Rate IDs without reinterpreting them. The active sampler/effect parameters, source identity, canvas state, immutable snapshots, and deferred non-realtime reclamation remain compatible.
- Future delivery is the raw VST3 artifact, not a rebuilt unsigned installer. Artifact `10403061289` is 3,198,770 bytes with GitHub and independently matched SHA-256 `6df7215f101346c1af89a06c5ac12012947d168a44b1ec7e1f2b63e2a48f48a0`. It contains the top-level `recompiler.dll.vst3` bundle and a non-empty 7,342,592-byte module at `Contents/x86_64-win/recompiler.dll.vst3`.

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

## Historical MELT replacement baseline (superseded internally)

- The global chain is now `SCRAMBLE -> MELT -> SPECTRAL DRAW -> SMEAR -> OUTPUT`. FRACTURE DSP, latency, controls, editor attachments, visualizer, and active state are removed; the host reports only Spectral Draw's fixed 1024-sample latency.
- MELT arms on a nonzero Amount edge and begins at the next shared-grid boundary. It references the previous grid interval inside a prepared four-second stereo ring, automatically creates two through four slices, and renders each with four-way-overlap Hann grains whose slower analysis hop produces `1.08x`–`4x` pitch-preserving expansion. REVERSE CHANCE is independently clamped and latched once per slice; both channels share timing, region, ratio, and direction.
- Grain windows come from a prepared fixed table. Stretch-dependent overlap normalization, a fixed 24-position/capped slice-level estimate, 6 ms equal-power slice edges, and an 8 ms bypass fade control level and seams. Activation copies no audio, performs at most 96 probe positions/384 interpolated channel reads, and does no allocation, lock, I/O, logging, or variable grid-length scan.
- State version 9 introduces `meltAmount` and `meltReverseChance`. Retired `fractureCharacter`/`fractureMix` state is ignored instead of being reinterpreted as a different sound; older sessions preserve every surviving parameter, canvas, source, identity, and prepared-data ownership contract while MELT restores neutral.
- The compact middle visualizer now shows actual Melt stretch, grid-event progress, and reversed-slice flags through bounded once-per-block atomics; it never reads the audio ring or slice records.
- Run #69 compiled the Release VST3, Standalone, and tests; CTest passed 1/1 in 0.70 seconds; all 16 deterministic 24-bit stereo listening renders were non-empty. The workflow verified the complete `recompiler.dll.vst3` bundle, required notices, a 7,461,376-byte module at `Contents/x86_64-win/recompiler.dll.vst3`, and a 7,512,064-byte Standalone.
- Artifact `10323415507` (`recompiler-dll-Windows-VST3`, 3,249,432 bytes) has GitHub and independently downloaded SHA-256 `15fde813f21de94716ef322e8eff5d7f14e520d88bf9b3e4fb40f12343358717`. Listening artifact `10323305671` has matching SHA-256 `c45cbb9fb45d555e946b0dea3cd6a73c32e31ef6baa59552d7f06f81eebdbb8b` and contains exactly 16 WAVs.
- MELT controlled-render difference RMS was `0/0.1129920/0.1391314/0.1674489/0.2230484` at Amount `0/25/50/75/100`; zero was sample-identical and transformation rose monotonically. Output RMS was `0.2214279/0.1889629/0.1847518/0.1777822/0.1722324`, with maximum peak `0.9107683` in active renders and no non-finite samples. These are objective guards, not a substitute for DAW listening.
- CodeRabbit auto-skipped PR #19 because the repository is below its automatic-review threshold and produced no review or inline threads. It remains an unavailable second opinion, not an approval authority.

## Verified single-macro MELT and dynamic SMEAR

- The compact editor exposes one MELT Amount control. `meltReverseChance` is retired from the active parameter layout, editor, and DSP interface; state version 10 explicitly ignores that removed ID instead of reinterpreting old values. MELT Amount now derives a bounded per-slice reverse probability internally while retaining automatic 2–4-slice pitch-preserving expansion, deterministic stereo-coherent decisions, next-grid arming, and exact settled bypass.
- SMEAR retains prepared fixed storage and deterministic scheduling but expands its bounded pool from six to sixteen records. Amount raises the target active population from about 2 to 16 while contracting the generated grain-length range from approximately 60–100 ms toward 8–30 ms, with an increasing short-grain bias. Existing grains finish naturally when Amount is lowered.
- Prepared 2048-point sine and Hann tables replace repeated per-grain window/pan oscillator trigonometry. Safe read-behind, three-band history selection, pitch choices, transient-aware wet suppression, overlap-energy normalization, cross-block persistence, hostile-value bounds, and the 10 ms exact-bypass transition remain intact. The editor visualizer scales to sixteen activity marks and makes them smaller as Amount rises.
- Run #72 compiled the Release VST3, Standalone, and tests at code head `a18375061de91b2fcd1f2f10641ac40a12778487`; CTest passed 1/1 in 1.51 seconds (1.57 seconds total). All 16 deterministic 24-bit stereo 48 kHz listening renders were non-empty. The workflow verified the complete bundle, required notices, a 7,460,352-byte Windows module at `recompiler.dll.vst3/Contents/x86_64-win/recompiler.dll.vst3`, and a 7,511,040-byte Standalone.
- Artifact `10330783094` (`recompiler-dll-Windows-VST3`, 3,248,778 bytes) has GitHub and independently downloaded SHA-256 `be87a0ba8b1440763ea8fc75e8bb543551dcc001c6e3bc32f24d8bd99527bed2`. Listening artifact `10330997194` (18,127,383 bytes) has matching SHA-256 `6bb0f6348c6e6a5dc514e14325cd9887538626b1aa6775471d7807e2f4583c30` and contains exactly 16 WAVs.
- SMEAR controlled-render difference RMS was `0/0.0200878/0.0295956/0.0422752/0.0508241` at Amount `0/25/50/75/100`; its above-5 kHz energy share rose monotonically from `0.004699` to `0.008135`, zero was sample-identical, every sample was finite, and the maximum rendered peak was `0.948338`. Focused tests additionally verified that maximum active grains increase with Amount, dense operation reaches at least twelve simultaneous grains, and the last dense grain is shorter than the sparse comparison. These are objective guards, not a substitute for DAW listening.
- CodeRabbit had not produced a review or inline thread after the checks settled. Per project policy it is treated as an optional second opinion; compilation, focused regression tests, source/realtime review, artifact verification, and the required post-merge `main` run are the approval evidence.

## Verified Windows installer delivery

- Passing Windows builds now create `recompiler-dll-Windows-Setup.exe` with Inno Setup while retaining the raw VST3 artifact. The installer preserves the complete bundle at `C:\Program Files\Common Files\VST3\recompiler.dll.vst3`, installs the Standalone app under `C:\Program Files\recompiler.dll`, provides Start Menu/optional desktop shortcuts, and registers an uninstaller.
- Run #75 compiled the unchanged Release VST3, Standalone, and tests; CTest passed 1/1 in 1.18 seconds. It built the installer, silently installed it on the disposable runner, SHA-256-compared every installed VST3 file and the Standalone executable with their build outputs, silently uninstalled it, and verified that installed product files were removed.
- Installer artifact `10360281104` (`recompiler-dll-Windows-Installer`, 4,521,573 bytes) has GitHub and independently downloaded SHA-256 `88d76f4da232fedee441213d78ffbd6bdb9ab2dc4eac5e188741c82776399195`. It contains the 5,073,006-byte setup executable plus its checksum file; independent inspection calculated setup SHA-256 `0aa63bbd1d153ec02cb8c1e3d51829c7911fb6f19dc5fb3506e52652e96c2553`, exactly matching the packaged checksum.
- The installer is not code-signed. Windows SmartScreen may warn until a trusted Authenticode certificate is available; this is a distribution-trust limitation, not an installer-integrity failure.

## Remaining external verification

- An allocator hook/realtime profiler and hands-on DAW host stress/listening pass are not available in CI and remain explicit external release checks.
- Direct multi-source row-click interaction still requires a hands-on editor check even though the event-routing fix compiles and source identity/selection behavior remains covered structurally.
- Pixel-level comparison of the current editor with the supplied reference, host interaction checks for every visible action, and subjective DAW listening remain external release checks. The downloaded artifact reference was obtained, but an independent local ZIP rehash could not be completed because the temporary file URL rejected shell authentication; the GitHub metadata and uploader digest agree.

See [[TEST_MATRIX]] for what is runtime-verified and [[REALTIME_AUDIT]] for the callback contract.
