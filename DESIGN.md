---
name: recompiler.dll — Black Calibration Bench
description: A fixed-scale, monochrome instrument panel built from machined black surfaces, warm ivory markings, and live signal chambers.
colors:
  bench-black: "#090a09"
  panel-black: "#111210"
  raised-black: "#191a17"
  control-black: "#242520"
  warm-ivory: "#e8e4d8"
  calibration-line: "#d7d3c8"
  muted-marking: "#96958d"
  dim-rule: "#5e605a"
  graticule: "#3e413a"
  telemetry-white: "#eeede5"
typography:
  module-title:
    fontFamily: "Pixelify Local, sans-serif"
    fontSize: "17px"
    fontWeight: 700
    lineHeight: 1
    letterSpacing: "0.01em"
  control:
    fontFamily: "Pixelify Local, sans-serif"
    fontSize: "13px"
    fontWeight: 700
    lineHeight: 1
  data:
    fontFamily: "Space Mono Local, monospace"
    fontSize: "10px"
    fontWeight: 700
    lineHeight: 1
  micro-data:
    fontFamily: "Space Mono Local, monospace"
    fontSize: "8px"
    fontWeight: 700
    lineHeight: 1
rounded:
  square: "0px"
  dial: "50%"
spacing:
  hairline: "1px"
  micro: "2px"
  rhythm: "4px"
  inset: "6px"
  control: "8px"
  cluster: "10px"
components:
  pixel-button:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    typography: "{typography.control}"
    rounded: "{rounded.square}"
    padding: "2px 10px"
    height: "28px"
  pixel-button-active:
    backgroundColor: "{colors.warm-ivory}"
    textColor: "{colors.bench-black}"
    typography: "{typography.control}"
    rounded: "{rounded.square}"
    padding: "2px 10px"
    height: "28px"
  module-header:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    typography: "{typography.module-title}"
    rounded: "{rounded.square}"
    padding: "1px 7px 0"
    height: "28px"
  display-chamber:
    backgroundColor: "{colors.bench-black}"
    textColor: "{colors.telemetry-white}"
    rounded: "{rounded.square}"
  numeric-readout:
    backgroundColor: "{colors.bench-black}"
    textColor: "{colors.warm-ivory}"
    typography: "{typography.data}"
    rounded: "{rounded.square}"
    padding: "0 4px"
    height: "23px"
  primary-knob:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    rounded: "{rounded.dial}"
    size: "66px"
  source-gain-knob:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    rounded: "{rounded.dial}"
    size: "38px"
---

# Design System: recompiler.dll — Black Calibration Bench

## Overview

**Creative North Star: "Black Calibration Bench"**

This is a compact operating surface, not a branded landing page: a near-black machined panel whose hierarchy comes from exact partitions, warm ivory calibration marks, and instrument-like readouts. Display chambers are the dominant black masses. Their waveform, particle, spectral, and meter graphics carry the visual energy while the surrounding hardware stays quiet, dense, and mechanically legible.

The interface preserves the approved fixed 960×647 logical geometry and treats every line as functional construction. Type is deliberately split between a pixel-built display face and a tabular data face. The left side of the header is an intentionally empty, unbranded equipment bay; its restraint is part of the identity, not missing content.

**Key Characteristics:**

- Fixed 960×647 logical instrument canvas with whole-UI scale presets only.
- Near-black tiered surfaces with warm ivory labels, rules, and state inversion.
- Dense 4px top-level rhythm and one-pixel construction lines.
- Square controls and containers; circles belong only to rotary controls.
- Black display chambers with etched graticules and crisp, pixel-rendered telemetry.
- Motion is evidence of audio activity, never ambient decoration.

## Colors

The palette is a warm, green-biased monochrome calibrated for long low-light sessions; hierarchy comes from narrow tonal steps rather than hue.

### Primary

- **Warm Ivory:** The single high-contrast command color for labels, borders, active fills, knobs, markers, and focus treatments.
- **Telemetry White:** The slightly brighter signal ink used inside canvases for waveforms, effect traces, scanner lines, and live meter segments.

