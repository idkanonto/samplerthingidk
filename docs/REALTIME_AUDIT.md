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

This audit covers straightforward-sampler code head `3dfbc5d55c717275d44ffb6798388d28bb3abbed` on [PR #22](https://github.com/idkanonto/samplerthingidk/pull/22), which passed [Windows Release CI run #79](https://github.com/idkanonto/samplerthingidk/actions/runs/34983220916), against [[DSP_NOTES]], and retains the earlier creative-effect ownership/lifecycle findings. Source and CI review do not replace an allocator hook, realtime profiler, or DAW stress pass.

## Audio-thread paths

| Path | Bounded behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Reads timing once, enumerates at most 4096 grid boundaries, walks host MIDI once, renders 16 voices, then runs the fixed global chain and smoothed Output | No file access, explicit lock, logging, stretch work, parsing, or explicit allocation. Parameter and bounded visual-telemetry access is atomic. |
| Note On | Scans at most 20 immutable sources twice, resolves one region/start/pitch value, scans/acquires at most 16 voices | The atomically published pool and prepared version are shared immutable references; manager retirement roots prevent final callback reclamation. |
| Voice render | Linear interpolation and scalar envelope/length/fade/steal state bounded by the supplied span | No RNG, collection, lock, or mutable source access. The removed per-event FX state is absent. |
| Host grid | Constant scalar math plus a fixed 4096-entry output array with an explicit truncation flag | No playhead access outside the single block-start read; no heap or UI access. |
| SCRAMBLE | One bounded sample loop and at most eight fixed slice decisions per activation. Fractional reads wrap inside the selected loop; seam blending adds only bounded duplicate reads. | Two-second stereo history plus fixed origin/offset/increment/loop/wet arrays are allocated in `prepare`; activation copies no audio and takes no lock. |
| MELT | Per sample, two channels each render at most five overlapping grains with fixed lookup-window, interpolation, and scalar normalization work. Activation makes at most four slice decisions, 96 energy-probe positions, and 384 interpolated channel reads, independent of tempo or grid length. | Four-second stereo history, four slice records, and the 4096-entry Hann table are allocated/prepared before playback. Captures are ring indices, not audio copies; no callback resize, clearing of the large ring, lock, or variable-length activation scan occurs. |
| SPECTRAL DRAW | One 1024-point transform per channel every 256 samples, fixed ring scans, bounded bin/mask lookup, frame-gain smoothing, and fixed-array reset on transport discontinuity | Signalsmith FFT work storage and bin-gain arrays are prepared/fixed; four inline immutable mask slots use atomic state transitions. The callback takes no canvas mutex, copy, allocation, or final reclamation. |
| SMEAR | At most sixteen grain iterations and bounded interpolated reads per sample, plus fixed raw/filtered histories, table lookups, and scalar envelope/pan/normalization math. Amount changes only bounded spawn targets and precomputed grain-length mappings; existing grains finish in place. | One-second three-band stereo history, sixteen fixed grain records, and 2048-point sine/Hann tables are allocated/prepared before playback. Seed changes invalidate logical history without clearing large rings in the callback. |

## Non-realtime paths

- File validation, decoding, waveform preparation, source-state restore, pool publication, and retirement collection remain control/state work.
- There is no background manual-stretch worker or queue. Decoded PCM is wrapped once in an immutable prepared handle on the control/state path.
- Source edits use `mutationMutex`, which is never reached by the callback.
- Persisted/updated source Gain is finite-clamped before publication. Voice envelope/sample sanitization is fixed-cost and protects internal voice state before the global chain.
- Editor selection is a stable source ID. Trigger highlighting is a separate atomic runtime ID and cannot retarget edits.
- Canvas drawing, clamping, encoding, restore, and canonical mutation occur on UI/state paths. Publication writes a free slot completely before its release-store; the callback reads only a held immutable slot.
- State migration allocates/mutates only during host state restore, never during audio rendering. A restored internal seed reaches the callback atomically; normal preparation initializes all salted RNG streams before the first block.
- Creative visualizers read relaxed scalar atomics published once per block. They never read DSP-owned arrays, history, grain records, or audio buffers.

## External verification boundary

Source review finds no callback file access, explicit locks, waits, logging, parsing, stretch preparation, canvas copying, or callback-owned final reclamation. The full-chain test exercises variable blocks, discontinuity, hostile state/audio, source removal, and retained immutable prepared handles. No allocator hook, realtime profiler, or DAW host stress pass is available in CI; those remain explicit external release checks.
