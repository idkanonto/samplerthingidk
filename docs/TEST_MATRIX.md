---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

The verified pre-redesign baseline is [Windows Release CI run #41](https://github.com/idkanonto/samplerthingidk/actions/runs/33950109493) at `85f9705`. Gate A evidence is pending; rows that say “Gate A suite” describe committed test intent, not a pass, until the branch CI succeeds.

| Area | Gate expectation | Current evidence |
|---|---|---|
| Packaging | Release VST3 and Standalone use the visible `recompiler.dll` identity; complete bundle contains a non-empty Windows module and notices | Gate A workflow and artifact inspection pending |
| Formats/pool | Decode WAV, AIFF/AIF, MP3, FLAC; enforce 20-source limit; remove/clear/enable; missing restore | Baseline passed run #41; Gate A suite preserves pool/state checks and the workflow's real MP3 fixture |
| Selection/region/pitch | Weighted playable-only selection; legal Start/End; deterministic Seed; full pitch math | Gate A suite covers Weight distribution, disabled/missing exclusion, region bounds, tonic/manual/MIDI pitch math |
| Stretch | OFF and 1x reuse original; 2x–4x duration; background immutable publication; stale/removal/lifetime safety | Gate A suite covers bounds, OFF/1x identity, 4x duration, and publication; stress/lifetime coverage is required again by Gate E |
| Voices | 16 POLY voices, oldest stealing, MONO replacement, Note Off, Final Length, envelopes | Gate A suite covers region safety, Final Length/envelopes, 16-voice capacity, and oldest selection; DAW MIDI remains external |
| Removed systems | No Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, or Bit Crush in runtime/UI/API | Gate A source audit plus legacy-state migration test pending CI |
| Host grid | 1/8, 1/16 default, 1/32; BPM/PPQ; arbitrary blocks; tempo changes; seek/loop; stopped/missing fallback | Gate A suite covers division math, block continuity, seek, rounding, and fallback; broader tempo/transport matrix expands in Gate B/E |
| FREEZE/SCRAMBLE | Boundary activation, determinism, bypass, click safety, bounds | Gate B pending |
| FRACTURE/SMEAR/CODEC | Presets, parameter extremes, bypass, finite bounds, rate-reducer integration | Gate C pending; current Rate Reduction foundation has deterministic/finite tests |
| SPECTRAL DRAW | STFT reconstruction/bypass, mask publication/persistence, scan mapping, latency, bounds | Gate D pending |
| State | Surviving parameters/sources restore; removed entries ignored; new FX/canvas persist | Gate A migration suite pending; full compatibility fixtures required in Gate E |
| Realtime | No callback I/O/locks/allocations/final reclamation; bounded fixed state | [[REALTIME_AUDIT]] code review; allocator/profiler and DAW stress remain external |

Update exact SHA, run ID, artifact digest/module size, and host boundaries only when evidence exists.