### Neutral

- **Bench Black:** The outer chassis, deepest display field, inactive control fill, and inverse text color.
- **Panel Black:** The default machined panel plane and canvas clearing color.
- **Raised Black:** A subtle selected-row and source-control tier.
- **Control Black:** The hover plane inside rotary controls.
- **Calibration Line:** The primary one-pixel perimeter and control stroke.
- **Muted Marking:** Secondary scales, empty-state copy, hatch marks, and supporting metadata.
- **Dim Rule:** Internal dividers and subdued display borders.
- **Graticule:** The etched grid color inside waveform and signal chambers.

### Named Rules

**The Ivory Inversion Rule.** Interactive selection and active states invert hard to warm ivory with bench-black text; do not introduce colored accents or soft tinted states.

**The Two Whites Rule.** Warm ivory belongs to the panel hardware; telemetry white belongs to live signal pixels inside display chambers.

## Typography

**Display Font:** Pixelify Local (with sans-serif fallback)

**Body Font:** Space Mono Local (with monospace fallback)
**Label/Mono Font:** Space Mono Local

**Character:** Pixelify Sans gives module names and actions a compact equipment-label voice. Space Mono carries filenames, values, units, scales, and telemetry with stable widths and tabular alignment.

### Hierarchy

- **Module Title** (700, 17px, 1 line-height): top-level module headers in uppercase, paired with an etched hatch fill.
- **Control** (700, 13px, 1 line-height): buttons and navigation actions.
- **Compact Display** (700, 11–14px): source title, strip labels, knob labels, and local headings.
- **Data** (700, 10px, 1 line-height): readouts, sample rows, select values, and steppers.
- **Micro Data** (700, 6–9px, 1 line-height): knob endpoints, spectral axes, meter scales, formats, and technical metadata.

### Named Rules

**The Face Split Rule.** Pixelify Sans names and commands; Space Mono measures and reports. Do not swap their jobs.

**The Uppercase Panel Rule.** Operational labels are terse uppercase equipment markings, not sentence-case application copy.

## Layout

The full instrument is a fixed 960×647 logical canvas with 4px outer padding and 4px gaps. Its primary row stack is 44px header, 278px source area, 41px global strip, and 264px effect area. The source area splits into a 252px sample browser and the selected-source workspace; the effect rack uses five adjacent modules, with the first three equal and Spectral Draw and Output tuned slightly narrower/wider to fit their instruments.

Top-level modules share a 28px header. Sample rows are 30px high. The source display reserves a 170px waveform chamber and a 72px control deck. The global strip is a fixed 41px bridge between editing and processing. Primary macro knobs are 66px; source gain uses the subordinate 38px knob.

There is no responsive reflow. The editor scales as one composition from the top-left at exactly 75%, 100%, 125%, or 150%. Internal proportions, type, borders, and interaction geometry remain unchanged at every preset.

**The One Instrument Rule.** Scale the complete 960×647 bench; never rearrange, wrap, collapse, or independently resize its modules.

**The Four-Pixel Rhythm Rule.** Top-level separation is 4px. Use smaller values only for internal optical fitting, not to create a competing spacing system.

## Elevation & Depth

The system is flat by default and uses no ambient shadows. Depth is structural: nested black tones, one-pixel ivory or dim rules, inset selection bars, circular knob rings, and the contrast between panel planes and deep display chambers. The header’s empty equipment bay alone uses a restrained dark linear gradient to suggest a recessed metal slot.

### Shadow Vocabulary

- **Selected Row Inset** (`inset 2px 0 var(--ivory)`): the sole rectangular selection indicator.
- **Primary Knob Rings** (`0 0 0 1px var(--black), 0 0 0 2px var(--muted)`): concentric calibration rings around 66px macro knobs.
- **Source Gain Ring** (`0 0 0 1px var(--muted)`): the quieter ring for the subordinate 38px gain knob.
- **Fader Cap Groove** (`inset 0 3px 0 var(--muted)`): a hard engraved line on the output fader cap.

