---
title: DSP Notes
tags:
  - dsp
  - realtime
status: active
---

# DSP Notes

## Realtime contract

- No file I/O, decoding, blocking lock, logging, background wait, avoidable allocation/deallocation, analysis, or stretch preparation in `processBlock`.
- Publish source pools and prepared source versions immutably. Keep non-realtime retirement roots so audio-thread reference release cannot reclaim a large buffer.
- Read playhead timing once per block. Creative stages consume the resulting fixed-capacity boundary list; they do not query the host or UI.
- Preallocate global-effect buffers in `prepareToPlay`. Bound every scan by the block size, source limit, voice count, grid capacity, or explicit DSP buffer capacity.
- Make bypass values transparent and keep fresh-instance creative processing neutral.

## Sampler and stretch

- Sources decode fully into RAM on control/state paths. Waveform peaks are immutable.
- `0` and `1x` Stretch both reuse decoded PCM. Ratios above one through `4x` run through pinned Signalsmith Stretch on one worker.
- Jobs carry source runtime identity and monotonically increasing revision. Queued work coalesces; stale/removed results do not publish.
- Voices retain immutable prepared data and use linear interpolation, per-source Gain, Final Length, Attack/Release, a 3 ms region fade, and a 3 ms steal tail.

## Host grid

- Grid units are 0.5, 0.25, or 0.125 quarter notes for 1/8, 1/16, or 1/32.
- A playing host with finite positive BPM and finite PPQ is authoritative. The enumerator treats each block as a half-open interval so a boundary is emitted once.
- Expected PPQ continuity is computed from the previous block's rate and BPM. A seek, loop, incompatible transport jump, grid edit, or clock-source transition marks a discontinuity.
- Missing or stopped host transport uses a continuous sample countdown at the latest valid BPM, initially 120. Grid output is a fixed array and cannot allocate.

## Removed DSP

The old Take/Step/per-event effect path and Bit Crush are absent. They must not be reintroduced as implementation shortcuts for the global chain in [[SIGNAL_CHAIN]].
