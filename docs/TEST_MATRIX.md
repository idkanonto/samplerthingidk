---
title: Test Matrix
tags:
  - testing
  - verification
status: active
---

# Test Matrix

Update evidence when tests actually run. “Code-inspected” is not a runtime pass.

| Area | Minimum verification | Current evidence |
|---|---|---|
| Build | Clean Release VST3 and Standalone builds | CI workflow exists; not rerun for this setup |
| Formats | Mono/stereo WAV, AIFF/AIF, MP3, FLAC at 44.1/48/96 kHz | Extension/decoder path code-inspected; runtime not run |
| Pool | 20 accepted, 21st rejected; remove/clear; missing-file restore | Code-inspected; runtime not run |
| Selection | Disabled never selected; Weight distribution is plausible; seeded repeatability | Code-inspected; behavior not run |
| Random region | Starts always within manual Start/End | Not implemented |
| Pitch | Source/Target keys, Transpose, Fine Tune, MIDI pitch/root behavior | Not implemented |
| Stretch | Pitch preservation, cache reuse/invalidation, off-audio-thread preparation | Not implemented |
| Voices | 16 voices, oldest stealing, Mono/Poly, Note Off, rapid notes/chords | Base polyphony code-inspected; modes not implemented |
| Chance/Mask | All six effects; 2/4/8/16; Note-On-only advance; NORMAL bypass | Not implemented |
| Takes | Full-cycle boundary, latest-8 eviction, deterministic HISTORY replay | Not implemented |
| Length/envelope | Chance → Final Length → Attack/Release ordering | Final Length not implemented |
| Global FX | Bit Crush → Sample Rate Reduction → Output Gain | Only Output Gain implemented |
| State | Automation and project save/reopen, moved/missing sources | Code-inspected; host runtime not run |
| Realtime | No I/O, blocking, allocation/reclamation, or stretch prep in callback | Architecture code-inspected; profiler/stress test not run |

When a feature passes, update [[CURRENT_STATE]] with the exact build, host, sample set, or automated test used.

