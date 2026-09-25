# recompiler.dll User Manual

This manual covers the current Windows VST3 version of recompiler.dll. It is written for producers who already know how to load an instrument plug-in and route MIDI in a DAW.

recompiler.dll is a sample-pool instrument. You load several audio files, set how each source should be trimmed and tuned, and play the pool from MIDI. Every note chooses one enabled source at random and starts from a random point inside that source's active region. Four global effects can then reshape the mixed result before it reaches the output.

The basic signal flow is:

**Sample pool -> source tuning and region -> MIDI voices -> Scramble -> Melt -> Spectral Draw -> Smear -> Output**

## 1 What recompiler.dll is

recompiler.dll is designed for turning a small folder of samples into a playable instrument that changes from note to note. It works especially well with vocal chops, drum hits, field recordings, one-shots, and short textures.

The plug-in is an instrument, so it receives MIDI and produces stereo audio. It does not process an audio input track. The current release is built as a Windows VST3 plug-in and a standalone application. The VST3 bundle is named `recompiler.dll.vst3`, while the plug-in name shown in the DAW is `recompiler.dll`.

Use it in a Windows DAW that supports VST3 instruments, such as FL Studio, Ableton Live, or REAPER. The current repository does not build an Audio Unit, so this release does not load natively in Logic Pro on macOS.

The normal workflow is:

1. Load up to 20 samples.
2. Trim and tune each source.
3. Play MIDI notes to trigger random sources and random starting positions.
4. Use Scramble, Melt, Spectral Draw, and Smear to reshape the combined sound.
5. Set the final output level.

## 2 Quick start

1. Insert recompiler.dll on an instrument track and route MIDI to it.
2. Click the drop area in the Samples panel, or click **Add Samples** on the Settings page, and choose one or more supported audio files.
3. Play a MIDI note. Each note-on chooses one enabled sample at random.
4. Use **Transpose** and **Fine Tune** when a source needs manual pitch correction.
5. Turn up **Scramble**, **Melt**, **Smear**, or **Spectral Depth**. Scramble and Melt may wait for the next tempo-grid boundary before their sound becomes obvious.
6. Leave **VOL** at 100% for unity gain, adjust **PITCH** if the whole instrument needs transposing, and make sure **Mute** is off.

For the most predictable first test, load one short WAV file, keep **Chords** off, leave **PITCH** and all four effect amounts at 0, and leave **VOL** at 100%.

## 3 Sample pool

### Capacity and formats

The pool holds a maximum of 20 samples. The supported filename extensions are:

- WAV
- AIF and AIFF
- MP3
- FLAC

Each source is decoded into memory. A source whose decoded audio would exceed 256 MiB is rejected.

### Importing samples

You can import several files at once in either of these ways:

- Click **DROP WAV, AIFF, MP3 OR FLAC** in the Samples panel and choose files.
- Open **Settings** and click **ADD SAMPLES**.
- Drag one or several supported files onto the Samples drop area. On Windows, the WebView2 editor passes the real file paths to the native importer; it does not reopen the chooser.

The file chooser only offers supported formats. After an import, the drop area reports how many files were added and rejected. A rejected file may be unsupported, damaged, too large, undecodable, or beyond the 20-sample limit.

### Working with the list

- Click a row to select it for editing. Selection does not audition the sample.
- Click the check box to enable or disable a source. Disabled sources stay in the project but are not chosen for playback.
- Click **REMOVE** to remove a source from the pool. This does not delete the audio file from disk.

Missing and disabled sources are excluded from random playback.

### Random selection

Each MIDI note-on chooses one enabled, playable source with equal probability. There is no weighting control. If five sources are enabled, each has the same chance of being chosen, regardless of file length.

After choosing a source, the plug-in also chooses a random legal starting position between that source's Start and End markers. Playback continues toward the End marker unless the MIDI note is released first. The region does not loop.

## 4 Source controls

Source controls apply only to the sample selected in the list.

### Waveform

The waveform shows the selected audio file. The darkened areas outside the markers are not used for new triggers. The file name, sample rate, bit depth, and duration appear above the waveform when that information is available.

### Start and End

