---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Gate A passed [PR run #42](https://github.com/idkanonto/samplerthingidk/actions/runs/34082214626) and [post-merge run #43](https://github.com/idkanonto/samplerthingidk/actions/runs/34082765219). Gate B passed [PR run #44](https://github.com/idkanonto/samplerthingidk/actions/runs/34084189109) and [post-merge run #45](https://github.com/idkanonto/samplerthingidk/actions/runs/34084650160). Gate C passed [PR run #46](https://github.com/idkanonto/samplerthingidk/actions/runs/34140666576) at `08b4ffc` and [post-merge run #47](https://github.com/idkanonto/samplerthingidk/actions/runs/34144904944) at `a5e1f09`. The Gate C artifact (`10025978696`) had digest `f6655154f4d40b91c4ab26e0460f25068fa86f6fa493a843c592b90c6aa9239e` and a workflow-verified 7,433,216-byte Windows module. Gate D rows describe branch test intent until its CI passes.

| Area | Gate expectation | Current evidence |
|---|---|---|
| Packaging | Release VST3 and Standalone use the visible `recompiler.dll` identity; complete bundle contains a non-empty Windows module and notices | Runs #42–#47 passed; Gate C workflow verified the 7,433,216-byte module and required notices |
| Formats/pool | Decode WAV, AIFF/AIF, MP3, FLAC; enforce 20-source limit; remove/clear/enable; missing restore | Runs #42/#43 decoded the writable format matrix plus a real MP3 and passed pool/state checks |
| Selection/region/pitch | Weighted playable-only selection; legal Start/End; deterministic Seed; full pitch math | Runs #42/#43 passed Weight distribution, disabled/missing exclusion, region bounds, and tonic/manual/MIDI pitch math |
| Stretch | OFF and 1x reuse original; 2x–4x duration; background immutable publication; stale/removal/lifetime safety | Runs #42/#43 passed bounds, OFF/1x identity, 4x duration, and publication; stress/lifetime coverage is required again by Gate E |
| Voices | 16 POLY voices, oldest stealing, MONO replacement, Note Off, Final Length, envelopes | Runs #42/#43 passed region safety, Final Length/envelopes, 16-voice capacity, and oldest selection; DAW MIDI remains external |
| Removed systems | No Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, or Bit Crush in runtime/UI/API | Gate A source audit and legacy-state migration test passed runs #42/#43 |
| Host grid | 1/8, 1/16 default, 1/32; BPM/PPQ; arbitrary blocks; tempo changes; seek/loop; stopped/missing fallback | Gate A grid coverage passed runs #42/#43; Gate B adds tempo changes, PPQ offsets, stopped transitions, and all spacing modes |
| FREEZE/SCRAMBLE | Boundary activation, determinism, bypass, click safety, bounds | Runs #44/#45 passed Chance 0/100, Amount 0/low/high, exact and both-sign octave branches, fixed-seed equality, bounded capture, pre-boundary identity, repeated activation, finite output, and transport invalidation |
| FRACTURE/SMEAR/CODEC | Presets, parameter extremes, bypass, finite bounds, rate-reducer integration | Runs #46/#47 passed all 30 preset value/output checks, extreme Drive/Resonance and hostile samples, exact bypasses, Smear cross-block history, Codec fixed-state determinism/silence, and the preserved 1x–64x reducer |
| SPECTRAL DRAW | STFT reconstruction/bypass, mask publication/persistence, scan mapping, latency, bounds | Gate D suite covers immutable publication, persistence/invalid state, exact 1024-sample bypass, empty/full masks, OLA error, 44.1/48/96 kHz preparation, arbitrary blocks, scanner wrap/PPQ alignment, canvas changes, discontinuity, and finite bounds; CI pending |
| State | Surviving parameters/sources restore; removed entries ignored; new FX/canvas persist | Gate A migration passed runs #42/#43; Gate B adds neutral defaults for absent temporal parameters; full compatibility fixtures remain in Gate E |
| Realtime | No callback I/O/locks/allocations/final reclamation; bounded fixed state | [[REALTIME_AUDIT]] code review; allocator/profiler and DAW stress remain external |

Update exact SHA, run ID, artifact digest/module size, and host boundaries only when evidence exists.
