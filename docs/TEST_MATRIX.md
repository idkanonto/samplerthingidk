---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Gate A passed [PR run #42](https://github.com/idkanonto/samplerthingidk/actions/runs/34082214626) and [post-merge run #43](https://github.com/idkanonto/samplerthingidk/actions/runs/34082765219). Gate B passed [PR run #44](https://github.com/idkanonto/samplerthingidk/actions/runs/34084189109) at `9fd7325` and [post-merge run #45](https://github.com/idkanonto/samplerthingidk/actions/runs/34084650160) at `4f2f31f`. The Gate B artifact (`10004759928`) had digest `495ddd0d7e5f6591891afcdfaa1bd553fa65a38c99e8f3bd101948418bfa0462` and a workflow-verified 7,400,448-byte Windows module. Gate C rows describe branch test intent until its CI passes.

| Area | Gate expectation | Current evidence |
|---|---|---|
| Packaging | Release VST3 and Standalone use the visible `recompiler.dll` identity; complete bundle contains a non-empty Windows module and notices | Runs #42–#45 passed; Gate B workflow verified the 7,400,448-byte module and required notices |
| Formats/pool | Decode WAV, AIFF/AIF, MP3, FLAC; enforce 20-source limit; remove/clear/enable; missing restore | Runs #42/#43 decoded the writable format matrix plus a real MP3 and passed pool/state checks |
| Selection/region/pitch | Weighted playable-only selection; legal Start/End; deterministic Seed; full pitch math | Runs #42/#43 passed Weight distribution, disabled/missing exclusion, region bounds, and tonic/manual/MIDI pitch math |
| Stretch | OFF and 1x reuse original; 2x–4x duration; background immutable publication; stale/removal/lifetime safety | Runs #42/#43 passed bounds, OFF/1x identity, 4x duration, and publication; stress/lifetime coverage is required again by Gate E |
| Voices | 16 POLY voices, oldest stealing, MONO replacement, Note Off, Final Length, envelopes | Runs #42/#43 passed region safety, Final Length/envelopes, 16-voice capacity, and oldest selection; DAW MIDI remains external |
| Removed systems | No Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, or Bit Crush in runtime/UI/API | Gate A source audit and legacy-state migration test passed runs #42/#43 |
| Host grid | 1/8, 1/16 default, 1/32; BPM/PPQ; arbitrary blocks; tempo changes; seek/loop; stopped/missing fallback | Gate A grid coverage passed runs #42/#43; Gate B adds tempo changes, PPQ offsets, stopped transitions, and all spacing modes |
| FREEZE/SCRAMBLE | Boundary activation, determinism, bypass, click safety, bounds | Runs #44/#45 passed Chance 0/100, Amount 0/low/high, exact and both-sign octave branches, fixed-seed equality, bounded capture, pre-boundary identity, repeated activation, finite output, and transport invalidation |
| FRACTURE/SMEAR/CODEC | Presets, parameter extremes, bypass, finite bounds, rate-reducer integration | Gate C suite covers all 30 preset value/output checks, extreme Drive/Resonance and hostile samples, exact bypasses, Smear cross-block history, Codec fixed-state determinism/silence, and the preserved 1x–64x reducer; CI pending |
| SPECTRAL DRAW | STFT reconstruction/bypass, mask publication/persistence, scan mapping, latency, bounds | Gate D pending |
| State | Surviving parameters/sources restore; removed entries ignored; new FX/canvas persist | Gate A migration passed runs #42/#43; Gate B adds neutral defaults for absent temporal parameters; full compatibility fixtures remain in Gate E |
| Realtime | No callback I/O/locks/allocations/final reclamation; bounded fixed state | [[REALTIME_AUDIT]] code review; allocator/profiler and DAW stress remain external |

Update exact SHA, run ID, artifact digest/module size, and host boundaries only when evidence exists.
