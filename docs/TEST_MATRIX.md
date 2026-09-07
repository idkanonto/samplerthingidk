---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Gate A passed [PR run #42](https://github.com/idkanonto/samplerthingidk/actions/runs/34082214626) and [post-merge run #43](https://github.com/idkanonto/samplerthingidk/actions/runs/34082765219). Gate B passed [PR run #44](https://github.com/idkanonto/samplerthingidk/actions/runs/34084189109) and [post-merge run #45](https://github.com/idkanonto/samplerthingidk/actions/runs/34084650160). Gate C passed [PR run #46](https://github.com/idkanonto/samplerthingidk/actions/runs/34140666576) and [post-merge run #47](https://github.com/idkanonto/samplerthingidk/actions/runs/34144904944). Gate D passed [PR run #48](https://github.com/idkanonto/samplerthingidk/actions/runs/34147146261) at `af45dd9` and [post-merge run #49](https://github.com/idkanonto/samplerthingidk/actions/runs/34151063471) at `37e1963`. Gate E passed [PR run #51](https://github.com/idkanonto/samplerthingidk/actions/runs/34164215738) at `027c57b`; CTest passed 1/1 in 0.13 seconds. Artifact `10033734310` had digest `73cfed01c3e2cd781bd7ada26a185797fedad33751a994838ee3c3ca22ff9b3f` and a workflow-verified 7,465,472-byte Windows module.

| Area | Gate expectation | Current evidence |
|---|---|---|
| Packaging | Release VST3 and Standalone use the visible `recompiler.dll` identity; complete bundle contains a non-empty Windows module and notices | Runs #42–#51 passed except the superseded Gate E compile-failure run #50; run #51 verified the 7,465,472-byte module and required notices |
| Formats/pool | Decode WAV, AIFF/AIF, MP3, FLAC; enforce 20-source limit; remove/clear/enable; missing restore | Runs #42/#43 decoded the writable format matrix plus a real MP3 and passed pool/state checks |
| Selection/region/pitch | Weighted playable-only selection; legal Start/End; deterministic Seed; full pitch math | Runs #42/#43 passed Weight distribution, disabled/missing exclusion, region bounds, and tonic/manual/MIDI pitch math |
| Stretch | OFF and 1x reuse original; 2x–4x duration; background immutable publication; stale/removal/lifetime safety | Runs #42/#43 passed bounds, OFF/1x identity, 4x duration, and publication; run #51 passed stale-revision rejection, removal during worker activity, and old prepared-version lifetime stress |
| Voices | 16 POLY voices, oldest stealing, MONO replacement, Note Off, Final Length, envelopes | Runs #42/#43 passed region safety, Final Length/envelopes, 16-voice capacity, and oldest selection; DAW MIDI remains external |
| Removed systems | No Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, or Bit Crush in runtime/UI/API | Gate A source audit and legacy-state migration test passed runs #42/#43 |
| Host grid | 1/8, 1/16 default, 1/32; BPM/PPQ; arbitrary blocks; tempo changes; seek/loop; stopped/missing fallback | Gate A grid coverage passed runs #42/#43; Gate B adds tempo changes, PPQ offsets, stopped transitions, and all spacing modes |
| FREEZE/SCRAMBLE | Boundary activation, determinism, bypass, click safety, bounds | Runs #44/#45 passed Chance 0/100, Amount 0/low/high, exact and both-sign octave branches, fixed-seed equality, bounded capture, pre-boundary identity, repeated activation, finite output, and transport invalidation |
| FRACTURE/SMEAR/CODEC | Presets, parameter extremes, bypass, finite bounds, rate-reducer integration | Runs #46/#47 passed all 30 preset value/output checks, extreme Drive/Resonance and hostile samples, exact bypasses, Smear cross-block history, Codec fixed-state determinism/silence, and the preserved 1x–64x reducer |
| SPECTRAL DRAW | STFT reconstruction/bypass, mask publication/persistence, scan mapping, latency, bounds | Runs #48/#49 passed immutable publication, persistence/invalid state, exact 1024-sample bypass, empty/full masks, OLA error, 44.1/48/96 kHz preparation, arbitrary blocks, scanner wrap/PPQ alignment, canvas changes, discontinuity, and finite bounds |
| State | Surviving parameters/sources restore; removed entries ignored; new FX/canvas persist | Runs #42/#43 passed migration; later gates add neutral defaults and canvas persistence; run #51 passed hostile live/restored Gain/Weight clamping and the complete compatibility suite |
| Full integration | Exact global order, variable blocks, discontinuity, hostile state/audio, bounded finite output | Run #51 passed 1/17/63/128/255/511/7/89-sample blocks through `FREEZE -> SCRAMBLE -> FRACTURE -> SPECTRAL DRAW -> SMEAR -> CODEC -> OUTPUT`, including NaN/Inf injection and transport discontinuity |
| Realtime | No callback I/O/locks/allocations/final reclamation; bounded fixed state | Run #51 passed the expanded lifecycle/full-chain suite and [[REALTIME_AUDIT]] source review; allocator/profiler and DAW stress remain external |

Update exact SHA, run ID, artifact digest/module size, and host boundaries only when evidence exists.
