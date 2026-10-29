# Drum sound editor (0.10.2)

Select an instrument in the kit workspace and click **Edit sound**. The editor has seven pages; scroll within a page to reach additional controls. **Back to kit** returns to the instrument cards. Use Back to kit before switching kits. All editing is non-destructive and saved in the DAW project; sample files remain external.

## Waveform on every page

The waveform stays above the scrolling controls on all seven tabs. The view selector offers:

- **Edited** (default): one hit rendered through Drum's current trim/reverse, gain/normalise, tuning, envelopes, tone filter, eight-band EQ, LFO, instrument mixer and macro settings. It updates after control edits, host macro changes and host tempo changes, without playing audio. At Y 1x its amplitude scale is ±1 (0 dBFS). Vertical zoom magnifies the display without changing gain; values beyond the chosen scale meet the display edge. The header reports the actual signal peak in dBFS. This is display clipping only.
- **Source:** the original file with Start/End and loop markers. Choose this to drag trim markers. This view scales to the source peak and does not show processing. Numeric Start/End controls also work while Edited is selected.
- **Send A–F:** the selected post-fader send signal, including its send level and macro mappings, before effects added in the DAW. An off send shows a flat line. These previews show the signal independently of whether its host output bus is enabled.

The preview uses the same voice DSP as playback, at the host sample rate and tempo, with 80% velocity and a one-second gate, matching manual Play. Trigger ignores note-off; Gate and Classic release after one second. It shows the instrument before destination routing and external DAW effects. Mute/solo suppression is reflected; polyphony, retrigger and choke need overlapping notes to show a difference. Pitch bend is centred. Random modulation uses a repeatable example and a free-running LFO starts at phase zero, so a live performance can differ.

Preview work runs in the background. **Updating preview...** marks a pending result; obsolete slider renders are cancelled. Zoom and scroll stay in place across tab/control changes, and Fit X restores the whole current waveform in time. A new instrument/sample or Source/Edited/Send selection resets horizontal zoom. Long sounds are limited to the first 30 seconds or 2.88 million stereo frames (whichever is shorter); the header explicitly labels a truncated preview. The source waveform and audible sample are not shortened.

### Waveform size and channels

- **Y zoom:** choose 0.25x–128x, or use **Shift + wheel** over the waveform. Choose Y 1x to reset. The setting stays fixed across tabs and sound edits, so level changes remain visible.
- **Fit Y:** fits the visible time range and displayed channel(s) once, choosing the largest available magnification that fits within the lane (subject to the zoom limits). Zoom into a quiet tail, then click Fit Y to inspect it. Silence resets to Y 1x. This does not enable continuous auto-scaling.
- **Height divider:** drag the grip immediately below the waveform to make the panel taller or shorter; double-click to restore its default height. The panel starts at 340 logical pixels where space permits and reserves room for the controls on the selected page.
- **Auto channels:** mono and exactly identical left/right signals use one full-height lane marked MONO. This includes a centred mono sample whose edited output has been duplicated to stereo. Once processing makes the channels differ, Auto shows separate L/R lanes.
- **Stereo L / R:** shows both channels for a stereo buffer, including duplicated mono. **Left only / Right only:** gives the selected channel the full height without summing or changing the audio. A mono source always shows its one available channel.

These are editor display preferences. They persist while the editor is open and are not saved as instrument controls. Source audio, stereo information, playback gain, pan and DAW state are unaffected.

## Sample

