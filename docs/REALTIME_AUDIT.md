---
title: Realtime Safety Audit
aliases:
  - Realtime Audit
tags:
  - architecture
  - realtime
  - verification
status: in-progress
---

# Realtime Safety Audit

This audit covers the Gate B branch against [[DSP_NOTES]]. Gate A passed PR run #42 and post-merge run #43; Gate B runtime verification is pending. Source/CI review does not replace an allocator hook, realtime profiler, or DAW stress pass.

## Audio-thread paths

| Path | Bounded behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Reads timing once, enumerates at most 64 grid boundaries, walks host MIDI once, renders 16 voices, runs fixed-state Rate Reduction, then Output | No file access, explicit lock, logging, stretch work, or explicit allocation. Parameter/status access is atomic. |
| Note On | Scans at most 20 immutable sources twice, resolves one region/start/pitch value, scans/acquires at most 16 voices | The atomically published pool and prepared version are shared immutable references; manager retirement roots prevent final callback reclamation. |
| Voice render | Linear interpolation and scalar envelope/length/fade/steal state bounded by the supplied span | No RNG, collection, lock, or mutable source access. The removed per-event FX state is absent. |
| Host grid | Constant scalar math plus a fixed 64-entry output array | No playhead access outside the single block-start read; no heap or UI access. |
| FREEZE | One bounded sample loop, wrapped interpolation, and scalar fades. Activation stores only ring indices/scalars. | Two-second stereo history is allocated in `prepare`; captured audio is referenced in place and never copied or reclaimed in the callback. |
| SCRAMBLE | One bounded sample loop and at most eight fixed chunk decisions per activation. Every logical read is clamped. | Preallocated stereo history plus fixed mapping/reverse arrays; no vector, lock, parser, or mutable UI state. |
| Rate Reduction | Two held floats and one bounded phase counter | No quantizer/Bit Crush, RNG, lock, or allocation; non-finite/pathological values are contained. |

## Non-realtime paths

- File validation, decoding, waveform preparation, source-state restore, pool publication, and retirement collection remain control/state work.
- Signalsmith work runs on the dedicated worker. Source identity and revision reject stale publication.
- Source edits use `mutationMutex`; the stretch queue uses its own mutex and condition variable. Neither is reached by the callback.
- Editor selection is a stable source ID. Trigger highlighting is a separate atomic runtime ID and cannot retarget edits.
- State migration allocates/mutates only during host state restore, never during audio rendering.

## Required later audit

Each global effect must document buffer capacity, boundary/discontinuity behavior, bypass identity, denormal/non-finite containment, parameter publication, and teardown ownership. Gate D must additionally document STFT latency and overlap-add storage. Gate E repeats the full-chain audit and records external checks still not run.