**The Structural Depth Rule.** Use rules, tonal nesting, and calibration rings for depth; never add blur, glass, glow, or floating-card shadows.

## Shapes

Panels, buttons, selects, rows, readouts, display chambers, and fader parts are square. Borders are one pixel unless a major chassis edge or rotary ring requires a stronger stroke. Dashed outlines describe focus, drop targets, and calibration arcs. The only rounded geometry is the functional circle of a knob and its concentric scale.

**The Square Hardware Rule.** A zero-radius rectangle is the default silhouette. Radius is not a softness control; it is reserved for rotary mechanics.

## Components

### Buttons

- **Shape:** Square, one-pixel calibration border, 28px minimum height, compact 2px × 10px inset.
- **Default:** Panel-black fill with warm-ivory Pixelify label.
- **Hover / Active:** Hard inversion to warm ivory with bench-black text; pressed state moves down exactly 1px.
- **Focus:** One-pixel dashed warm-ivory outline inset by 4px.
- **Disabled:** Retain structure at 36% opacity with the default cursor.

### Inputs / Fields

- **Selects:** Bench-black field, one-pixel calibration border, square corners, 32px source-control height or 28px global-strip height.
- **Steppers:** A black numeric field joined to a 19px up/down rail; hover on the arrows uses hard ivory inversion.
- **Readouts:** Small black chambers with one-pixel borders, 23px height, and bold tabular data.
- **Focus:** Use the shared inset dashed outline; never add a glow.

### Cards / Containers

- **Corner Style:** Square.
- **Background:** Panel black by default; bench black for embedded display chambers.
- **Shadow Strategy:** None; use structural depth tokens only.
- **Border:** One-pixel calibration line on primary modules, dim rule on subordinate displays.
- **Internal Padding:** 6px is the recurring module inset, with 4px between top-level modules.

### Navigation

Navigation is a joined row of 30px-high Pixelify buttons. Adjacent items share borders; the current page is the same hard ivory inversion used by every other selected state. The header’s 510×30 empty bay remains unlabelled and does not become a logo placeholder.

### Rotary Controls

The primary macro knob is 66px with a 9px ivory ring, a dashed outer calibration orbit, and a 276-degree operating sweep from −138 degrees. Source gain is a reduced 38px version with a 6px ring. The invisible range input extends over the ring so the visual silhouette stays mechanical while the hit area remains forgiving.

### Display Chambers

Waveform, effect, spectral, and meter displays are true black instrument fields with crisp, unsmoothed canvas rendering. Etched one-pixel graticules organize the field; warm telemetry pixels carry the signal. Spectral editing uses a crosshair cursor, visible scan line, and terse axis markings. Static empty states remain muted.

### Sample Rows

Rows are exactly 30px high with a 20px square enable switch, an ellipsized Space Mono filename, and a compact action. Hover and selection raise the row one tonal step; selection also adds the two-pixel ivory inset bar.

## Do's and Don'ts

### Do:

- **Do** preserve the 960×647 logical canvas and the 75/100/125/150 whole-interface scale presets.
- **Do** keep top-level gaps at 4px, module headers at 28px, and sample rows at 30px.
- **Do** reserve the header’s left equipment bay as intentionally empty and unbranded.
- **Do** use hard black/ivory inversion for active, selected, and hover states.
- **Do** render waveform and telemetry canvases with image smoothing disabled.
- **Do** let real signal telemetry be the only continuous motion on the surface.

### Don't:

- **Don't** add responsive reflow, breakpoint-specific rearrangement, or fluid typography.
- **Don't** introduce accent hues, gradients outside the recessed equipment bay, or more white variants.
- **Don't** round panels, buttons, fields, rows, readouts, or display chambers.
- **Don't** add ambient animation, decorative pulses, loading shimmer, or easing to audio controls.
- **Don't** add a wordmark, subtitle, icon, or placeholder copy to the empty header bay.
- **Don't** soften construction with blurred shadows, glass effects, glow, or anti-aliased canvas graphics.