- **Start / End:** trim the playable region, as percentages of the original file. Choose Source to drag either green marker, or enter a numeric percentage in either view. The waveform preserves signed peaks at the current display resolution. Close zoom draws the individual samples as a continuous trace. **+ / -** zoom around the centre; **Ctrl + wheel** zooms at the pointer; ordinary wheel or horizontal scrolling pans; **Fit X** shows the entire waveform in time. The footer shows horizontal zoom and the visible duration in milliseconds. Zoom can reach a 32-sample window.
- **Sample gain:** -24 to +24 dB before the filter. **Normalise** compensates for the file's peak without rewriting it. Silent files remain silent.
- **Source channels:** Stereo preserves the file's channels. Mono averages left and right before the filter; a mono file retains its level. Instrument pan and pan modulation still follow this stage. Opposite-polarity stereo material may cancel when summed. This is a playback setting saved per instrument, distinct from the waveform's display-only channel selector. Changing a sounding voice crossfades over 5 ms. Normalise continues to use the original file peak. Edited shows the result; Source still shows the original channels.
- **Direction:** forward or reverse within the selected region. **Zero-crossing snap** searches up to 256 source frames for a nearby crossing in the left channel; markers retain the requested percentages while playback uses the snapped boundaries.
- **Trigger:** plays through note-off. **Gate:** fades out when the assigned MIDI note is released. Both use Fade in / Fade out. **Classic ADSR:** uses the amplitude envelope and can sustain a loop while the note is held. Unlike Simpler's strictly monophonic One-Shot mode, Drum retains configurable polyphony in Trigger mode for existing kits.
- **Voices:** 1–128 per instrument, within the global 128-voice pool. **Retrigger** cuts previous hits of the same instrument with a short fade. Choke groups remain independent of this setting.
- **Loop:** available in Classic mode. Loop start/end are relative to the trimmed region. Loop crossfade blends the boundary and is limited to half the loop length. Forward and reverse loops release on MIDI note-off. Manual Play uses a one-second gate for Gate/Classic, preventing indefinite preview loops; Stop is immediate.
- **Fade in/out:** milliseconds. A minimum two-millisecond end ramp remains to suppress discontinuities.
- **Slice to new group:** creates 2, 4, 8, 16, 32 or 64 equal regions on free MIDI keys, within the 88-instrument and 16-group limits. The original instrument remains intact. Slices share the source file and inherit controls, with Trigger mode and looping disabled. This operation rejects the entire change if capacity is insufficient.

Trim/loop markers, playback mode, direction and loop on/off are latched for each new hit. Other control edits can affect sounding voices. Reset page restores that page's defaults.

## Envelopes and pitch

Classic mode exposes amplitude Attack, Decay, Sustain and Release. The pitch envelope has its own ADSR and a bipolar semitone amount. Fine tuning, velocity-to-volume, velocity-to-pitch, random pan and MIDI pitch-bend range are also available. Pitch bend is per MIDI channel and defaults to zero range so old projects are unchanged. Pitch controls alter playback speed and duration.

The Edited waveform reflects these controls when active. Amplitude ADSR needs **Playback: Classic ADSR**; Trigger and Gate use Fade in/out instead. The pitch envelope needs a nonzero **Pitch envelope** amount. Editing its ADSR with amount zero leaves the sound unchanged. Source always shows the unprocessed file.

### Shape envelopes with points and curves

Amplitude and pitch graphs sit side by side below the waveform; Filter > Filter envelope has its own graph. Click and release on a line to add a point without changing its existing shape. Drag a point in both axes to set time and level; points stay in time order and within 0–100%. Double-click a point to remove it (minimum two, maximum 32). Drag a line with no minimum hold time to curve that segment around the grabbed position; horizontal movement shifts the bend placement.

Right-click a non-final point to mark **S**, the held level. The points before S run from note-on; points after S describe release time after note-off. A short MIDI note releases from the actual current level. There is no fixed sustain duration in the graph. A nonzero final level fades briefly to zero. New graph and numeric timing edits are limited to the **entire original sample duration**, independent of trim or tuning. Existing longer settings are preserved on load and brought within that limit when edited. Without a loaded sample the fallback limit is 80 seconds. Numeric time edits retain the chosen stage and proportionally fit other time stages into the remaining duration; defaults also obey the limit. MIDI hold time remains independent.

**ADSR values** reveals the optional numeric stage sliders. **ADSR** removes the custom shape and restores the stored numeric ADSR. Numeric stage sliders are disabled while a custom shape is active. Reset page clears custom shapes on that page. The curve changes audible DSP and is saved per instrument, including copies and slices.

Graphs have separate **- / + / Fit** time controls; wheel or horizontal scrolling pans, **Ctrl + wheel** zooms around the pointer, and middle-button dragging pans. Fit resumes automatic fitting; scale stays fixed during a gesture. Focus a point and use arrows for 1 ms / 1% changes, Shift for 0.1 ms / 0.1%, or Delete/Backspace to remove. Overlapping points have offset grips connected to the actual curve.

Audible amplitude graph edits switch to Classic automatically. Audible filter graph edits enable Filter and set -2 octaves when the amount is zero and not mapped. A click/release that only inserts a point preserves the current sound. Pitch/filter amount sliders are now visible in the graph headers. Moving, bending or deleting a pitch point with amount zero enables +12 st unless a macro owns the amount; adding a point alone preserves silence. Audible graph edits select **Edited**, so their effect is immediately visible. Numeric ADSR changes at zero pitch amount still leave the sound unchanged.

When a macro controls amplitude timing, the effective curve follows its value and the whole graph locks point editing. Click **Unlink timing** on the graph to keep the currently displayed timing and remove only its timing macro assignments, then edit the points. Other macro mappings and values are preserved. Removing a mapping from the Macros page instead restores the original base value. Amount mappings lock the graph's amount slider. Reset page leaves macro mappings intact.

