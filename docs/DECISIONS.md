---
title: Product Decisions
tags:
  - product
  - decisions
status: active
---

# Product Decisions

These decisions govern implementation together with [[PRODUCT_SPEC_V2]].

## Release-candidate identity and FAULT lock (2026-10-01)

- Lock the WebView editor to 1080×675 logical pixels with 75/100/125/150 preset scaling.
- Use the supplied RECOMPILER mark in the header and the supplied damnnprodigy and shadx2 marks on ABOUT.
- Replace SETTINGS with the typed, scrollable ABOUT manual; keep UI scale and creative-seed access there and move effect bypasses into module headers.
- Use Spleen for module/control naming and Cozette 1.30.0 for values, counters, metadata, and technical copy.
- Rename the bit-1 FAULT choice from PITCH to RESAMPLE without changing its persisted mask position. RESAMPLE is exactly −12 or +12 semitone varispeed, approximately 50/50 per event.
- Standardize every rotary on one 270° component with 180 logical pixels of vertical full travel, 6× Shift fine control, double-click reset, and wheel adjustment.

## Accepted

- Preserve the sampler core, stable parameter IDs that still exist, source identity, immutable prepared data, and deferred non-realtime reclamation.
- Brand the visible product `recompiler.dll` while retaining standards-compliant VST3 packaging and existing manufacturer/plugin codes plus bundle ID for host continuity.
- Choose the shared 1/8, 1/16, or 1/32 creative grid automatically from host tempo, targeting a stable musical slice duration. Do not expose Global Grid.
- Process global creative effects only after voice mixing and only in the fixed [[SIGNAL_CHAIN]] order.
- Treat CodeRabbit as a second opinion. Compilation, approved behavior, test validity, ownership/lifetime, realtime safety, and packaging findings are actionable; redesign and scope expansion are not authoritative.
- Keep source playback direct and immutable. MELT is the only time-stretching system; do not expose or prepare manual per-source Stretch.
- Make SCRAMBLE, MELT, and SMEAR the three focused one-knob creative effects. Amount remains a perceptual macro rather than a collection of exposed technical controls; MELT reversal probability is an internal amount-derived decision rather than a separate performance axis.
- Fold useful Freeze repeat/hold/octave gestures into SCRAMBLE and remove Freeze as a headline stage.
- Remove FRACTURE and replace its chain position with MELT: an original fixed-storage overlap-add slice stretcher with automatic grid slices and internally latched reversal decisions.
- Replace the old SMEAR blur with a bright, pitched, transient-aware grain cloud whose single macro increases fixed-pool density while introducing progressively smaller grains.
- Keep Spectral Draw and the sampler core intact apart from the approved straightforward-workflow simplification.
- Keep a persisted internal creative seed for coherent state restore but remove Seed from the producer-facing parameter surface.
- Implement a real STFT/FFT overlap-add Spectral Draw processor as its own high-risk gate.
- Batch tests and project-brain updates with the implementation they describe; avoid documentation-only CI churn.
- Automatic tempo-derived grid changes re-grid SCRAMBLE and MELT without treating the change as a transport discontinuity.
- Report only Spectral Draw's fixed 1024-sample latency; MELT adds no host latency.
- Publish only bounded scalar creative telemetry from the callback; visualizers paint live DSP state on the message thread and never inspect audio buffers.
- Publish future Windows test builds as a raw complete VST3 bundle. Do not rebuild or replace the already published unsigned installer, and do not create new unsigned installer executables.
- Supersede the blue XP skin with the user's monochrome, beveled desktop reference across the complete editor. Keep it responsive and compact; match the reference's hierarchy, source browser, waveform tools, five effect cards, and footer. The information dialog remains available from Settings or menus and uses the same monochrome treatment.
- Migrate the final editor surface to a self-contained React, TypeScript, Vite, and CSS frontend hosted by JUCE WebView. Keep the native JUCE editor buildable as a fallback until migration is complete; C++ remains authoritative for parameters, sample/state mutation, and bounded realtime telemetry, and release artifacts embed all frontend files without a localhost or internet dependency.
- Restore Add Samples through a file picker; keep drag-and-drop. Move individual source removal into the sample menu and add per-row audition. Do not restore bulk Clear/Enable All/Disable All.
- Give SCRAMBLE, MELT, and SMEAR Mode menus of checkable creative gestures. Their Amount knobs remain the intensity controls; every gesture defaults on so the engine's prior sound is unchanged. Persist the checklists and effect power states without changing existing parameter IDs.
- Treat the reference's SEQ tab as an automatic-timing explanation, not a new programmable sequencer. Main and FX tabs navigate actual editor surfaces; Settings exposes information.

## Removed

