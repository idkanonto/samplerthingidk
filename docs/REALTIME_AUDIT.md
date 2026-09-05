---
title: Realtime Safety Audit
aliases:
  - Realtime Audit
tags:
  - architecture
  - realtime
  - verification
status: verified
---

# Realtime Safety Audit

This audit covers the approved V2 signal path in [[SIGNAL_CHAIN]]. It distinguishes the audio callback from control, state-restore, editor, and background-worker paths; those non-realtime paths are allowed to allocate, lock, decode, and reclaim data.

## Audio-thread paths

| Path | Bounded realtime behavior | Ownership and synchronization |
|---|---|---|
| `processBlock` | Clears the supplied buffer, walks the host-provided MIDI events once, renders 16 preallocated voices, applies two fixed-state master stages, then applies Output Gain. | No file access, explicit allocation, application mutex, logging, or background-work wait. Parameter reads and UI status writes are atomic. |
| Note On and source selection | Consumes one fixed Step Mask slot, scans no more than 20 immutable source entries twice for weighted selection, computes one bounded region/start/pitch decision, and acquires one of 16 voices. | The pool is acquired as an atomically published immutable shared snapshot. Replaced pools, sources, and prepared buffers are retained by `SampleManager` retirement roots until non-realtime collection. |
| Chance decision | Makes a fixed sequence of scalar RNG decisions. Reorder shuffles four fixed indices and Drop fills eight fixed mask bits. | `EventDecision` is a fixed value with no vector, string, or dynamic payload. NORMAL steps skip this path and consume no Chance RNG calls. |
| Step Mask | Loads fixed mask/reset atomics and advances one audio-thread-owned integer cursor. | Length edits request a reset through an atomic generation; there is no tempo, transport, timer, or UI-object access. |
| Take capture and HISTORY | Copies one fixed `TakeEvent` into fixed arrays, with at most 16 events per Take and eight Takes. HISTORY advances a bounded cursor and reuses the stored decision. | Stable source IDs use 37 fixed bytes. UI requests/status use atomics. Prepared shared references remain rooted by `SampleManager`, so replacement or eviction cannot perform final large-buffer destruction in the callback. |
| Voice start/render/stop | Uses scalar state, fixed Chance data, linear interpolation, bounded four-piece Reorder advancement, and fixed envelope/drop/steal state. Voice acquisition scans exactly 16 entries. | Voices retain an immutable prepared-data reference. Current or retired manager ownership prevents a voice reset from becoming the final owner during realtime processing. |
| Master processing | Bit Crush and stereo sample-and-hold use two held floats and one phase counter; loops are bounded by the supplied buffer. | No RNG, allocation, lock, or external access. Non-finite/pathological values are contained before quantization. |

## Non-realtime paths

- File validation, JUCE decoding, full-buffer allocation, waveform peak generation, and source-state restoration occur through `SampleManager` control/state methods, never from `processBlock`.
- Non-unity Signalsmith Stretch runs on the dedicated worker. Immutable results are published only when source runtime identity and revision still match; stale and removed-source results are discarded.
- Source edits and publication use `mutationMutex`; stretch queue coordination uses `stretchMutex` and a condition variable. None is acquired by `processBlock`, Note On/Off, voice rendering, Step Mask, Chance, Take, or master processing.
- Retired pools, sources, and prepared buffers are erased by control-thread collection from the editor timer or other control operations. Worker shutdown joins only during processor teardown.
- Project serialization, missing-file reconstruction, component allocation, and file chooser work remain host/editor-thread responsibilities.

## Verification boundary

Automated tests exercise deterministic selection and Chance behavior, source removal and prepared-version replacement while voices/Takes retain references, latest-eight Take eviction, cross-block master state, invalid numeric containment, and dense fixed-capacity voice behavior. Phase 10 adds real decode fixtures across the supported writable formats and sample rates, plus a CI-provided MP3 fixture and required Standalone build.

The audit is source- and CI-based. The applicable regression and ownership tests passed [Windows Release CI run #39](https://github.com/idkanonto/samplerthingidk/actions/runs/33940234501) at `7265f77b44bf5db53eb2a59ba6abbbfccd9bcfb4`. It does not replace an instrumented realtime-thread profiler, allocator hook, or DAW stress session; those external checks remain explicit in [[TEST_MATRIX]].

See [[DSP_NOTES]] for the governing contract and [[CURRENT_STATE]] for the exact verified build.
