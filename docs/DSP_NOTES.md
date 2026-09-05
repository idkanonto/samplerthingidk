---
title: DSP Notes
tags:
  - dsp
  - realtime
status: active
---

# DSP Notes

## Realtime contract

- The audio callback must not perform file I/O, decoding, blocking locks, logging, avoidable allocation/deallocation, expensive analysis, or stretch preparation.
- Publish immutable sample-pool snapshots to the audio thread. Ensure removal and replacement cannot make the realtime thread perform final reclamation.
- Prepare pitch-preserving stretch outside the realtime thread and publish/cache completed results safely.
- Preserve sample-accurate handling of MIDI offsets within host blocks.

## Current engine

- Sources are decoded fully into RAM on control/state paths.
- Non-unity Stretch is prepared by one background worker with Signalsmith Stretch. Immutable prepared versions are selected by source runtime identity and revision; stale results are discarded, queued work is coalesced/cancelled, and superseded buffers are reclaimed off the audio thread after voices release them.
- Playback uses linear interpolation for source-rate conversion.
- Each voice has Attack/Release, optional sample-counted Final Length, a 3 ms source-region boundary fade, and a 3 ms tail crossfade on stealing.
- Sixteen voices are preallocated. POLY steals the oldest active voice; MONO reuses the primary voice with that same crossfade and releases residual POLY voices.
- Chance randomness is resolved once during Note On into a fixed-size value. Voice rendering uses only stored values, bounded four-piece Reorder work, a precomputed Bend multiplier, and a smoothed eight-slot Drop gain; it performs no RNG calls or allocation.
- Step Mask bits and reset generation are atomically published from control/state paths. Its fixed cursor belongs to the audio thread, advances once per Note On in MIDI-buffer order, and bypasses Chance resolution entirely on NORMAL steps.
- Take capture and replay use fixed arrays owned by the audio thread. `TakeEvent` stores fixed UUID bytes, an immutable prepared reference, and resolved event values; HISTORY performs no RNG calls. UI mode/selection requests and status use bounded atomics rather than locks or mutable UI access.
- Prepared versions referenced by Takes remain rooted in SampleManager's current or retired storage. Replacing a Take may decrement a reference on the audio thread, but cannot destroy the prepared audio there; the last retained reference is erased only by control-thread garbage collection or processor teardown.
- A xorshift64* generator supplies allocation-free seeded random decisions.

## Stretch teardown

Queued stretch work is cancelled during source removal, pool replacement, and teardown. An already-running pinned Signalsmith `exact()` call is joined off the realtime thread because the upstream one-shot API has no safe mid-call cancellation hook; detaching it would permit code to run after plugin unload.

Processing order is defined in [[SIGNAL_CHAIN]]. Verification belongs in [[TEST_MATRIX]].