Each graph has **Reset / Undo / Redo**. Reset restores its shape, numeric ADSR and modulation amount to defaults (while retaining playback/filter enable state); reset is disabled for macro-controlled timing or amount. Undo/Redo retains up to 64 edits per envelope for the currently selected instrument. A whole point/curve/slider drag is one entry; view navigation does not enter history. New edits clear redo. Changing instrument/sample, closing the editor or externally recalling different envelope settings clears history. Undo can restore older settings beyond the current sample-length limit.

Double-click a slider's **numeric box**, including an active text editor, to restore its default. Single-click still lets you type. This covers mixer, Edit Sound, envelope amounts, EQ and macro values/ranges. EQ frequency uses the selected band's default; macro values use 50%, and mapping min/max use the selected target's range.

### Trigger, pitch and Camelot

The header separately labels the assigned MIDI trigger, estimated processed-audio pitch (note/Hz or a varying range), and musical key/Camelot. Background analysis is bounded and can report uncertainty. A stable single drum tone cannot establish major/minor; noisy and short hits may have no reliable pitch.

**Key label (manual override)** selects a known major/minor key, explicitly labelled manual. Auto detect returns to analysis. Manual labels are metadata and do not transpose automatically when tuning changes. Source view still analyses the processed preview; Send views analyse the chosen send. External Live effects are excluded. Examples: A minor = 8A, C major = 8B, B-flat minor = 3A; see the [official Camelot chart](https://mixedinkey.com/camelot-wheel/).

## Filter

**8-band EQ** provides eight independently enabled stereo bands with low cuts (12/48 dB/oct), low shelf, bell, notch, high shelf and high cuts (12/48 dB/oct). Drag a dot horizontally for frequency, vertically for gain (Q for cuts/notch). Alt-drag or wheel changes Q; double-click a dot toggles its band. Numeric frequency (20 Hz–20 kHz), gain (±24 dB), Q (0.1–18) and output gain (±24 dB) are also available. Dragging enables the band and EQ. The combined response curve uses the same coefficients as playback; Edited includes the EQ. EQ is after the attack layer and before amplitude/mixing.

The EQ uses one linked stereo curve. Spectrum analysis, independent L/R or M/S, band audition and adaptive Q are not included. It takes interaction cues from [EQ Eight](https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/#eq-eight); it does not load or emulate the native device. **Filter envelope** selects the separate modulated tone filter described below.

![Eight-band EQ](images/drum-eight-band-eq.png)

Clean low-pass, high-pass, band-pass and notch filters offer 12/24 dB slopes, cutoff, resonance (Q), drive, filter ADSR, bipolar envelope amount and velocity sensitivity. The sample-gain stage precedes the filter; the amplitude envelope and instrument mixer follow it. Filter/envelope amounts are in octaves. This is Drum's filter implementation, not an emulation of Ableton's licensed circuit models.

![Draggable filter envelope](images/drum-filter-envelope.png)

## LFO

Sine, triangle, square, rising/falling saw and random waveforms modulate volume, pitch, pan and filter cutoff. Choose a free rate (0.01–30 Hz) or host-tempo divisions from 1/64 to four bars. Fade-in time, note retrigger and phase are available. Each voice has its own LFO. Free-running mode starts from an elapsed-time phase; it is not locked to the DAW's bar position. Standalone falls back to 120 BPM.

To hear modulation and see it in Edited, choose **LFO: On** and raise at least one depth. Filter depth also requires Filter On. A slow LFO or a long fade-in can make little visible difference to a short hit; time zoom helps inspect subtle changes.

## Attack layer

Turn **Attack layer** On to add a centred transient at the start of each sample hit:

- **Click:** a short tonal click with an extra harmonic for a sharper front edge.
- **Pulse:** a short sine burst; lower tone adds a knock, higher tone adds a tick.
- **Noise:** a repeatable filtered noise burst for a rougher attack.
- **Attack level:** -60 dB (silent) to 0 dB, default -18 dB.
- **Attack tone:** 30 Hz–16 kHz, controlling oscillator pitch or noise brightness. High settings are limited for the current host sample rate.
- **Attack decay:** 1–100 ms, default 8 ms, controlling the transient's maximum duration.

Start with Click, -18 dB, 1.8 kHz and 8 ms, then audition and adjust. The layer is added after sample gain/tone filtering, then passes through the eight-band EQ before the amplitude envelope, velocity response, LFO volume, instrument volume/pan, mute/solo, choke and sends. A long amplitude attack or Fade in softens it too. It starts once per note, does not repeat at loop boundaries, and ends if the sample voice ends. It requires a loaded sample. It does not follow sample tuning or pitch modulation; Attack tone controls its pitch separately. The sample and layer can sum above full scale, so adjust their levels as needed.

Edited and Send waveforms include the attack layer; Source does not. Zoom into the first few milliseconds to inspect its shape. These controls are saved per instrument and inherited by copies/slices. Reset page restores Off and the default settings.

## Outputs and sends

Route each instrument to Main or Out 2–16. Send A–F provide six additional stereo outputs, with per-instrument post-fader send levels; -60 dB is off. They contain audio for effects on DAW tracks, not internal reverb/delay engines. A disabled instrument output falls back to Main. A disabled send stays silent. On hosts that enable every output, selecting an auxiliary output removes that instrument from Main even if no receiving track is connected.

In Live, create a receiving audio track, select the Drum MIDI track as **Audio From**, and select the required plug-in output in the second chooser. Set monitoring to In and put the desired audio effects on that track. Route its audio to Main. Host output labels can include channel numbers as well as Drum's bus names. Validate routing before recording; organisational groups do not automatically create audio buses.

### Ableton effects on individual instruments

Live's native devices, such as EQ Eight and Compressor, run in Live's device chains and cannot be dragged into Drum's VST3. Assign the kick to Out 2, the snare to Out 3, and so on, then put the desired Live effects on their receiving audio tracks. All instruments sharing an output share that external effect chain. Main plus Out 2–16 provide 16 destinations; this is not 88 independent effect chains. Six sends can feed shared external effects. Live saves those devices and their routing in the Set; Drum saves its output/send choices. External effects are after Drum's waveform preview and are not drawn in it. See [Ableton's routing manual](https://www.ableton.com/en/manual/routing-and-i-o/).

Third-party VST3 effects shown in Live's browser are a different format from native Live devices. Hosting them inside Drum would require a separate effects-hosting feature, which this build does not provide.

## Macros

Sixteen stable VST3 parameters, **Macro 1–16**, are exposed to host automation. Each instrument can map one target to each macro, with independent Min/Max values. One macro can therefore control multiple instruments across the active kit. Reversed ranges are allowed. If several macros map to the same target, the highest numbered mapping wins. An unmapped macro changes no sound.

Targets include the instrument mixer, sample gain, cutoff/resonance, envelope times/amounts, LFO rate/depth and sends. Mapping a parameter overrides its base control value; removing the mapping restores that base value. Macro values belong to the plug-in instance, while assignments belong to each instrument and travel with copies/kit recall. In Live, expose the desired Macro parameters through the plug-in device's Configure controls and automate them in the usual way. Direct automation lanes for every per-instrument editing field are not supplied in this preview.

## Compatibility and remaining scope

Schema 9 adds weighted curves; earlier envelope states retain their previous curvature. Schema 8 adds eight EQ bands, bypass and output gain, with EQ off for older states. Schema 7 adds custom point/curve envelopes and manual key labels. Schema 6 and earlier preserve their ADSR sound without custom points; key analysis defaults to Auto. Current state saves every editing field, macro mapping and macro value. Schema 5 preserves all its existing editing settings and defaults to Stereo with attack Off. Schemas 1–4 load with neutral editing defaults. Invalid or incomplete new state is rejected without replacing the current kit. Editing does not embed or collect audio files.

The reference is the [Ableton Racks manual](https://www.ableton.com/en/live-manual/12/instrument-drum-and-effect-racks/) and [Simpler documentation](https://www.ableton.com/en/live-manual/12/live-instrument-reference/#simpler). Complete parity remains broader: Warp/time-stretch, transient/manual/beat slicing, key tracking, glide/spread, envelope looping, filter circuit models, layered chains, hosting arbitrary devices, built-in effect returns, macro names/variations and Live preset import are not implemented. Drum currently uses one audio sample per instrument and unique MIDI notes 21–108.

## Compact editing and level warnings (0.10.2)

Every waveform includes adaptive time graticules. The Edit Sound panel uses more of the window, single-line fields and graphs below the waveform. Envelope time and waveform time are separate: MIDI defines the duration of the S hold.

Sample gain turns red if the selected source region plus normalisation/gain exceeds 0 dBFS before mono/filter/envelope/fader processing. This is a headroom warning, not a hard clip at the gain stage. Edited/Send captions also turn red when their rendered peak reaches full scale. Fade in is disabled in Classic, whose amplitude graph controls the attack; in Trigger/Gate, fade edits select Edited and visibly shape the hit.