- Take History and its browser/state behavior.
- Programmable Step Mask and its event cursor.
- Per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop.
- Bit Crush.
- Separate FREEZE and CODEC stages.
- Exposed Seed, SCRAMBLE Chance, Freeze Size/Hold/Chance/Octave Chance, every FRACTURE control, and CODEC Amount/Quality.
- Start Range, Final Length, public Attack/Release, FRACTURE RATE, and the FRACTURE preset browser. `fractureCharacter` and `fractureMix` are retired in state version 9 and are not mapped onto MELT.
- Exposed MELT Reverse Chance. `meltReverseChance` is retired in state version 10 and is not mapped onto the one-knob MELT macro.
- Per-source Selection Weight and manual Stretch; enabled playable sources are selected equally and MELT owns creative stretching.
- Clear All, Enable All, and Disable All. Add Samples is restored by the later monochrome-reference decision above; removal and enable state remain per source.
- Root MIDI Note, manual Global Grid, and Spectral Scan Rate. Chords derives its root from Play In Key, while musical timing is automatic.
- Separate Spectral Draw and Erase modes. Dragging draws; one Reset action clears the canvas.
- Any Loop/One-shot mode. The user chooses only whether Chords follows MIDI pitch.

## 2026-09-25 — Final workflow simplification

Accepted:

- Remove Source Key and Play In Key from the active UI and pitch path. Preserve their saved values only as inert compatibility data.
- Anchor Chords to fixed neutral MIDI note 72 so keyboard tracking is independent of hidden project metadata.
- Keep the stable `output` host ID while changing its public control to 0–125% VOL with explicit pre-version-12 saved-state migration; add stable `globalPitch` from -12 to +12 semitones.
- Import Windows Explorer drops through WebView2 additional objects and the same validated native importer used by the chooser.
- Use locally bundled Geist Pixel Square in lowercase for the display layer and Space Mono for values and metadata.
- Put a full-size Reset action in the Spectral Draw header and clear the local canvas immediately before backend confirmation.

Rejected:

- Reintroducing automatic key detection, scale selection, or hidden key correction.
- Treating a browser drop as a request to reopen the file chooser.
- Decorative full-editor grid backgrounds.

Legacy state entries for these systems are ignored rather than reinterpreted.

## 2026-09-28 — Final FAULT product pass

Accepted:

- Replace the live SCRAMBLE and MELT chain with one new `FaultProcessor`; their parameter IDs and state are retired rather than mapped onto a different sound.
- Add stable automatable `faultPressure` from 0–100, default 0. Persist the state-backed mutation mask separately with PULL=1, DUST=2, BEND=4, default 7.
- Schedule FAULT from a dedicated 1/16 clock and group it into only 1/2, 1/4, 1/8, or 1/16 segments. Pressure controls occurrence probability and the division distribution, not wet/dry amount.
- Keep the persisted creative seed and latch one division, occurrence, mutation, and mutation profile for each segment. Reset logical FAULT history on transport discontinuities, clock-source changes, and seed changes.
- Use new fixed-storage overlap-add paths for PULL and duration-preserving BEND, curated DUST profiles, and six-millisecond equal-power event edges. Allocate all FAULT buffers and tables in `prepareToPlay()`.
- Rename the producer workflow to POOL, SOURCE, TUNE, DRIFT, TRIM, STACK, VOICES, BLEED, ETCH/CLEAR, and MASTER while keeping the stable surviving parameter IDs.
- Bundle Spleen 8×16 and 6×12 for native-strike module/control text and IBM Plex Mono regular/semibold for filenames, data, and telemetry. Preserve case for dynamic data.
- Make the WebView2 additional-object bridge the single authoritative OS file-drop path; the JUCE editor-level duplicate drop target is retired. Keep the chooser as fallback.

Retired:

- Active/public `targetKey`, `scrambleAmount`, and `meltAmount` parameters; old values restore inertly and cannot affect sound.
- SCRAMBLE/MELT feature masks, processors, bridge telemetry, UI modules, and synthetic visualizer activity.
- Geist Pixel Square and Space Mono assets.

## 2026-09-30 — Focused slice and source-processing correction

Accepted:

- Keep FAULT scheduling and PRESSURE, but limit its random slice choices to PITCH=1, BITCRUSH=2, and REVERSE=4. STRETCH is not a FAULT choice.
- Implement FAULT PITCH as direct sampler-style resampling across non-zero integer intervals from -12 through +12 semitones. Playback rate is `2^(semitones/12)` and playback duration changes inversely; no overlap-add, formant preservation, time compensation, or post-stretch is permitted.
- Preserve the existing quantize/hold bitcrusher algorithm unchanged under the BITCRUSH label. REVERSE reads only the captured slice from end to beginning.
- Restore independent per-source Stretch at 0.25×–2× speed. Prepare pitch-preserving source versions on the existing bounded background-worker model; source tuning remains separate.
- Remove the dedicated output mute control, state, bridge command, and DSP ramp. MASTER VOL at 0% maps to exact zero gain and is the sole mute path.
- Advance saved state to version 14. Pre-version-14 source Stretch data remains neutral rather than being reinterpreted with the new speed semantics.

Superseded by this decision:

- The 2026-09-28 PULL/DUST/BEND FAULT choice set.
- The earlier decision that manual per-source Stretch stays removed.
- The earlier dedicated output Mute control/state.

## Deferred

- Any feature not named in [[PRODUCT_SPEC_V2]].