**Start** and **End** define the allowed playback region. Drag inside the waveform; the nearer marker follows the pointer. A new note can begin at a random point within this region, then plays toward End.

For keyboard adjustment, focus a marker and use the Left or Right arrow keys. Hold Shift for a larger step. Home and End move the focused marker toward the beginning or end of the file while preserving a valid region.

Use a tight region to focus on a syllable or drum hit. Use a wide region to get more variation from random start positions.

### Transpose

**Transpose** applies a fixed pitch shift from -24 to +24 semitones.

- +12 moves the source one octave up.
- -12 moves it one octave down.
- +7 moves it up a perfect fifth.

Transpose is applied in addition to Fine Tune, global PITCH, and, when Chords is on, the incoming MIDI-note offset. The plug-in does not perform automatic key detection or source-to-target key correction.

### Fine Tune

**Fine Tune** adjusts pitch from -100 to +100 cents. One hundred cents equals one semitone. Use it to correct a slightly sharp or flat recording, or to create deliberate detuning.

### Gain

**Gain** sets the selected source's level from -60 dB to +12 dB before the global effects. Use it to balance quiet and loud files so that random source changes do not create large volume jumps.

## 5 Chords

The **Chords** switch changes how incoming MIDI note numbers affect pitch.

### Chords off

Incoming notes act as triggers. Their pitches are ignored. Every MIDI key plays the chosen source using its Transpose, Fine Tune, and global PITCH settings.

Use this mode for one-shot triggering, fixed-pitch vocal chops, drum pools, and rhythmic patterns where different MIDI notes should not create a melody.

### Chords on

Incoming MIDI notes change playback pitch relative to a fixed neutral reference. MIDI note 72 is zero offset, each semitone above raises playback by one semitone, and each semitone below lowers it by one. This stays predictable because it does not depend on sample metadata or a global key selector.

Different DAWs label octaves differently, so the reference may appear as C4 or C5 even though the MIDI note number is the same.

## 7 Poly and Mono

### Poly

**POLY** lets notes overlap. The audio engine can play up to 16 voices at once. If a seventeenth voice is needed, the oldest active voice is reused. Releasing a MIDI key fades voices that were triggered by that note.

Use Poly for chords, pads, stacked rhythmic hits, and overlapping tails.

### Mono

**MONO** uses one main voice. A new note replaces the previous note with a short click-reducing transition. It is a straightforward retriggering mono mode, not a legato or portamento system.

Use Mono for bass lines, lead parts, and patterns where each new trigger should cut the previous one.

The header currently displays an active count as `00 / 20 VOICES`. The engine itself has 16 playback voices; the displayed denominator is inconsistent with the audio engine.

## 8 Scramble

Scramble rearranges short pieces of the combined sampler output on an automatic tempo grid. It can jump to different pieces, create short holds or repeats, reverse fragments, and introduce half-speed or double-speed pitched fragments.

The **Scramble** knob runs from 0 to 100:

- 0 is a settled dry bypass.
- Low values alter a small part of each active phrase.
- Higher values divide the phrase into more slices, manipulate more of them, and make repeats and pitch gestures more pronounced.

Scramble arms when its amount rises above 0 and begins on the next available grid boundary after it has usable audio history. The grid is chosen automatically from 1/8, 1/16, or 1/32 notes to stay near a 125 ms slice at the host tempo. When reliable host timing is unavailable, the plug-in falls back to its latest valid tempo or 120 BPM.

The current WebView interface does not expose separate Scramble gesture switches. Its jump, hold, reverse, pitch, and motif behavior is chosen automatically. The **SCRAMBLE** button on the Settings page enables or bypasses the entire engine without changing the knob value.

Useful starting points:

- 10 to 30 for small drum or percussion variations.
- 30 to 60 for vocal rearrangement.
- 60 to 100 for obvious glitch phrases and aggressive repeats.

## 9 Melt

Melt stretches small slices while largely preserving their pitch. The result ranges from soft, pulled-apart timing to heavily elongated, folding fragments. Some slices reverse automatically.

The **Melt** knob runs from 0 to 100:

- 0 is a settled dry bypass.
- Low values use fewer slices, gentler stretching, and less wet signal.
- High values use more slices, stronger time expansion, shorter grain detail, and a higher chance of reversed slices.

The internal stretch range grows roughly from 1.08x to 4x as the amount increases. Melt starts on a tempo-grid boundary, so it may not react at the exact instant you move the knob. It also needs recent audio to stretch.

The current WebView interface has no separate Melt mode controls. Stretch, slice variation, and reversal are automatic. The **MELT** button on the Settings page enables or bypasses the full engine.

Use Melt for dragged-out vocals, unstable transitions, half-frozen ambience, and time-stretched fills.

## 10 Smear

Smear turns recent audio into a moving stereo grain cloud. It uses short overlapping fragments with musical pitch offsets, scattered read positions, pitch and pan motion, stereo spread, bright high-frequency emphasis, and bounded feedback. Transients are allowed to remain clearer by reducing the wet texture around sharp attacks.

The **Smear** knob runs from 0 to 100:

- 0 is a settled dry bypass.
- Low values create a sparse cloud with a few longer grains.
- High values create more simultaneous grains, shorter fragments, wider motion, and a denser crystalline texture.

Smear needs a little recent audio history before grains can appear. Its detailed grain features are automatic in the current WebView interface; there are no separate dispersion, pitch, stereo, or feedback controls. The **SMEAR** button on the Settings page enables or bypasses the whole engine.

Use Smear for shimmering vocal tails, wide percussion dust, animated pads, and ambient transitions.

## 11 Spectral Draw

Spectral Draw is a time-varying attenuation mask. It removes selected frequency areas as a scanner travels across the drawing. It is not a normal static EQ, and it cannot boost frequencies.

### Reading the canvas

- The top of the canvas is labeled **HIGH** and represents higher frequencies.
- The bottom is labeled **LOW** and represents lower frequencies.
- Time moves from left to right.
- The bright vertical line shows the current scan position.
- Brighter painted cells produce more attenuation when the scanner reaches them.

### Drawing and erasing

- Left-drag or touch-drag to draw.
- Right-drag, Shift-drag, or Alt-drag to erase.
- With keyboard focus, use the arrow keys to move the cursor. Shift plus an arrow moves farther. Space or Enter draws; Delete, Backspace, or Shift plus Space erases.
- Click **RESET** to clear the whole mask.

The **Spectral Depth** knob sets how strongly the drawing attenuates the sound. At 0, the mask has no audible effect. At 100, fully painted areas can be completely removed. An empty canvas does nothing at any depth.

The scanner follows the host song position when valid tempo and position information are available. Its cycle is selected automatically from 2, 4, 8, or 16 quarter notes to stay near two seconds. It continues from a safe tempo fallback when host position is unavailable.

Spectral Draw adds 1024 samples of reported plug-in latency. Most DAWs compensate automatically. The **SPECTRAL DRAW** button on the Settings page bypasses the engine without erasing the picture. **CLEAR SPECTRAL MASK** on Settings performs the same clear action as Reset.

## 12 Output

The Output panel controls the final stereo signal after all creative effects.

### Left and right meter

The **L** and **R** bars show the recent output peaks for the two channels. They are visual meters only and do not change the sound.

### VOL

The VOL fader is shown as 0% to 125%. It uses a smooth perceptual curve rather than treating the percentage as linear amplitude:

- 0% is true silence.
- 1% is approximately -65 dB.
- 100% is unity gain, or 0 dB.
- 125% is approximately +5.6 dB.

There is no final limiter. If the plug-in or DAW channel clips, lower VOL, reduce individual source Gain, or reduce effect amounts.

### PITCH

The global PITCH fader transposes every newly triggered voice from -12 to +12 semitones. Its default is 0. It stacks with each source's Transpose and Fine Tune settings and with the MIDI offset when Chords is on.

### Mute

**MUTE** silences the final output with a short smooth transition. When muted, the button changes to **UNMUTE**. Muting does not unload samples or reset effects.

## 13 Settings

Open the Settings page from the top-right navigation. The current page contains four groups.

### Engine

The Engine panel shows three informational labels:

- Audio is native C++.
- The editor is embedded and works offline without loading a website.
- Project state is saved through the host.

