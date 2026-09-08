---
title: Realtime Safety Audit
aliases:
  - Realtime Audit
tags:
  - architecture
  - realtime
  - verification
status: active
---

# Realtime Safety Audit

This audit covers creative-quality code head `d301d34e20e09652d63ed4d85be3faae771ad210`, which passed [Windows Release CI run #60](https://github.com/idkanonto/samplerthingidk/actions/runs/34195038905), against [[DSP_NOTES]] and retains the earlier Gate E ownership/lifecycle findings. Source and CI review do not replace an allocator hook, realtime profiler, or DAW stress pass; exact current Windows evidence is recorded in [[TEST_MATRIX]].

## Audio-thread paths

| Path | Bounded behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Reads timing once, enumerates at most 64 grid boundaries, walks host MIDI once, renders 16 voices, then runs the fixed global chain and Output | No file access, explicit lock, logging, stretch work, parsing, or explicit allocation. Parameter/status access is atomic. |
| Note On | Scans at most 20 immutable sources twice, resolves one region/start/pitch value, scans/acquires at most 16 voices | The atomically published pool and prepared version are shared immutable references; manager retirement roots prevent final callback reclamation. |
| Voice render | Linear interpolation and scalar envelope/length/fade/steal state bounded by the supplied span | No RNG, collection, lock, or mutable source access. The removed per-event FX state is absent. |
| Host grid | Constant scalar math plus a fixed 64-entry output array | No playhead access outside the single block-start read; no heap or UI access. |
| SCRAMBLE | One bounded sample loop and at most eight fixed slice decisions per activation. Every fractional logical read wraps inside the captured region. | Two-second stereo history plus fixed origin/offset/increment/loop/wet arrays are allocated in `prepare`; activation copies no audio and takes no lock. |
| FRACTURE | Fixed predictor, sample-hold, waveshaper, filter/formant, envelope/modulation, and fractional-comb math per sample | The 60 ms stereo comb is allocated in `prepare`; oscillator/random/filter state is scalar or fixed-array, and feedback/resonance/DC/output are bounded. |
| SPECTRAL DRAW | One 1024-point transform per channel every 256 samples, fixed ring scans, bounded bin/mask lookup, and fixed-array reset on transport discontinuity | Signalsmith FFT work storage is resized only in `prepare`; four inline immutable mask slots use atomic state transitions. The callback takes no canvas mutex, copy, allocation, or final reclamation. |
| SMEAR | At most six grain iterations and bounded interpolated reads per sample, plus scalar envelope/high-pass/window/pan math | One-second stereo history and six fixed grain records are allocated in `prepare`. Seed changes invalidate logical history without clearing the large ring in the callback. |

## Non-realtime paths

- File validation, decoding, waveform preparation, source-state restore, pool publication, and retirement collection remain control/state work.
- Signalsmith work runs on the dedicated worker. Source identity and revision reject stale publication.
- Source edits use `mutationMutex`; the stretch queue uses its own mutex and condition variable. Neither is reached by the callback.
- Gate E finite-clamps hostile persisted/updated source Gain and Weight before publication. Voice envelope/sample sanitization is fixed-cost and protects internal voice state before the global chain.
- Editor selection is a stable source ID. Trigger highlighting is a separate atomic runtime ID and cannot retarget edits.
- Canvas drawing, clamping, encoding, restore, and canonical mutation occur on UI/state paths. Publication writes a free slot completely before its release-store; the callback reads only a held immutable slot.
- State migration allocates/mutates only during host state restore, never during audio rendering. A restored internal seed reaches the callback atomically; normal preparation initializes all salted RNG streams before the first block.

## External verification boundary

Source review finds no callback file access, explicit locks, waits, logging, parsing, stretch preparation, canvas copying, or callback-owned final reclamation. The full-chain test exercises variable blocks, discontinuity, hostile state/audio, stale worker completion, source removal during preparation, and retained old prepared versions. No allocator hook, realtime profiler, or DAW host stress pass is available in CI; those remain explicit external release checks.
