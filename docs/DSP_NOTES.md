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

- SCRAMBLE allocates one two-second stereo ring in `prepare`. Activation records only ring indices and fixed scalar/array decisions; it never copies captured audio.
- Every eligible boundary at a nonzero Amount receives a bounded event. Amount chooses four through eight slices and a monotonically increasing number of manipulated slices, eliminating chance-driven empty phrases while retaining gesture variation.
- A selected slice can jump, replay a micro-loop, hold a shorter region, reverse, or read at 0.5x/2x for integrated octave-style fragments. Adjacent selected slices can share one gesture to form a motif rather than unrelated switches.
- One- or two-grid event spans are selected with an amount-dependent distribution. A 2.5 ms per-slice taper bounds discontinuities while allowing intentionally abrupt internal edits.
- The dry signal continues to refresh history during an event only when ring capacity proves the captured range cannot be overwritten. Event completion keeps valid recent history; transport discontinuity invalidates it in constant time.
- A separately salted RNG stream preserves sampler-source random-selection stability. Amount 0 is sample-identical and cannot activate.

## Creative character effects

- FRACTURE couples two phase-related slow oscillators, smoothed bounded random targets, and an input envelope. Amount uses nonlinear curves to increase wet level, drive, modulation depth, resonance, filter/formant motion, comb character, packet damage, and the effective rate-reduction severity together.
- Predictive digital damage is inside the FRACTURE topology: bandwidth loss feeds a held residual/reconstruction stage before waveshaping and parallel stable tonal structures. Rate Reduction remains a user choice but its effective factor is moderated by the FRACTURE macro.
- Six parallel low/band/notch/formant/hollow-comb/metallic-comb structures are blended at their outputs; incompatible filter coefficients are never interpolated. Comb feedback, filter state, a sample-rate-invariant 15 Hz DC blocker, and final output are bounded.
- FRACTURE presets are a fixed compile-time bank. UI selection writes Amount, Character, and Rate on the message thread; callback code never parses or accesses preset names.
- SMEAR allocates one second of stereo history and six fixed grain records in `prepareToPlay`. Grains start only when a safe read-behind distance exists, use Hann tapers, musically restricted pitch intervals, bounded scatter, stereo panning/crossfeed, and high-passed residual emphasis.
- Fast/slow input envelopes reduce SMEAR wet level around transients so the texture complements attacks instead of turning the source into blur. Amount controls density, grain duration, interval range, scatter, and wet intensity. Amount 0 remains sample-identical while filling history.
- Both creative engines use separately salted deterministic RNG state and fixed/preallocated storage. Hostile settings and samples are finite-clamped; maximum feedback/resonance is bounded.

## Public implementation research

- Signalsmith Stretch (MIT) remains the only externally integrated DSP dependency. Its license and notice are already packaged; this pass did not copy additional Signalsmith code.
- Mutable Instruments Clouds (MIT) was studied for preallocated capture/history organization and musically coupled texture controls. No Clouds source was copied or added as a dependency.
- DaisySP (MIT) granular-player and decimator implementations were studied for phase-offset windowing, bounded grain playback, and deterministic rate-hold structure. The shipped implementations are original and no DaisySP source was copied.
- Signalsmith DSP (MIT) was studied as a compact realtime-DSP reference; it was not needed as a dependency.
- chowdsp_utils (mixed licensing, with relevant DSP modules under GPLv3), Rubber Band (GPL-2.0-or-later or commercial), and Surge XT (GPLv3) were architecture references only. Their reciprocal/commercial terms were not introduced into this project and no code was copied.

## Spectral Draw

- SPECTRAL DRAW uses a 1024-point complex Signalsmith FFT with a 256-sample hop and square-root Hann analysis/synthesis windows. Four-way overlap-add is normalized in place; all FFT, ring, window, and work storage is fixed or prepared before playback.
- The stage always feeds its STFT history. Depth 0 selects an exact 1024-sample delayed dry path, matching the latency reported to the host, so automation can enter the spectral path without unprimed storage.
- The 128×64 canvas is attenuation-only. Scanner/time and square-root-mapped frequency coordinates use bilinear interpolation; a full mask at maximum Depth reaches zero gain without positive or unbounded spectral gain.
- A fixed four-slot publication store keeps canonical canvas mutation/encoding behind a message/state-thread mutex. The callback atomically acquires one immutable published slot for the block and never takes that mutex or copies the canvas.
- Host PPQ aligns scanner phase for 2/4/8/16-quarter-note cycles. Finite fallback BPM advances phase when host PPQ is unavailable. Discontinuity clears only fixed processor arrays and scalar indices; no storage is allocated or reclaimed.

## Removed DSP

The old standalone FREEZE, standalone CODEC, blur-based SMEAR, Take/Step/per-event effect path, and Bit Crush are absent. Useful Freeze gestures and Codec damage live only inside the coherent SCRAMBLE/FRACTURE macros described above. They must not be reintroduced as duplicate headline stages or implementation shortcuts for [[SIGNAL_CHAIN]].