These labels are status information, not controls.

### Interface Scale

Choose one of four fixed editor sizes:

- 75 percent: 720 x 485
- 100 percent: 960 x 647
- 125 percent: 1200 x 809
- 150 percent: 1440 x 971

The scale setting is saved with the plug-in state. Some DAWs may constrain the window size.

### Sample Library

This panel shows the number of pool entries, up to 20, and repeats the supported formats. **ADD SAMPLES** opens the multi-file chooser.

### Effect Engines

The **SCRAMBLE**, **MELT**, **SMEAR**, and **SPECTRAL DRAW** buttons enable or bypass their complete engines. Bypassing an engine does not reset its amount or erase the spectral mask, and the enable states are saved.

**NEW RANDOM SEED** creates a new random pattern for future sample choices and creative-effect decisions. The seed is saved with the project, but there is no numeric seed display or manual seed entry.

**CLEAR SPECTRAL MASK** erases the full Spectral Draw canvas.

## 14 Saving projects

When the DAW saves the plug-in state, recompiler.dll stores:

- All host parameters: Chords, Poly or Mono, the four effect amounts, VOL, and global PITCH.
- The sample list and each file's external path.
- Each source's enabled state, Start, End, Transpose, Fine Tune, and Gain. Legacy Source Key metadata may remain in older projects but is inert.
- The selected sample.
- The Spectral Draw canvas.
- Output mute.
- Interface scale.
- Effect-engine enable states.
- The internal creative seed.

The plug-in does not embed audio files in the DAW project. If a source file is moved, renamed, disconnected, or deleted, the project restores a missing entry instead of playable audio. A missing row is crossed out and excluded from random selection. The current interface does not provide a relink command; restore the file at its saved path or remove the entry and import the file again.

The Main or Settings tab choice, active notes, meters, and live effect history are not saved.

## 15 Tips and creative workflows

### Vocal chop workflow

1. Load several short vocal phrases.
2. Tune each phrase manually with Transpose and Fine Tune.
3. Turn Chords on and play a simple MIDI chord or melody around MIDI note 72.
4. Use global PITCH when the whole instrument needs an octave or key-center shift.
5. Add Scramble around 20 to 45 for rhythmic variation, then add a small amount of Melt for stretched endings.

### Drum glitch workflow

1. Load kicks, snares, percussion, or full drum loops.
2. Leave Chords off and keep PITCH at 0.
3. Narrow Start and End around the useful part of each file.
4. Balance the source Gain controls.
5. Raise Scramble until the rhythm begins to break apart. Try a little Smear for stereo debris.

### Texture and ambience workflow

1. Load field recordings, noise, and sustained material.
2. Use wide source regions for greater random variation.
3. Raise Melt and Smear gradually.
4. Draw a few broad shapes in Spectral Draw, then adjust Spectral Depth while the track plays.
5. Leave headroom at Output because overlapping grains can increase peaks.

### Melodic sample workflow

1. Load one or more pitched one-shots and align them manually with Transpose and Fine Tune.
2. Turn Chords on and use Poly for chords or Mono for a lead.
3. Treat MIDI note 72 as the neutral note, then use global PITCH for a broad octave shift if needed.
4. Correct tuning with Fine Tune and set octaves with Transpose.
5. Start with the creative effects at 0, then add them after the basic pitch behavior is correct.

## 16 Troubleshooting

### No sound

- Confirm that at least one sample is loaded, enabled, and not marked missing.
- Send MIDI notes to the plug-in's instrument track.
- Make sure Output is not muted and VOL is above 0%.
- Check that Start and End leave a usable region.
- Check the DAW track mute, monitor, and routing settings.

### A sample will not load

- Use WAV, AIF, AIFF, MP3, or FLAC.
- Confirm that the file exists and can be opened in another audio application.
- Try a smaller file. A source over the 256 MiB decoded-audio limit is rejected.
- Check whether the pool already contains 20 entries.
- Read the added/rejected count shown in the drop area. Converting a rejected file to a standard PCM WAV is a useful diagnostic step.

### The wrong sample is playing

