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
- Playback uses linear interpolation for source-rate conversion.
- Each voice has Attack/Release, a 3 ms physical-end fade, and a 3 ms tail crossfade on stealing.
- Sixteen voices are preallocated; oldest active voice is stolen.
- A xorshift64* generator supplies allocation-free seeded random decisions.

## V2 stretch direction

Signalsmith Stretch is preferred because an MIT-licensed implementation was selected. It is not currently integrated. Before integration, confirm the pinned upstream version/license, cache key and invalidation rules, memory bounds for 20 sources, failure fallback, and that cache publication never blocks the audio thread.

Processing order is defined in [[SIGNAL_CHAIN]]. Verification belongs in [[TEST_MATRIX]].

