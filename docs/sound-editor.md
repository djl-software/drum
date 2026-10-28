# Drum sound editor (0.5)

Select an instrument in the kit workspace and click **Edit sound**. The editor has six pages; scroll within a page to reach additional controls. **Back to kit** returns to the instrument cards. Kit switching remains available above the editor. All editing is non-destructive and saved in the DAW project; sample files remain external.

## Sample

- **Start / End:** trim the playable region, as percentages of the original file. Drag either green waveform marker or enter a numeric percentage. Ctrl + wheel zooms the overview, Shift + wheel scrolls it. The overview uses 512 peak bins, so it is not a sample-by-sample waveform display.
- **Sample gain:** -24 to +24 dB before the filter. **Normalise** compensates for the file's peak without rewriting it. Silent files remain silent.
- **Direction:** forward or reverse within the selected region. **Zero-crossing snap** searches up to 256 source frames for a nearby crossing in the left channel; markers retain the requested percentages while playback uses the snapped boundaries.
- **Trigger:** plays through note-off. **Gate:** fades out when the assigned MIDI note is released. Both use Fade in / Fade out. **Classic ADSR:** uses the amplitude envelope and can sustain a loop while the note is held. Unlike Simpler's strictly monophonic One-Shot mode, Drum retains configurable polyphony in Trigger mode for existing kits.
- **Voices:** 1–128 per instrument, within the global 128-voice pool. **Retrigger** cuts previous hits of the same instrument with a short fade. Choke groups remain independent of this setting.
- **Loop:** available in Classic mode. Loop start/end are relative to the trimmed region. Loop crossfade blends the boundary and is limited to half the loop length. Forward and reverse loops release on MIDI note-off. Manual Play uses a one-second gate for Gate/Classic, preventing indefinite preview loops; Stop is immediate.
- **Fade in/out:** milliseconds. A minimum two-millisecond end ramp remains to suppress discontinuities.
- **Slice to new group:** creates 2, 4, 8, 16, 32 or 64 equal regions on free MIDI keys, within the 88-instrument and 16-group limits. The original instrument remains intact. Slices share the source file and inherit controls, with Trigger mode and looping disabled. This operation rejects the entire change if capacity is insufficient.

Trim/loop markers, playback mode, direction and loop on/off are latched for each new hit. Other control edits can affect sounding voices. Reset page restores that page's defaults.

## Envelopes and pitch

Classic mode exposes amplitude Attack, Decay, Sustain and Release. The pitch envelope has its own ADSR and a bipolar semitone amount. Fine tuning, velocity-to-volume, velocity-to-pitch, random pan and MIDI pitch-bend range are also available. Pitch bend is per MIDI channel and defaults to zero range so old projects are unchanged. Pitch controls alter playback speed and duration.

## Filter

Clean low-pass, high-pass, band-pass and notch filters offer 12/24 dB slopes, cutoff, resonance (Q), drive, filter ADSR, bipolar envelope amount and velocity sensitivity. The sample-gain stage precedes the filter; the amplitude envelope and instrument mixer follow it. Filter/envelope amounts are in octaves. This is Drum's filter implementation, not an emulation of Ableton's licensed circuit models.

## LFO

Sine, triangle, square, rising/falling saw and random waveforms modulate volume, pitch, pan and filter cutoff. Choose a free rate (0.01–30 Hz) or host-tempo divisions from 1/64 to four bars. Fade-in time, note retrigger and phase are available. Each voice has its own LFO. Free-running mode starts from an elapsed-time phase; it is not locked to the DAW's bar position. Standalone falls back to 120 BPM.

## Outputs and sends

Route each instrument to Main or Out 2–16. Send A–F provide six additional stereo outputs, with per-instrument post-fader send levels; -60 dB is off. They contain audio for effects on DAW tracks, not internal reverb/delay engines. A disabled instrument output falls back to Main. A disabled send stays silent. On hosts that enable every output, selecting an auxiliary output removes that instrument from Main even if no receiving track is connected.

In Live, create a receiving audio track, select the Drum MIDI track as **Audio From**, and select the required plug-in output in the second chooser. Set monitoring to In and put the desired audio effects on that track. Route its audio to Main. Host output labels can include channel numbers as well as Drum's bus names. Validate routing before recording; organisational groups do not automatically create audio buses.

## Macros

Sixteen stable VST3 parameters, **Macro 1–16**, are exposed to host automation. Each instrument can map one target to each macro, with independent Min/Max values. One macro can therefore control multiple instruments across the active kit. Reversed ranges are allowed. If several macros map to the same target, the highest numbered mapping wins. An unmapped macro changes no sound.

Targets include the instrument mixer, sample gain, cutoff/resonance, envelope times/amounts, LFO rate/depth and sends. Mapping a parameter overrides its base control value; removing the mapping restores that base value. Macro values belong to the plug-in instance, while assignments belong to each instrument and travel with copies/kit recall. In Live, expose the desired Macro parameters through the plug-in device's Configure controls and automate them in the usual way. Direct automation lanes for every per-instrument editing field are not supplied in this preview.

## Compatibility and remaining scope

Schema 5 saves every editing field, macro mapping and macro value. Schemas 1–4 load with neutral editing defaults. Invalid or incomplete new state is rejected without replacing the current kit. Editing does not embed or collect audio files.

The reference is the [Ableton Racks manual](https://www.ableton.com/en/live-manual/12/instrument-drum-and-effect-racks/) and [Simpler documentation](https://www.ableton.com/en/live-manual/12/live-instrument-reference/#simpler). Complete parity remains broader: Warp/time-stretch, transient/manual/beat slicing, key tracking, glide/spread, envelope looping, filter circuit models, layered chains, hosting arbitrary devices, built-in effect returns, macro names/variations and Live preset import are not implemented. Drum currently uses one audio sample per instrument and unique MIDI notes 21–108.
