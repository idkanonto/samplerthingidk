---
title: Product Decisions
tags:
  - product
  - decisions
status: active
---

# Product Decisions

These decisions govern implementation together with [[PRODUCT_SPEC_V2]].

## Accepted

- Preserve the sampler core, stable parameter IDs that still exist, source identity, immutable prepared data, and deferred non-realtime reclamation.
- Brand the visible product `recompiler.dll` while retaining standards-compliant VST3 packaging and existing manufacturer/plugin codes plus bundle ID for host continuity.
- Use a shared 1/8, 1/16-default, or 1/32 host grid for every creative global effect, with a deterministic internal fallback.
- Process global creative effects only after voice mixing and only in the fixed [[SIGNAL_CHAIN]] order.
- Treat CodeRabbit as a second opinion. Compilation, approved behavior, test validity, ownership/lifetime, realtime safety, and packaging findings are actionable; redesign and scope expansion are not authoritative.
- Keep Signalsmith stretch preparation on one background worker. `0` and `1x` both mean original duration; allow extension only through `4x`.
- Place the existing sample-and-hold Rate Reduction inside CODEC when that stage is implemented.
- Implement a real STFT/FFT overlap-add Spectral Draw processor as its own high-risk gate.
- Batch tests and project-brain updates with the implementation they describe; avoid documentation-only CI churn.

## Removed

- Take History and its browser/state behavior.
- Programmable Step Mask and its event cursor.
- Per-event Reverse, Retrigger, Skip, Reorder, Bend, and Drop.
- Bit Crush.

Legacy state entries for these systems are ignored rather than reinterpreted.

## Deferred

- Final visual redesign and art direction.
- Any feature not named in [[PRODUCT_SPEC_V2]].
