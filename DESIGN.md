---
name: recompiler.dll — Black Calibration Bench
description: A fixed-scale, monochrome instrument panel built from machined black surfaces, warm ivory markings, and live signal chambers.
colors:
  bench-black: "#070806"
  panel-black: "#121410"
  raised-black: "#1a1d18"
  control-black: "#252921"
  warm-ivory: "#e9e8df"
  calibration-line: "#d5d6cc"
  muted-marking: "#a5a79c"
  dim-rule: "#5c6158"
  graticule: "#3e413a"
  telemetry-white: "#eeede5"
typography:
  module-title:
    fontFamily: "Spleen 8x16 Local, monospace"
    fontSize: "16px"
    fontWeight: 700
    lineHeight: 1
    letterSpacing: "0.01em"
  control:
    fontFamily: "Spleen 6x12 Local, monospace"
    fontSize: "12px"
    fontWeight: 700
    lineHeight: 1
  data:
    fontFamily: "Cozette Local, monospace"
    fontSize: "10px"
    fontWeight: 600
    lineHeight: 1
  micro-data:
    fontFamily: "Cozette Local, monospace"
    fontSize: "8px"
    fontWeight: 600
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
    height: "20px"
  primary-knob:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    rounded: "{rounded.dial}"
    size: "58px"
  source-gain-knob:
    backgroundColor: "{colors.panel-black}"
    textColor: "{colors.warm-ivory}"
    rounded: "{rounded.dial}"
    size: "38px"
---

# Design System: recompiler.dll — Black Calibration Bench

## Overview

**Creative North Star: "Black Calibration Bench"**

This is a compact operating surface, not a website: a near-black machined sampler workstation whose hierarchy comes from exact partitions, warm ivory calibration marks, supplied pixel artwork, and instrument-like readouts. Recessed CRT chambers carry the waveform, FAULT, BLEED, ETCH, and meter telemetry while the surrounding chassis stays quiet, dense, and mechanically legible.

The interface uses a fixed 1080×675 logical geometry and treats every line as functional construction. Spleen names and commands; Cozette measures and reports. The primary supplied RECOMPILER mark is fitted into the workstation's top rail, while the two creator marks support the ABOUT manual.

**Key Characteristics:**

- Fixed 1080×675 logical instrument canvas with whole-UI scale presets only.
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

**Module Font:** Spleen 8×16 Local at its native 16px strike.

**Control Font:** Spleen 6×12 Local at its native 12px strike.
**Data Font:** Cozette Local.

**Character:** Spleen gives module names and actions a crisp industrial bitmap voice without arcade styling. Cozette carries filenames, values, units, scales, metadata, and the ABOUT manual with compact bitmap rhythm.

### Hierarchy

- **Module Title** (700, 17px, 1 line-height): top-level module headers in uppercase, paired with an etched hatch fill.
- **Control** (700, 13px, 1 line-height): buttons and navigation actions.
- **Compact Display** (700, 11–14px): source title, strip labels, knob labels, and local headings.
- **Data** (700, 10px, 1 line-height): readouts, sample rows, select values, and steppers.
- **Micro Data** (700, 6–9px, 1 line-height): knob endpoints, spectral axes, meter scales, formats, and technical metadata.

### Named Rules

**The Face Split Rule.** Spleen names and commands; Cozette measures and reports. Do not swap their jobs.

**The Uppercase Panel Rule.** Operational labels are terse uppercase equipment markings, not sentence-case application copy.

## Layout

The full instrument is a fixed 1080×675 logical canvas with 4px outer padding and 4px gaps. Its primary row stack is 52px header, 286px source area, 40px global strip, and the remaining effect rack. The source area splits into a 270px POOL and the wider SOURCE workspace. The effect rack uses four adjacent modules: FAULT receives 420px and the largest visualizer; BLEED, ETCH, and MASTER use narrower task-specific widths.

Top-level modules share a 28px header. Sample rows are 28px high. SOURCE reserves a 158px waveform chamber and a 68px control deck. The global strip is a fixed 40px bridge between editing and processing. FAULT, BLEED, and ETCH use the same 58px PRESSURE knob. SOURCE TRIM is intentionally a horizontal slider.

There is no responsive reflow. The editor scales as one composition from the top-left at exactly 75%, 100%, 125%, or 150%. Internal proportions, type, borders, and interaction geometry remain unchanged at every preset.

**The One Instrument Rule.** Scale the complete 1080×675 bench; never rearrange, wrap, collapse, or independently resize its modules.

**The Four-Pixel Rhythm Rule.** Top-level separation is 4px. Use smaller values only for internal optical fitting, not to create a competing spacing system.

## Elevation & Depth

The system is flat by default. Depth is structural: nested black tones, one-pixel ivory or dim rules, inset selection bars, circular knob rings, and the contrast between panel planes and recessed display chambers. Only actual graphical CRT windows receive subtle inset phosphor falloff, scanlines, and edge vignette.

### Shadow Vocabulary

- **Selected Row Inset** (`inset 2px 0 var(--ivory)`): the sole rectangular selection indicator.
- **Primary Knob Rings** (`0 0 0 1px var(--black), 0 0 0 2px var(--muted)`): concentric calibration rings around 66px macro knobs.
- **Fader Cap Groove** (`inset 0 3px 0 var(--muted)`): a hard engraved line on the output fader cap.

