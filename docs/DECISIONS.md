---
title: Product Decisions
tags:
  - product
  - decisions
status: active
---

# Product Decisions

These decisions govern implementation together with [[PRODUCT_SPEC_V2]].

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

Legacy state entries for these systems are ignored rather than reinterpreted.

## Deferred

- Any feature not named in [[PRODUCT_SPEC_V2]].