Sample-row selection chooses which source you are editing; it does not force that source to play. Every note randomly chooses among all enabled, playable sources. Disable the other rows when you need to hear one source by itself.

### The sample starts in an unexpected place

Random start position is part of the instrument. Narrow the Start and End region around the exact material you want, but note that playback may still begin anywhere inside that region.

### The pitch is wrong

- Set Transpose and Fine Tune to 0 while diagnosing.
- Set global PITCH to 0.
- If Chords is on, remember that incoming MIDI changes pitch relative to fixed MIDI note 72.
- Large pitch shifts also change playback speed and duration.

### Chords do not change pitch

Turn Chords on. With Chords off, MIDI note numbers are ignored and only trigger playback.

### An effect appears inactive

- Raise its amount above 0.
- Check the matching Effect Engines button on Settings.
- Scramble and Melt wait for a tempo boundary and need recent audio history.
- Smear needs recent audio before its grain cloud can form.
- The animated effect displays are activity views, not editable controls.

### Spectral Draw does nothing

- Draw visible marks on the canvas.
- Raise Spectral Depth above 0.
- Enable Spectral Draw on the Settings page.
- Let the scanner reach the painted area.
- Paint over the frequency region occupied by the sound. For example, a low-frequency mask may have little effect on a bright cymbal.

### The project reopened with a missing file

The plug-in stores file paths, not the audio itself. Put the source back at its original path, or remove the crossed-out row and import the file from its new location. Keep project samples in a stable project folder before saving or sharing a session.

### The editor is too large or too small

Open Settings and choose 75, 100, 125, or 150 percent. If a DAW still crops the editor, close and reopen the plug-in window after changing scale, or choose a smaller setting.

### The DAW reports latency

Spectral Draw requires 1024 samples of processing latency, including when its depth is at 0 so that timing remains stable. Enable the DAW's plug-in delay compensation for aligned playback.

## 17 Control reference

| Control | What it does | Typical range or options |
| --- | --- | --- |
| Main | Opens the main sample and effects page | Main |
| Settings | Opens settings and utility actions | Settings |
| Sample drop area | Opens the sample chooser on click; directly imports Windows file drops | WAV, AIF, AIFF, MP3, FLAC |
| Sample row | Selects a source for editing | Up to 20 rows |
| Sample check box | Includes or excludes the source from random playback | Enabled or disabled |
| Remove | Removes the source from the pool, not from disk | Per source |
| Start | Sets the earliest allowed random start | Beginning to just before End |
| End | Sets the playback-region end | Just after Start to file end |
| Transpose | Adds a fixed pitch offset | -24 to +24 semitones |
| Fine Tune | Corrects or detunes pitch | -100 to +100 cents |
| Source Gain | Balances one source before global effects | -60 to +12 dB |
| Chords | Chooses fixed-pitch triggers or MIDI-following pitch | OFF or ON |
| Poly or Mono | Allows overlapping voices or one replacing voice | POLY or MONO |
| Scramble | Controls rhythmic slice rearrangement and glitch gestures | 0 to 100 |
| Melt | Controls automatic pitch-preserving slice stretching | 0 to 100 |
| Smear | Controls the density and intensity of the stereo grain cloud | 0 to 100 |
| Spectral canvas | Draws or erases time-varying frequency attenuation | 128 x 64 mask |
| Spectral Reset | Clears the spectral mask | Reset |
| Spectral Depth | Controls how strongly the mask attenuates sound | 0 to 100 |
| Output meter | Displays left and right output peaks | L and R visual display |
| VOL | Sets final plug-in gain on a perceptual curve | 0% to 125%; 100% is unity |
| PITCH | Transposes every newly triggered voice | -12 to +12 semitones |
| Mute | Smoothly silences or restores final output | Mute or Unmute |
| Interface Scale | Changes the fixed editor size | 75, 100, 125, or 150 percent |
| Add Samples | Opens the multi-file sample chooser | Settings page |
| Effect Engine buttons | Enable or bypass each complete creative engine | Scramble, Melt, Smear, Spectral Draw |
| New Random Seed | Starts a new future random pattern | One action; no numeric entry |
| Clear Spectral Mask | Clears the complete Spectral Draw canvas | One action |
