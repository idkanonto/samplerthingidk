---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Gate A passed [PR run #42](https://github.com/idkanonto/samplerthingidk/actions/runs/34082214626) at `a923539` and [post-merge run #43](https://github.com/idkanonto/samplerthingidk/actions/runs/34082765219) at `2e46632`. The inspected Gate A artifact digest was `0ede6e35ca212e7cbcd3c7d1f8671b8e1ca4f5a8b8a69289628fcb74f1367b90`. Gate B rows describe committed test intent until its CI passes.

| Area | Gate expectation | Current evidence |
|---|---|---|
| Packaging | Release VST3 and Standalone use the visible `recompiler.dll` identity; complete bundle contains a non-empty Windows module and notices | Runs #42/#43 passed; inspected Gate A archive contained the 7,380,992-byte Windows module and required notices |
| Formats/pool | Decode WAV, AIFF/AIF, MP3, FLAC; enforce 20-source limit; remove/clear/enable; missing restore | Runs #42/#43 decoded the writable format matrix plus a real MP3 and passed pool/state checks |
| Selection/region/pitch | Weighted playable-only selection; legal Start/End; deterministic Seed; full pitch math | Runs #42/#43 passed Weight distribution, disabled/missing exclusion, region bounds, and tonic/manual/MIDI pitch math |
| Stretch | OFF and 1x reuse original; 2x–4x duration; background immutable publication; stale/removal/lifetime safety | Runs #42/#43 passed bounds, OFF/1x identity, 4x duration, and publication; stress/lifetime coverage is required again by Gate E |
| Voices | 16 POLY voices, oldest stealing, MONO replacement, Note Off, Final Length, envelopes | Runs #42/#43 passed region safety, Final Length/envelopes, 16-voice capacity, and oldest selection; DAW MIDI remains external |
| Removed systems | No Take History, Step Mask, per-event Reverse/Retrigger/Skip/Reorder/Bend/Drop, or Bit Crush in runtime/UI/API | Gate A source audit and legacy-state migration test passed runs #42/#43 |
| Host grid | 1/8, 1/16 default, 1/32; BPM/PPQ; arbitrary blocks; tempo changes; seek/loop; stopped/missing fallback | Gate A grid coverage passed runs #42/#43; Gate B adds tempo changes, PPQ offsets, stopped transitions, and all spacing modes |
| FREEZE/SCRAMBLE | Boundary activation, determinism, bypass, click safety, bounds | Gate B suite covers Chance 0/100, Amount 0/low/high, exact and both-sign octave branches, fixed-seed equality, bounded capture, pre-boundary identity, repeated activation, finite output, and transport invalidation; CI pending |
| FRACTURE/SMEAR/CODEC | Presets, parameter extremes, bypass, finite bounds, rate-reducer integration | Gate C pending; current Rate Reduction foundation has deterministic/finite tests |
| SPECTRAL DRAW | STFT reconstruction/bypass, mask publication/persistence, scan mapping, latency, bounds | Gate D pending |
| State | Surviving parameters/sources restore; removed entries ignored; new FX/canvas persist | Gate A migration passed runs #42/#43; Gate B adds neutral defaults for absent temporal parameters; full compatibility fixtures remain in Gate E |
| Realtime | No callback I/O/locks/allocations/final reclamation; bounded fixed state | [[REALTIME_AUDIT]] code review; allocator/profiler and DAW stress remain external |

Update exact SHA, run ID, artifact digest/module size, and host boundaries only when evidence exists.
