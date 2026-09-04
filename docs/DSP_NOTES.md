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
- Each voice has Attack/Release, a 3 ms source-region boundary fade, and a 3 ms tail crossfade on stealing.
- Sixteen voices are preallocated; oldest active voice is stolen.
- A xorshift64* generator supplies allocation-free seeded random decisions.

## Stretch teardown

Queued stretch work is cancelled during source removal, pool replacement, and teardown. An already-running pinned Signalsmith `exact()` call is joined off the realtime thread because the upstream one-shot API has no safe mid-call cancellation hook; detaching it would permit code to run after plugin unload.

Processing order is defined in [[SIGNAL_CHAIN]]. Verification belongs in [[TEST_MATRIX]].
