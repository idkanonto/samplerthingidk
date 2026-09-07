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

This audit covers verified Gate D `main` and Gate E PR #15 against [[DSP_NOTES]]. Gate D passed PR run #48 and post-merge run #49. Gate E head `027c57b8c0c30bf5450042de64f006c1c9e92fb0` passed [Windows Release CI run #51](https://github.com/idkanonto/samplerthingidk/actions/runs/34164215738), including the expanded lifecycle and exact full-chain stress suite. Source/CI review does not replace an allocator hook, realtime profiler, or DAW stress pass.

## Audio-thread paths

| Path | Bounded behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Reads timing once, enumerates at most 64 grid boundaries, walks host MIDI once, renders 16 voices, then runs the fixed global chain and Output | No file access, explicit lock, logging, stretch work, parsing, or explicit allocation. Parameter/status access is atomic. |
| Note On | Scans at most 20 immutable sources twice, resolves one region/start/pitch value, scans/acquires at most 16 voices | The atomically published pool and prepared version are shared immutable references; manager retirement roots prevent final callback reclamation. |
| Voice render | Linear interpolation and scalar envelope/length/fade/steal state bounded by the supplied span | No RNG, collection, lock, or mutable source access. The removed per-event FX state is absent. |
| Host grid | Constant scalar math plus a fixed 64-entry output array | No playhead access outside the single block-start read; no heap or UI access. |
| FREEZE | One bounded sample loop, wrapped interpolation, and scalar fades. Activation stores only ring indices/scalars. | Two-second stereo history is allocated in `prepare`; captured audio is referenced in place and never copied or reclaimed in the callback. |
| SCRAMBLE | One bounded sample loop and at most eight fixed chunk decisions per activation. Every logical read is clamped. | Preallocated stereo history plus fixed mapping/reverse arrays; no vector, lock, parser, or mutable UI state. |
| FRACTURE | Fixed waveshaper/filter math per sample; six parallel stable structures and fractional comb reads | The 60 ms stereo comb is allocated in `prepare`; state is scalar/fixed-array and feedback/output are bounded. |
| SPECTRAL DRAW | One 1024-point transform per channel every 256 samples, fixed ring scans, bounded bin/mask lookup, and fixed-array reset on transport discontinuity | Signalsmith FFT work storage is resized only in `prepare`; four inline immutable mask slots use atomic state transitions. The callback takes no canvas mutex, copy, allocation, or final reclamation. |
| SMEAR | Two bounded fractional reads per channel and scalar window/blur math per sample | The half-second stereo delay is allocated in `prepare`; read heads wrap within capacity and no grains are objects. |
| CODEC | Fixed predictor/reconstruction math plus two bounded counters and sample-and-hold | Per-channel state is fixed arrays. No real encoder, quantizer/Bit Crush, RNG, lock, parsing, or allocation. |

## Non-realtime paths

- File validation, decoding, waveform preparation, source-state restore, pool publication, and retirement collection remain control/state work.
- Signalsmith work runs on the dedicated worker. Source identity and revision reject stale publication.
- Source edits use `mutationMutex`; the stretch queue uses its own mutex and condition variable. Neither is reached by the callback.
- Gate E finite-clamps hostile persisted/updated source Gain and Weight before publication. Voice envelope/sample sanitization is fixed-cost and protects internal voice state before the global chain.
- Editor selection is a stable source ID. Trigger highlighting is a separate atomic runtime ID and cannot retarget edits.
- Canvas drawing, clamping, encoding, restore, and canonical mutation occur on UI/state paths. Publication writes a free slot completely before its release-store; the callback reads only a held immutable slot.
- State migration allocates/mutates only during host state restore, never during audio rendering.

## External verification boundary

Gate E source review found no callback file access, explicit locks, waits, logging, parsing, stretch preparation, canvas copying, or callback-owned final reclamation. Run #51 exercised the exact global order under variable blocks, discontinuity, hostile state/audio, stale worker completion, source removal during preparation, and retained old prepared versions. No allocator hook, realtime profiler, or DAW host stress pass is available in CI; those remain explicit external release checks.