**The Structural Depth Rule.** Use rules, tonal nesting, and calibration rings for chassis depth. Restrict subtle glow and glass falloff to graphical CRT windows; never float modules like web cards.

## Shapes

Panels, buttons, selects, rows, readouts, display chambers, and fader parts are square. Borders are one pixel unless a major chassis edge or rotary ring requires a stronger stroke. Dashed outlines describe focus, drop targets, and calibration arcs. The only rounded geometry is the functional circle of a knob and its concentric scale.

**The Square Hardware Rule.** A zero-radius rectangle is the default silhouette. Radius is not a softness control; it is reserved for rotary mechanics.

## Components

### Buttons

- **Shape:** Square, one-pixel calibration border, 28px minimum height, compact 2px × 10px inset.
- **Default:** Panel-black fill with a warm-ivory Spleen UI label.
- **Hover / Active:** Hard inversion to warm ivory with bench-black text; pressed state moves down exactly 1px.
- **Focus:** One-pixel dashed warm-ivory outline inset by 4px.
- **Disabled:** Retain structure at 36% opacity with the default cursor.

### Inputs / Fields

- **Selects:** Bench-black field, one-pixel calibration border, square corners, 32px source-control height or 28px global-strip height.
- **Steppers:** A black numeric field joined to a 19px up/down rail; hover on the arrows uses hard ivory inversion.
- **Editable values:** Plain, unboxed Cozette text lines with a subtle underline only on hover or edit. PRESSURE, TRIM, VOL, and PITCH share click-to-edit, Enter/blur commit, Escape cancel, and arrow-key adjustment.
- **Focus:** Use the shared inset dashed outline; never add a glow.

### Cards / Containers

- **Corner Style:** Square.
- **Background:** Panel black by default; bench black for embedded display chambers.
- **Shadow Strategy:** None; use structural depth tokens only.
- **Border:** One-pixel calibration line on primary modules, dim rule on subordinate displays.
- **Internal Padding:** 6px is the recurring module inset, with 4px between top-level modules.

### Navigation

Navigation is a joined row of mechanical MAIN / ABOUT keys. Adjacent items share borders; the current page uses the same hard ivory inversion as every other selected state. The supplied primary wordmark occupies the remaining top rail without becoming oversized.

### Rotary Controls

The shared PRESSURE knob is 58px with an 8px ivory ring, a dashed outer calibration orbit, and a conventional 270° sweep from lower-left through straight up to lower-right. FAULT, BLEED, and ETCH use this exact component. Vertical dragging uses 180 logical pixels for full travel; Shift drag is 6× finer. Double-click resets and the wheel adjusts. SOURCE TRIM remains a horizontal gain slider because its job differs.

### Control Families

- **Selection keys:** MAIN / ABOUT, STACK, VOICES, and FAULT's FLIP / DUST / WARP mutations use the same hard ivory active inversion and dark inactive state.
- **Action keys:** IMPORT, REMOVE, and CLEAR use the shared dashed hardware-action treatment and never imply selected state.
- **Effect enables:** FAULT, BLEED, and ETCH use one 22px square status switch at the far-right edge of each module header.
- **Creative macro:** Each creative engine presents `PRESSURE → knob → editable value`; the interface contract is shared while each DSP meaning remains specific.
- **Source controls:** TUNE, DRIFT, and STRETCH are one stepper family. TRIM is a horizontal slider aligned to the same four-column control deck.
- **Master controls:** VOL and PITCH are true sibling faders with identical tracks, caps, ticks, and editable-value baselines. Unity and zero are stronger ticks rather than duplicated endpoint labels.

### Display Chambers

Waveform, effect, spectral, and meter displays are true black instrument fields with crisp, unsmoothed canvas rendering. Etched one-pixel graticules organize the field; warm telemetry pixels carry the signal. Spectral editing uses a crosshair cursor, visible scan line, and terse axis markings. Static empty states remain muted.

### Sample Rows

Rows are exactly 28px high with a 20px square enable switch, an ellipsized Cozette filename, and a compact action. Hover and selection raise the row one tonal step; selection also adds the two-pixel ivory inset bar.

## Do's and Don'ts

### Do:

- **Do** preserve the 1080×675 logical canvas and the 75/100/125/150 whole-interface scale presets.
- **Do** keep top-level gaps at 4px, module headers at 28px, and sample rows at 28px.
- **Do** use the supplied primary mark in the header and both creator marks on ABOUT without redrawing them.
- **Do** use hard black/ivory inversion for active, selected, and hover states.
- **Do** render waveform and telemetry canvases with image smoothing disabled.
- **Do** let real signal telemetry be the only continuous motion on the surface.

### Don't:

- **Don't** add responsive reflow, breakpoint-specific rearrangement, or fluid typography.
- **Don't** introduce accent hues, gradients outside the recessed equipment bay, or more white variants.
- **Don't** round panels, buttons, fields, rows, readouts, or display chambers.
- **Don't** add ambient animation, decorative pulses, loading shimmer, or easing to audio controls.
- **Don't** replace supplied raster marks with text, vectors, or invented artwork.
- **Don't** extend scanlines, vignette, glass, or phosphor glow beyond actual graphical display windows.
