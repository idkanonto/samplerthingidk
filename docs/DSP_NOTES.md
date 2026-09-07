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

## Temporal global effects

- FREEZE and SCRAMBLE each allocate a two-second stereo ring only in `prepare`. During rendering they write the incoming global mix while idle and freeze the ring while an event references its captured logical range.
- Activation records ring indices and fixed scalar/array decisions; it does not copy captured audio. When an event ends, logical validity resets and the old memory is overwritten incrementally.
- FREEZE derives capture and hold frames from the current grid BPM. Its optional octave direction is fixed for the event and reads the captured ring with wrapped linear interpolation.
- SCRAMBLE uses no more than eight chunks. Its source index and reverse flag arrays are resolved once at activation; render-time addresses are clamped to the captured logical range.
- Separate salted RNG streams preserve sampler random-selection stability. Chance 0 and Amount 0 avoid activation; bypassed finite samples remain identical.
- A discontinuity drops event and logical-history state in constant time. Large buffers are not cleared from the audio callback.

## Creative character effects

- FRACTURE computes stable state-variable filter coefficients once per block and runs parallel structures rather than interpolating incompatible coefficients. A prepared fractional comb supplies hollow/metallic paths; feedback, integrators, DC rejection, and output are explicitly bounded.
- FRACTURE presets are a fixed compile-time bank. UI selection writes the six real automatable parameters on the message thread; the callback never parses, allocates, or accesses preset names.
- SMEAR allocates half a second of stereo delay storage in `prepareToPlay`. Two complementary-window read heads move through bounded short grains, with scalar cross-block phase and blur state. Amount 0 writes history but leaves finite input samples untouched.
- CODEC approximates bandwidth loss and packet damage with fixed per-channel predictor, residual, blur, and hold state. The existing deterministic sample-and-hold reducer is its final internal operation and retains the `rateReduction` parameter ID.
- All three stages clamp hostile settings and active-path non-finite samples. FRACTURE Mix 0, SMEAR Amount 0, and CODEC Amount 0 with 1x Rate are exact finite-sample bypasses.

## Removed DSP

The old Take/Step/per-event effect path and Bit Crush are absent. They must not be reintroduced as implementation shortcuts for the global chain in [[SIGNAL_CHAIN]].
