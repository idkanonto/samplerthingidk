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

This audit covers creative-logic code head `d129a402b5db309c41428b4a0d1a39003f84fd5b` on [PR #18](https://github.com/idkanonto/samplerthingidk/pull/18), which passed [Windows Release CI run #64](https://github.com/idkanonto/samplerthingidk/actions/runs/34676232538), against [[DSP_NOTES]], and retains the earlier Gate E ownership/lifecycle findings. Source and CI review do not replace an allocator hook, realtime profiler, or DAW stress pass.

## Audio-thread paths

| Path | Bounded behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Reads timing once, enumerates at most 4096 grid boundaries, walks host MIDI once, renders 16 voices, then runs the fixed global chain and smoothed Output | No file access, explicit lock, logging, stretch work, parsing, or explicit allocation. Parameter and bounded visual-telemetry access is atomic. |
| Note On | Scans at most 20 immutable sources twice, resolves one region/start/pitch value, scans/acquires at most 16 voices | The atomically published pool and prepared version are shared immutable references; manager retirement roots prevent final callback reclamation. |
| Voice render | Linear interpolation and scalar envelope/length/fade/steal state bounded by the supplied span | No RNG, collection, lock, or mutable source access. The removed per-event FX state is absent. |
| Host grid | Constant scalar math plus a fixed 4096-entry output array with an explicit truncation flag | No playhead access outside the single block-start read; no heap or UI access. |
| SCRAMBLE | One bounded sample loop and at most eight fixed slice decisions per activation. Fractional reads wrap inside the selected loop; seam blending adds only bounded duplicate reads. | Two-second stereo history plus fixed origin/offset/increment/loop/wet arrays are allocated in `prepare`; activation copies no audio and takes no lock. |
| FRACTURE | Two-channel 2x oversampling, adjacent waveshape interpolation, one stable morph filter per channel, envelope/modulation, DC blocking, fixed dry delay, and bounded chunking | Oversampling filters, two prepared block buffers, and dry-delay storage are initialized in `prepare`; callback chunks never exceed prepared capacity and perform no resize. State is scalar or fixed-array. |
| SPECTRAL DRAW | One 1024-point transform per channel every 256 samples, fixed ring scans, bounded bin/mask lookup, frame-gain smoothing, and fixed-array reset on transport discontinuity | Signalsmith FFT work storage and bin-gain arrays are prepared/fixed; four inline immutable mask slots use atomic state transitions. The callback takes no canvas mutex, copy, allocation, or final reclamation. |
| SMEAR | At most six grain iterations and bounded interpolated reads per sample, plus fixed raw/filtered histories and scalar envelope/window/pan/normalization math | One-second three-band stereo history and six fixed grain records are allocated in `prepare`. Seed changes invalidate logical history without clearing large rings in the callback. |

## Non-realtime paths

- File validation, decoding, waveform preparation, source-state restore, pool publication, and retirement collection remain control/state work.
- Signalsmith work runs on the dedicated worker. Source identity and revision reject stale publication.
- Source edits use `mutationMutex`; the stretch queue uses its own mutex and condition variable. Neither is reached by the callback.
- Gate E finite-clamps hostile persisted/updated source Gain and Weight before publication. Voice envelope/sample sanitization is fixed-cost and protects internal voice state before the global chain.
- Editor selection is a stable source ID. Trigger highlighting is a separate atomic runtime ID and cannot retarget edits.
- Canvas drawing, clamping, encoding, restore, and canonical mutation occur on UI/state paths. Publication writes a free slot completely before its release-store; the callback reads only a held immutable slot.
- State migration allocates/mutates only during host state restore, never during audio rendering. A restored internal seed reaches the callback atomically; normal preparation initializes all salted RNG streams before the first block.
- Creative visualizers read relaxed scalar atomics published once per block. They never read DSP-owned arrays, history, grain records, or audio buffers.

## External verification boundary

Source review finds no callback file access, explicit locks, waits, logging, parsing, stretch preparation, canvas copying, or callback-owned final reclamation. The full-chain test exercises variable blocks, discontinuity, hostile state/audio, stale worker completion, source removal during preparation, and retained old prepared versions. No allocator hook, realtime profiler, or DAW host stress pass is available in CI; those remain explicit external release checks.
