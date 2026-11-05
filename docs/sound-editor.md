# Drum sound editor

From 0.14.0, sound editing is available in both the MIDI track and shared kit library. Track edits stay in the Live Set; library edits are shared when saved with **Save library**. See [MIDI tracks and the shared library](library-kit-workflow.md).

Select an instrument in the kit workspace and click **Edit sound**. The editor has eight pages; scroll within a page to reach additional controls. **Back to kit** returns to the instrument cards. Use Back to kit before switching kits. All editing is non-destructive and saved in the DAW project; sample files remain external.

## Waveform on every page

The waveform stays above the scrolling controls on all eight tabs. The view selector offers:

- **Edited** (default): one hit rendered through Drum's current trim/reverse, gain/normalise, tuning, envelopes, tone filter, eight-band EQ, LFO, instrument mixer and macro settings. It updates after control edits, host macro changes and host tempo changes, without playing audio. At Y 1x its amplitude scale is Â±1 (0 dBFS). Vertical zoom magnifies the display without changing gain; values beyond the chosen scale meet the display edge. The header reports the actual signal peak in dBFS. This is display clipping only.
- **Source:** unprocessed audio displayed in the selected playback direction, with Start/End and loop markers. Reverse flips the display; START is the audible beginning of reverse playback. Choose this to drag trim markers. This view scales to the source peak and does not show processing. Numeric Start/End controls also work while Edited is selected.
- **Send Aâ€“F:** the selected post-fader send signal, including its send level and macro mappings, before shared return effects. An off send shows a flat line. These previews show the signal independently of whether its host output bus is enabled.

The preview uses the same voice DSP as playback, at the host sample rate and tempo, with 80% velocity and a one-second gate, matching manual Play. Trigger ignores note-off; Gate and Envelope release after one second. It shows the instrument before destination routing and external DAW effects. Mute/solo suppression is reflected; polyphony, retrigger and choke need overlapping notes to show a difference. Pitch bend is centred. Random modulation uses a repeatable example and a free-running LFO starts at phase zero, so a live performance can differ.

Preview work runs in the background. **Updating preview...** marks a pending result; obsolete slider renders are cancelled. Zoom and scroll stay in place across tab/control changes, and Fit X restores the whole current waveform in time. A new instrument/sample or Source/Edited/Send selection resets horizontal zoom. Long sounds are limited to the first 30 seconds or 2.88 million stereo frames (whichever is shorter); the header explicitly labels a truncated preview. The source waveform and audible sample are not shortened.

### Waveform size and channels

- **Y zoom:** choose 0.25xâ€“128x, or use **Shift + wheel** over the waveform. Choose Y 1x to reset. The setting stays fixed across tabs and sound edits, so level changes remain visible.
- **Fit Y:** fits the visible time range and displayed channel(s) once, choosing the largest available magnification that fits within the lane (subject to the zoom limits). Zoom into a quiet tail, then click Fit Y to inspect it. Silence resets to Y 1x. This does not enable continuous auto-scaling.
- **Height divider:** drag the grip immediately below the waveform to make the panel taller or shorter; double-click to restore its default height. The panel starts at 340 logical pixels where space permits and reserves room for the controls on the selected page.
- **Auto channels:** mono and exactly identical left/right signals use one full-height lane marked MONO. This includes a centred mono sample whose edited output has been duplicated to stereo. Once processing makes the channels differ, Auto shows separate L/R lanes.
- **Stereo L / R:** shows both channels for a stereo buffer, including duplicated mono. **Left only / Right only:** gives the selected channel the full height without summing or changing the audio. A mono source always shows its one available channel.

These are editor display preferences. They persist while the editor is open and are not saved as instrument controls. Source audio, stereo information, playback gain, pan and DAW state are unaffected.

## Sample

- **Start / End:** trim the playable region, as percentages of the full file measured in the chosen playback direction. In Reverse, Start measures from the original file's end. Switching direction retains the same source region; the displayed percentages mirror. Loop percentages also follow playback order within that region. Saved coordinates and source files are unchanged. Choose Source to drag either green marker, or enter a numeric percentage in either view. The waveform preserves signed peaks at the current display resolution. Close zoom draws the individual samples as a continuous trace. **+ / -** zoom around the centre; **Ctrl + wheel** zooms at the pointer; ordinary wheel or horizontal scrolling pans; **Fit X** shows the entire waveform in time. The footer shows horizontal zoom and the visible duration in milliseconds. Zoom can reach a 32-sample window.
- **Sample gain:** -24 to +24 dB before the filter. **Normalise** compensates for the file's peak without rewriting it. Silent files remain silent.
- **Source channels:** Stereo preserves the file's channels. Mono averages left and right before the filter; a mono file retains its level. Instrument pan and pan modulation still follow this stage. Opposite-polarity stereo material may cancel when summed. This is a playback setting saved per instrument, distinct from the waveform's display-only channel selector. Changing a sounding voice crossfades over 5 ms. Normalise continues to use the original file peak. Edited shows the result; Source still shows the original channels.
- **Direction:** forward or reverse within the selected region. **Zero-crossing snap** searches up to 256 source frames for a nearby crossing in the left channel; markers retain the requested percentages while playback uses the snapped boundaries.
- **Trigger:** plays through note-off. **Gate:** fades out when the assigned MIDI note is released. Both use Fade in / Fade out. **Envelope:** uses the amplitude envelope and can sustain a loop while the note is held. Unlike Simpler's strictly monophonic One-Shot mode, Drum retains configurable polyphony in Trigger mode for existing kits.
- **Voices:** 1â€“128 per instrument, within the global 128-voice pool. **Retrigger** cuts previous hits of the same instrument with a short fade. Choke groups remain independent of this setting.
- **Loop:** available in Envelope mode. Loop start/end are relative to the trimmed region. Loop crossfade blends the boundary and is limited to half the loop length. Forward and reverse loops release on MIDI note-off. Manual Play uses a one-second gate for Gate/Envelope, preventing indefinite preview loops; Stop is immediate.
- **Fade in/out:** milliseconds. Trigger/Gate use these fades. Envelope mode uses amplitude Attack and Release for the MIDI gate; Fade out also smooths the physical sample end in every mode, with a minimum two-millisecond end ramp to suppress discontinuities. The attack layer has its own independent Decay.
- **Slice to new group:** creates 2, 4, 8, 16, 32 or 64 equal regions on free MIDI keys, within the 88-instrument and 16-group limits. The original instrument remains intact. Slices share the source file and inherit controls, with Trigger mode and looping disabled. This operation rejects the entire change if capacity is insufficient.

Trim/loop markers, playback mode, direction and loop on/off are latched for each new hit. Other control edits can affect sounding voices. Reset page restores that page's defaults.

## Envelopes and pitch

Envelope mode (previously Classic ADSR) exposes amplitude Attack, Decay, Sustain and Release. The pitch envelope has its own ADSR and a bipolar semitone amount. Fine tuning, velocity-to-volume, velocity-to-pitch, random pan and MIDI pitch-bend range are also available. Pitch bend is per MIDI channel and defaults to zero range so old projects are unchanged. Pitch controls alter playback speed and duration.

The Edited waveform reflects these controls when active. Amplitude ADSR needs **Playback: Envelope**; Trigger and Gate use Fade in/out instead. The pitch envelope needs a nonzero **Pitch envelope** amount. Editing its ADSR with amount zero leaves the sound unchanged. Source always shows the unprocessed file.

### Shape envelopes with points and curves

Amplitude and pitch graphs sit side by side below the waveform; Filter > Filter envelope has its own graph. Click and release on a line to add a point without changing its existing shape. Drag a point in both axes to set time and level; points stay in time order and within 0â€“100%. Double-click a point to remove it (minimum two, maximum 32). Drag a line with no minimum hold time to curve that segment around the grabbed position; horizontal movement shifts the bend placement.

Right-click a non-final point to mark **S**, the held level. The points before S run from note-on; points after S describe release time after note-off. A short MIDI note releases from the actual current level. There is no fixed sustain duration in the graph. A nonzero final level fades briefly to zero. New graph and numeric timing edits are limited to the **entire original sample duration**, independent of trim or tuning. Existing longer settings are preserved on load and brought within that limit when edited. Without a loaded sample the fallback limit is 80 seconds. Numeric time edits retain the chosen stage and proportionally fit other time stages into the remaining duration; defaults also obey the limit. MIDI hold time remains independent.

**ADSR values** reveals the optional numeric stage sliders. **ADSR** removes the custom shape and restores the stored numeric ADSR. Numeric stage sliders are disabled while a custom shape is active. Reset page clears custom shapes on that page. The curve changes audible DSP and is saved per instrument, including copies and slices.

Graphs have separate **- / + / Fit** time controls; wheel or horizontal scrolling pans, **Ctrl + wheel** zooms around the pointer, and middle-button dragging pans. Fit resumes automatic fitting; scale stays fixed during a gesture. Focus a point and use arrows for 1 ms / 1% changes, Shift for 0.1 ms / 0.1%, or Delete/Backspace to remove. Overlapping points have offset grips connected to the actual curve.

Audible amplitude graph edits switch to Envelope automatically. Audible filter graph edits enable Filter and set -2 octaves when the amount is zero and not mapped. A click/release that only inserts a point preserves the current sound. Pitch/filter amount sliders are now visible in the graph headers. Moving, bending or deleting a pitch point with amount zero enables +12 st unless a macro owns the amount; adding a point alone preserves silence. Audible graph edits select **Edited**, so their effect is immediately visible. Numeric ADSR changes at zero pitch amount still leave the sound unchanged.

When a macro controls amplitude timing, the effective curve follows its value and the whole graph locks point editing. Click **Unlink timing** on the graph to keep the currently displayed timing and remove only its timing macro assignments, then edit the points. Other macro mappings and values are preserved. Removing a mapping from the Macros page instead restores the original base value. Amount mappings lock the graph's amount slider. Reset page leaves macro mappings intact.

Each graph has **Reset / Undo / Redo**. Reset restores its shape, numeric ADSR and modulation amount to defaults (while retaining playback/filter enable state); reset is disabled for macro-controlled timing or amount. Undo/Redo retains up to 64 edits per envelope for the currently selected instrument. A whole point/curve/slider drag is one entry; view navigation does not enter history. New edits clear redo. Changing instrument/sample, closing the editor or externally recalling different envelope settings clears history. Undo can restore older settings beyond the current sample-length limit.

Double-click a slider's **numeric box**, including an active text editor, to restore its default. Single-click still lets you type. This covers mixer, Edit Sound, envelope amounts, EQ and macro values/ranges. EQ frequency uses the selected band's default; macro values use 50%, and mapping min/max use the selected target's range.

### Trigger, pitch and Camelot

The header separately labels the assigned MIDI trigger, estimated processed-audio pitch (note/Hz or a varying range), and musical key/Camelot. Background analysis is bounded and can report uncertainty. A stable single drum tone cannot establish major/minor; noisy and short hits may have no reliable pitch.

**Key label (manual override)** selects a known major/minor key, explicitly labelled manual. Auto detect returns to analysis. Manual labels are metadata and do not transpose automatically when tuning changes. Source view still analyses the processed preview; Send views analyse the chosen send. External Live effects are excluded. Examples: A minor = 8A, C major = 8B, B-flat minor = 3A; see the [official Camelot chart](https://mixedinkey.com/camelot-wheel/).

## Filter

**8-band EQ** provides eight independently enabled stereo bands with low cuts (12/48 dB/oct), low shelf, bell, notch, high shelf and high cuts (12/48 dB/oct). Drag a dot horizontally for frequency, vertically for gain (Q for cuts/notch). Alt-drag or wheel changes Q; double-click a dot toggles its band. Numeric frequency (20 Hzâ€“20 kHz), gain (Â±24 dB), Q (0.1â€“18) and output gain (Â±24 dB) are also available. Dragging enables the band and EQ. The combined response curve uses the same coefficients as playback; Edited includes the EQ. The same EQ settings process the sample and attack in separate paths before their independent envelopes, then they are mixed.

The EQ uses one linked stereo curve. Spectrum analysis, independent L/R or M/S, band audition and adaptive Q are not included. It takes interaction cues from [EQ Eight](https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/#eq-eight); it does not load or emulate the native device. **Filter envelope** selects the separate modulated tone filter described below.

![Eight-band EQ](images/drum-eight-band-eq.png)

Clean low-pass, high-pass, band-pass and notch filters offer 12/24 dB slopes, cutoff, resonance (Q), drive, filter ADSR, bipolar envelope amount and velocity sensitivity. The sample-gain stage precedes the filter; the amplitude envelope and instrument mixer follow it. Filter/envelope amounts are in octaves. This is Drum's filter implementation, not an emulation of Ableton's licensed circuit models.

![Draggable filter envelope](images/drum-filter-envelope.png)

## LFO

Sine, triangle, square, rising/falling saw and random waveforms modulate volume, pitch, pan and filter cutoff. Choose a free rate (0.01â€“30 Hz) or host-tempo divisions from 1/64 to four bars. Fade-in time, note retrigger and phase are available. Each voice has its own LFO. Free-running mode starts from an elapsed-time phase; it is not locked to the DAW's bar position. Standalone falls back to 120 BPM.

To hear modulation and see it in Edited, choose **LFO: On** and raise at least one depth. Filter depth also requires Filter On. A slow LFO or a long fade-in can make little visible difference to a short hit; time zoom helps inspect subtle changes.

## Attack

Tick **Attack** to add a centred transient independently of the sample envelopes:

- **Click:** a short tonal click with an extra harmonic for a sharper front edge.
- **Pulse:** a short sine burst; lower tone adds a knock, higher tone adds a tick.
- **Noise:** a repeatable filtered noise burst for a rougher attack.
- **Attack level:** -60 dB (silent) to 0 dB, default -18 dB. The shaped Click/Pulse/Noise burst is peak-calibrated: 0 dB reaches full scale before EQ, velocity response, LFO and instrument volume/pan; -6 dB is about half amplitude. At the 80% velocity used by Play/Edited, an isolated 0 dB layer peaks around -1.9 dBFS with otherwise neutral controls. Sample plus layer can exceed full scale. Existing active layers become louder at the same stored dB value because the control is now calibrated.
- **Attack tone:** 30 Hzâ€“16 kHz, controlling oscillator pitch or noise brightness. High settings are limited for the current host sample rate.
- **Attack decay:** 1â€“100 ms, default 8 ms, controlling the transient's maximum duration.
- **Position:** 0â€“2,000 ms after MIDI note-on, default 0. Position is latched per note; it does not move an already scheduled hit.

Start with Click, -18 dB, 1.8 kHz, 8 ms and Position 0 ms. Try 5â€“15 ms for a later knock, or 50â€“150 ms for a distinct second hit. The layer bypasses sample gain, filter/drive, amplitude/pitch/filter envelopes and sample fades. It uses its own decay window. The instrument EQ applies to both paths; velocity response, LFO volume, instrument volume/pan and sends apply after they are mixed. Sample tuning and pitch modulation do not change Attack tone or Position.

The layer fires once per note, even if the sample or amplitude envelope has already ended, and does not repeat at loop boundaries. A loaded sample is required. Normal note-off leaves the independent transient scheduled; Stop, mute, choke, retrigger and voice stealing still cancel it. Both signals can sum above full scale, so adjust their levels as needed.

Edited and Send waveforms include the attack layer; Source does not. Zoom into the first few milliseconds to inspect its shape. These controls are saved per instrument and inherited by copies/slices. Reset page restores Off and the default settings.

## Chain: per-instrument VST3 effects

Open **Chain** and drop a `.vst3` audio-effect file/bundle from Explorer into the full-width row. Alternatively use **Load VST3...** to choose an installed module. Drum no longer has an embedded plugin list. On Windows Live, start a VST3 audio-effect drag with Chain visible and release over the named drop target. The bundled browser bridge resolves the source row against Live's installed plugin database; ambiguous paths offer a chooser. No Control Surface is needed. Explorer and Load VST3 remain fallbacks when a Live version or browser layout cannot be identified. Native Live devices and instrument plugins cannot load here.

Audio runs left to right through up to 16 effects, after the built-in sample/attack processing and instrument volume/pan, before output and send routing. Empty, fully bypassed or unavailable chains pass through. **Open** displays the effect's own editor, **Bypass** skips it, **Remove** deletes it, and dragging its header or pressing **< / >** changes order. EQ before compression changes what drives the compressor; EQ after compression shapes its result. Compare both at matched levels.

The DAW project stores order, bypass and effect state. Copies get independent instances. Only the active kit's plugins are loaded; kit switches capture the departing settings and replace live effects. Missing plugins retain their saved slots/state and pass audio through; restore the module at its saved path and choose **Retry loading**. Plugin binaries, licences and samples are external.

Stereo and mono audio effects are supported; required sidechains, MIDI-effect input, multichannel-only effects and direct effect-parameter automation/macro mappings are not. Stereo is downmixed/duplicated when an effect only supports mono. All active-kit paths are aligned to the longest effect-chain delay and that latency is reported to the host. Effect tails continue after a source voice ends. Effects run in-process without crash isolation. Saved state is limited to 16 MiB per effect and 192 MiB of encoded effect data across all kits; exceeding either reports a save error and retains the previous saved settings.

**Waveforms and tonal analysis stop before Chain**, including Send previews and sample-gain warnings. Play/MIDI and actual sends include the effects. Use the effect's meters and listen to its output. All MIDI remains on the single Drum track; no receiving audio tracks are needed for these internal chains.

## Outputs and sends

Sends A-F are six local shared effects returns per kit. Each instrument sends a post-fader, post-pan, post-Chain copy into a shared return. All instruments feeding the same letter are summed before its effects, then mixed into Drum Main. The dry Main/Out 2-16 destination stays independent.

Use Edit A-F in the routing diagram to add built-in or VST3 effects, open their editors, reorder, bypass or remove them. Each return supports up to 16 effects and saves with the kit and DAW session. For shared ambience, send both Woods to A, add Room to A, and set Room Mix to 100%. Empty or bypassed return chains pass the send through, adding another dry copy to Main.

Send controls range from minus infinity (displayed as the infinity sign) to 0 dB. The bottom position produces exact silence. Double-click resets to silence; typing -inf also turns a send off. Macro send targets use the same range. Legacy -60 dB off settings migrate to silence.

The diagram shows edited sound -> instrument Chain -> dry destination and six parallel sends -> return effects -> Main sum -> DAW. Lit wires show enabled routes, respecting mute/solo and macro send settings. A return-to-Main connection can stay lit when another instrument feeds that return. This indicates routing rather than instantaneous audio metering. Disabled auxiliary dry destinations fall back to Main.

No receiving DAW tracks or enabled auxiliary send outputs are needed. Old host send bus positions remain as silent placeholders to preserve saved output ordering. Main and Out 2-16 remain available for external processing. Native Live effects require external DAW routing; built-in Drum effects and VST3 audio effects can run in local return chains.

## Macros

Sixteen stable VST3 parameters, **Macro 1â€“16**, are exposed to host automation. Each instrument can map one target to each macro, with independent Min/Max values. One macro can therefore control multiple instruments across the active kit. Reversed ranges are allowed. If several macros map to the same target, the highest numbered mapping wins. An unmapped macro changes no sound.

Targets include the instrument mixer, sample gain, cutoff/resonance, envelope times/amounts, LFO rate/depth and sends. Mapping a parameter overrides its base control value; removing the mapping restores that base value. Macro values belong to the plug-in instance, while assignments belong to each instrument and travel with copies/kit recall. In Live, expose the desired Macro parameters through the plug-in device's Configure controls and automate them in the usual way. Direct automation lanes for every per-instrument editing field are not supplied in this preview.

## Compatibility and remaining scope

Schema 11 adds ordered VST3 effect slots, bypass and opaque plugin state; older projects receive empty chains. Schema 10 adds attack Position, defaulting to 0 ms for older projects. The attack now bypasses the sample envelopes for all projects, including existing Attack On presets. Schema 9 adds weighted curves; earlier envelope states retain their previous curvature. Schema 8 adds eight EQ bands, bypass and output gain, with EQ off for older states. Schema 7 adds custom point/curve envelopes and manual key labels. Schema 6 and earlier preserve their ADSR sound without custom points; key analysis defaults to Auto. Current state saves every editing field, macro mapping and macro value. Schema 5 preserves all its existing editing settings and defaults to Stereo with attack Off. Schemas 1â€“4 load with neutral editing defaults. Invalid or incomplete new state is rejected without replacing the current kit. Editing does not embed or collect audio files.

The reference is the [Ableton Racks manual](https://www.ableton.com/en/live-manual/12/instrument-drum-and-effect-racks/) and [Simpler documentation](https://www.ableton.com/en/live-manual/12/live-instrument-reference/#simpler). Complete parity remains broader: Warp/time-stretch, transient/manual/beat slicing, key tracking, glide/spread, envelope looping, filter circuit models, layered instrument chains, native Live device hosting, macro names/variations and Live preset import are not implemented. Drum currently uses one audio sample per instrument and unique MIDI notes 21â€“108.

## Compact editing and level warnings (0.12.1)

Every waveform includes adaptive time graticules. The Edit Sound panel uses more of the window, single-line fields and graphs below the waveform. Envelope time and waveform time are separate: MIDI defines the duration of the S hold.

Sample gain turns red only when the completed processed preview exceeds 0 dBFS. Exactly full scale, a high gain setting alone and display zoom do not cause a red warning. The preview includes the amplitude envelope, EQ, attack layer and mixer level, at 80% velocity and a one-second gate; Send views use the selected send. Pending/stale previews clear the warning until a current result arrives. Edited/Send captions use the same strict limit. This is one rendered hit, not a live meter of overlapping notes, Chain effects or external effects. Floating-point audio is retained rather than hard-clipped. Fade in is disabled in Envelope, whose amplitude graph shapes the sample; Trigger/Gate use fades. Neither affects the independent attack layer.

## Shared audition transport (0.12.3)

Play, Loop and BPM now live in the workspace header, visible at the same location above every editing tab. Play triggers one audition with Loop off. Loop latches and highlights; Play starts repeating at one hit per beat and becomes Pause. BPM ranges from 20 to 999 and follows the host by default. Editing BPM overrides it until the editor is reopened. Sample-region looping remains separate. Pause fades audition voices without cancelling MIDI voices. Instrument/kit changes and editor closure stop audition repetition. Loop/BPM are editor audition settings, not saved musical parameters.

### Compact effect panels

Each Chain card keeps Open, Bypass, Remove and order arrows. Effects supporting a stereo sidechain show No Input/source, Gain (0 dB), Mix (100%) and Mute source (off). Select another kit instrument as a pre-effect detector source. Gain and Mix affect the detector signal, not output level or wet/dry balance. Mute source removes its audible output while retaining the detector feed. Use Open for plugin-specific controls. Routing saves with the Set; external Live tracks are not available as sources. Drag Live effects into the fixed strip above the cards; this uses native code inside Drum, without a helper executable.

## Drum built-in instrument effects (0.13.0)

Each instrument has its own chain; this is not a drum-bus chain. Click **Add Drum effect** to add EQ, Compressor, Transient, Saturation, Clipper, Limiter, Gate or Utility. Built-ins and VST3 effects process left to right in the same chain. Drag a card header or use its arrows to change order. Open edits that instance, Bypass preserves its place and settings, and Remove deletes it. All settings save in the Set.

The chain EQ uses the same eight-band DSP and graphical editor as Edit Sound â†’ Filter, but has independent settings. The sound-editor EQ remains before the chain. Put a chain EQ before saturation to control what drives it, or after saturation to shape the harmonics it creates. EQ is optional and is not inserted by default.

Compressor has stereo-linked peak detection, threshold, ratio, soft knee, attack, release, makeup and wet/dry mix. Use it to control a hit's body without necessarily removing the attack. Transient uses fast and slow envelope followers; Attack changes the onset and Sustain changes the body, with Length controlling the slower follower. It does not rewrite the sample or its drawn envelope.

Saturation uses a soft nonlinear curve with Drive, Mix and Output. Clipper offers Drive, Ceiling, Softness and Output. Both use 2x oversampling, with their latency compensated by the chain graph. Clipping deliberately changes sharp peaks; saturation adds harmonics. Lower Output when comparing processed and original sound so loudness does not mask the tonal difference.

Limiter is a stereo-linked, instantaneous-attack sample-peak limiter with Input, Ceiling and Release. It is **not a true-peak or look-ahead mastering limiter**. Very short releases and heavy limiting can distort; use modest reduction on individual hits. Gate reduces audio below Threshold by Range, using Attack and Release to avoid abrupt changes. Utility provides gain, width (0% mono, 100% original), polarity inversion and channel selection (0 stereo, 1 left, 2 right, 3 swap).

Open panels display input/output peak and gain-reduction readings. Reset restores that effect's parameters; double-click a numeric control to restore its default. The waveform preview remains before chain processing, so the effect meters and listening are the checks for chain changes.

From 0.13.1, the factory chain for new instruments is empty. Existing saved instruments and personal saved defaults are retained. Under **Chain presets / defaults**, choose a factory preset, copy/paste a whole chain, save/load a `.drumchain` file, or save the current chain as the default for new instruments. Applying a preset or pasting replaces the selected instrument's chain; copying first preserves the previous chain for later paste. Kick punch and Snare snap are starting points, not automatic level matching. Sidechain source notes and source mute are reset when copying/applying presets so a new instrument cannot accidentally mute another source.

## Browser identity and unsupported devices

Live's browser exposes a name to Drum's drag reader, not a proven VST3 device identity. Browser drops load a single matching VST3 directly. Multiple matches or known native Live names show a chooser listing the installed vendor/name/path. Direct `.vst3` file drops and Load VST3 identify the file explicitly. Native Live devices and synth instruments cannot be inserted into this audio-effects chain; an unsupported browser drop displays an explanation. Drum does not change Live's browser filters.

The 30 October 2026 installed-plugin audit found one collision against known native Live audio-effect names: Ableton **Limiter** versus Mastering the Mix **LIMITER**. No other duplicate installed VST3 effect names were found after the installation cleanup. The chooser remains for known native names and multiple matches; unknown future native names may require updating the collision list.


## Graphs and output analysis (0.13.1)

Both EQ views have a solid spectrum shaded into the background, behind the EQ response curve and band handles. Play a hit or enable audition Loop to see it. The sound-editor spectrum shows the selected instrument after sound processing and before its chain; each chain EQ shows its own processed output. The spectrum uses a separate -90 to 0 dBFS scale, not the response curve's +/-24 dB scale. Cuts and boosts change the spectrum when audio is playing. Stereo power is combined without phase cancellation. The analyser is visual only: it does not normalise or alter the sound. FFT analysis runs at 15 updates per second on the UI thread while the EQ is shown.

Compact chain tiles leave more space for the waveform. Their two thin output meters show left/right sample peaks with a short decay; green shows levels below -12 dBFS, yellow from -12 to -3 dBFS, and red above -3 dBFS (headroom warning, not necessarily clipping). They reuse the existing audio validation pass and do not run FFTs. Bypassed tiles have no active output reading.

Open Compressor, Gate or Limiter to see an input/output transfer graph and a measured peak point. Drag in the graph to set the threshold (Limiter: ceiling). Saturation and Clipper show their transfer curves. Transient and Utility show recent input/output peaks. These graphs help explain where processing changes level; the meters and listening show the result on actual hits. Mix is the rightmost knob wherever available. Parameter identities and saved settings remain unchanged.


## Visual refinements (0.13.2)

The effect button and browser target are labelled **Add effect**. EQ spectra interpolate between FFT bins and average a narrow frequency neighbourhood to remove blocky steps; the solid fill fades into the background. Each EQ band has its own colour, with inactive bands dimmed. The response curve remains separate from the measured spectrum.

Saturation now shows its transfer curve alongside the **actual processed stereo output waveform**, blue for left and amber for right. Play or loop a hit to see how Drive, Output and Mix change the shape. The scope shows 512 samples (10.7 ms at 48 kHz), with a fixed +/-1 amplitude scale and a rising zero-crossing trigger. It is a live close-up rather than a preview of the whole sample. Capture stops when the panel is hidden. Transfer graphs use coloured shaded fills; the amber area in dynamics graphs marks the threshold/ceiling region. Repeated footer explanations have been removed from the panels; processing limitations remain documented here.


In 0.13.3 the separate Add effect drop strip is removed. The toolbar reads **Add effect**, **Load VST3...**, **Presets**, side by side. Drag VST3 effects directly onto the chain; the temporary full-window drag surface remains active during browser drags.

EQ spectra now use an 8,192-sample FFT (4,097 bins), giving about 5.9 Hz bin spacing at 48 kHz. Narrower display smoothing preserves more detail around EQ cuts and boosts. The analysis window covers about 171 ms at 48 kHz; the display still refreshes at 15 Hz.

Browser drops with one matching VST3 now load directly. Multiple matches and known native Live device names (such as Limiter) still open a chooser to avoid loading the wrong effect. Native Live devices are still unsupported inside Drum.

Browser shutdown now services synchronous UI/COM replies while its lookup worker exits, avoiding a circular wait during editor destruction. Clicks inside Drum do not start browser identity lookups. This addresses a shutdown deadlock path; host-close behaviour still needs verification in Live.

Implementation reference: [Microsoft UI Automation threading guidance](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-threading) and [COM-aware waits](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cowaitformultiplehandles).


## Contiguous keyboard order (0.13.4)

A kit is an ordered sound library mapped to consecutive keyboard keys. The first sound uses A-1 (MIDI 21), then A#-1, B-1, C0 and so on, up to C7 (MIDI 108) for all 88 sounds. Group order comes first; pad order within each group comes second. Both black and white keys count. Add a sound to an earlier group and later sounds shift up. Delete a sound and later sounds shift down. Moving/copying groups or sounds, loading samples as new instruments and slicing follow the same rule.

Use the instrument/group move controls to change order. The instrument details show the assigned key instead of allowing independent note assignments. Clicking a pad, playing its keyboard key and audition Play still trigger that sound. Sample pitch/tuning is unaffected by this reassignment.

Sidechain routing follows the source instrument as its key shifts; deleting its source sets No Input and clears source mute. Chains, samples, names and editing settings stay with the instrument. Existing libraries are compacted into their saved group/pad order on load (schema 12). During a layout change, old held/audition notes are cleared so an earlier key cannot continue playing a different sound. New key presses use the updated layout. External MIDI clips, if used, are not rewritten by Drum.

## Key tab (0.16.15)

The Key tab maps edited-preview spectral energy to twelve note classes, **C, C#, D, D#, E, F, F#, G, G#, A, A#, B**. Octaves are combined. Bars use relative power: the strongest note is 0 dB, with a -48 dB display floor. Energy between two notes is distributed between neighbouring columns, so fine detuning is visible. This is note-class energy, not a full frequency analyser or a claim that every harmonic is a separate played note.

Analysis averages multiple windows across the processed waveform (up to the 30-second preview limit), including any repeats rendered by sample looping. It runs on a cancellable background worker and updates when the sound changes. The note-energy spectrum combines channel power without cancelling inverted stereo signals; the existing pitch/key estimate uses the stronger channel. The spectrum and detected pitch/key describe the edited instrument before its effect chain. Very short, noisy or changing-pitch drums may not provide a stable note or major/minor key. Repetition alone cannot establish a key for an unpitched hit.

**Coarse** changes playback tuning by semitones (-48 to +48). **Fine** adjusts -100 to +100 cents; 100 cents equals one semitone. These are the same stored instrument-tuning and fine-detune controls used elsewhere, not additional offsets. They change sample playback speed and therefore duration, alongside pitch. Adjust Coarse to move the sound towards the desired note, then Fine for a smaller correction, listening while watching the spectrum. Reset page returns both to zero; double-click a numeric value to reset that control. Macro-assigned tuning controls are disabled here while mapped.

The waveform stays above the spectrum. The detected pitch and existing key/Camelot indicator remain separate from the assigned MIDI trigger note. Tuning changes are saved with the instrument in its kit or library.

### Tile keys and Auto Tune (0.16.16)

Samples are analysed on the loading worker for both kits and libraries. Tiles display a detected musical key and Camelot code, for example Am / 8A. Single pitched hits show their note with “key uncertain”; noise and ambiguous hits show “Key uncertain”. A single note cannot establish major/minor. Tile labels describe source analysis with coarse/fine tuning applied, before envelopes, modulation and effects; the Key tab analyses the edited preview. Analysis is cached with the decoded sample and recalculated when loaded again.

Choose a **Target key** in the Key tab and click **Auto Tune**. Drum chooses the nearest transposition to the target root and sets the existing Coarse/Fine values, including cents correction for a stable detected note. The waveform and spectrum update. This is sample transposition, not real-time pitch correction or major/minor conversion. A major phrase stays major when transposed. Unpitched sounds cannot be auto-tuned; tune these by ear. Pitched sweeps use their representative detected pitch, with the varying range still shown. Macro-controlled tuning is protected.

### Tune and playback cursor (0.16.17)

The Key tab is now **Tune**, immediately after Filter. Analysis excludes silent padding and uses shorter, more frequent pitch windows for drum hits. Auto Tune accepts a confidently detected representative pitch even when the hit sweeps in pitch. It transposes the entire hit; it does not flatten a pitch sweep. The tile continues to mark varying pitches.

Amber waveforms and blue/violet note spectra are visually distinct from the mint controls. A pink vertical line follows the newest sounding voice for the selected instrument, whether triggered by MIDI or preview. Source view follows the actual sample position, including trim, reverse and loop wraps; Edited view follows elapsed playback time on its rendered preview. Zoom and pan also apply to the cursor. The line disappears when the voice ends or falls outside the displayed preview range.

### Spectrum key estimates (0.16.18)

Auto Tune and its target selector have been removed. Use Coarse and Fine to tune manually. The displayed key is now the highest-power C–B spectrum column, rather than the pitch detector or major/minor classifier. Instrument tiles also show this spectral estimate, including noisy or varying hits, labelled “(est.)”. Tile estimates use source analysis shifted by manual tuning; the Tune page uses the edited preview. Silence has no measurable key and displays “No signal”. A dominant note alone does not establish major/minor or a Camelot code. Legacy manual key labels remain in saved data but no longer override the display.

### Sample layout (0.16.19)

Sample opens in Source view for trimming. Drag the blue Start/End vertical markers and flags on the waveform; their percentage positions are displayed beside them. Separate Start/End slider boxes are hidden. Reverse still measures positions from the reversed beginning. Choose Edited to preview processing, or switch to another edit tab.

The remaining Sample controls are arranged four per row, including slicing. The waveform fills the height remaining above those controls; compact windows keep the controls scrollable.

### Envelopes & pitch layout (0.16.20)

The waveform matches Sample's tall waveform. Amplitude and pitch graphs sit side by side below it, with compact toolbars. Five remaining controls share one narrow row beneath the graphs. Optional ADSR numeric controls scroll within that row instead of reducing graph height. This layout prioritises waveform height; the envelope graphs are correspondingly smaller.

### Envelope overlay (0.16.21)

In Envelopes & pitch, choose **Amplitude** or **Pitch** above the waveform. The selected envelope is drawn directly over the Edited waveform and can be edited using the existing point/curve gestures. Pink denotes amplitude; blue denotes pitch. Only the selected envelope receives mouse edits. Its reset, undo/redo, ADSR and pitch-depth controls sit beneath the waveform.

Envelope time positions match the waveform's milliseconds. Horizontal zoom and pan are shared; wheel pans, Ctrl+wheel zooms time, Shift+wheel changes waveform height, and middle-drag pans. Use the waveform's Fit X to return to the full preview. Selecting Source temporarily hides the envelope to allow source trimming; selecting an envelope button returns to Edited. The original-sample duration remains the editing limit.

### Mode-specific controls (0.16.22)

Amplitude shows Velocity to volume; Pitch shows Velocity to pitch and Pitch bend range. Random pan is common to both. Optional ADSR values also follow the selected envelope. The duplicate Fine tuning box has been removed here; Fine remains on Tune and still changes the same stored setting.

These numeric controls, including Pitch depth, use value boxes with a shaded level bar. Drag to adjust without jumping to the clicked position, scroll for increments, or click the number to type. Double-click resets the value to its default. Existing macro locks and custom-envelope ADSR protection still apply.

Filter and Tune now replace the waveform with their graphs at the same size as the Sample waveform. The Filter EQ response and output spectrum share a +24 to -60 dB display range.

EQ band gain now covers -60 to +24 dB, including chain EQ and saved state. Drum numeric sliders use shaded value bars (drag, scroll or type; double-click resets). Filter envelope changes the separate tone filter cutoff over time, rather than moving the eight EQ bands.

Envelopes contains Amplitude, Pitch and Filter selectors. Filter envelope and its tone-filter controls now live here; the Filter tab contains eight-band EQ. Tab order: Sample, Envelopes, Filter, Attack layer, LFO, Tune, Outputs & sends, Macros, Chain.

EQ now has 16 bands. Select a numbered band below the graph and use its on/off switch; disabled points are hidden. Shape selects low cut, shelf, bell, notch or high cut. Cut shapes offer slopes from 6 to 72 dB/oct in 6 dB steps (higher slopes remove unwanted frequencies more sharply). Frequency, Gain, Q and Output labels sit beside value bars. Reset EQ clears all bands and restores defaults. Existing eight-band kits and chain presets retain their first eight bands; additional bands start off.

Control labels are right-aligned beside their own value bars, with larger white text. EQ on is at the top-left inside the graph and bypasses the whole EQ; each checkbox below a band number switches only that band. The duplicate selected-band switch has been removed.

Kit and Library Groups share the same layout: bright heading, + at the top right, group count at the bottom, and Rename/Duplicate/Delete menus through the group arrow, right-click or double-click. Deleting a group requires confirmation.

Drum Link opens collapsed; expand it when needed. Unassigned keys show Live-style note names (MIDI 21 = A-1), replacing (unused). With Setup > Automatically update Ableton clips enabled and the Live companion connected, Session and Arrangement notes follow instrument IDs through reorder/insertion. Removing an instrument mutes its notes instead of playing a replacement; restoring that instrument ID restores their previous mute state. Changes during clip recording are deferred until recording stops. Enable this option before rearranging the kit.

Group boundaries: Setup has separate Kit and Library entries, applying to the active collection of that type. 0 keeps notes contiguous; 0.5 starts each following nonempty group at the next available C or F#; 1 starts it at C. The first group still begins at A-1 (MIDI 21). A boundary already reached is reused, not skipped. Empty groups use no keys. Gaps count against the MIDI 21–108 range; changes that exceed it are rejected. Saved collections retain their boundary, and older collections default to continuous. Live notes follow boundary changes only with a connected companion and automatic clip updates enabled.

The group-boundary menu labels are None, Half octave and Octave (formerly 0, 0.5 and 1). Setup > About Drum shows the version. The former Update toolbar action is now Setup > Reconnect to Ableton, for initial Configure linkage or retrying automatic rack setup. Normal connected editing needs no manual update.

## Tune key display and tile clicks

Open an instrument with **Edit**; double-clicking a tile does not open the editor. Tune shows the highest-power note prominently beside its major and minor Camelot references. For example, A shows 11B (major) / 8A (minor); a single note does not determine a major/minor scale. The readout follows Coarse and Fine immediately while the edited-audio analysis refreshes in the background.

Tile and Tune key labels share the edited-audio analysis, including tuning, envelopes and filters, before the effect chain. Changed settings invalidate the cached result; both use the same transposed source readout while updated analysis finishes. Tune always analyses the main instrument signal, regardless of the waveform send view.

EQ output analysers scan overlapping FFT windows and retain peaks across each screen update, including short hits followed by silence. Both Filter and chain EQ start displaying incoming audio before a full FFT window has accumulated. The shaded spectrum shows live playback output.

Envelope editing: click empty graph space to place a point and drag it immediately. Ctrl-click on a line also places a movable point; ordinary line drags shape curves. Shift-drag a point for finer movement. Each envelope Reset creates a flat neutral shape: full amplitude, zero pitch/filter modulation. Undo restores the previous settings.

Attack layer has an **Attack waveform only** checkbox that hides the sample from the rendered waveform without changing playback. Outputs & Sends replaces the waveform with the instrument-chain split into its main destination and six parallel sends; disabled host outputs are labelled. Macros uses the full editing area for macro mapping cards. Tune bars analyse the edited sample and update after sound edits, rather than scrolling continuously during repeated playback.

The final editing tabs are ordered Chain, Outputs & Sends, Macros. Audio passes through the chain before its output/send split. Macros control parameters rather than forming a serial audio stage.

LFO replaces the sample waveform with an editable single cycle, shown from 0% to 100%. Choose a standard waveform or draw points and curves to switch to Custom. Custom LFO shapes are stored with the instrument and repeat at the chosen free or tempo-synchronised rate. Graph Reset gives a flat centre line and turns the LFO off; Undo/Redo restores shape edits. Random displays an illustrative cycle because playback generates fresh random values.



Stereo Width is an optional instrument-chain effect. Delay sets a 0–30 ms Haas offset on the chosen Left/Right side. Protect bass keeps the low band centred below Bass below. Pan balances the output left/right; Mix is last on the right and defaults to zero, so adding the effect initially leaves the sound unchanged.

The live graph shows delay placement, channel correlation and the level change when folded to mono. Negative correlation and a large negative mono change indicate potential cancellation; these measurements are guides rather than a guarantee of mono compatibility. Mono check auditions this effect output in mono for this instrument, after width, mix and pan. It affects the audible instrument while enabled, resets when the effect editor closes, and is never saved in the kit or effect preset. Effects later in the chain can still alter stereo compatibility.

## Expanded chain effects (0.19.0)

All effects are optional additions to each instrument’s chain and process in tile order. Existing effect identifiers and parameters are retained. Old Compressor and Saturation presets load with their new controls inactive.

- **Dynamic EQ:** a focused dynamic bell, with Frequency and Q choosing the region, Threshold deciding when it reacts, signed Range setting maximum cut/boost, and Attack/Release setting response speed. Use a negative Range to reduce ringing or harshness only on strong hits. Range starts at zero; the static 16-band EQ remains available separately, and multiple Dynamic EQ tiles can target different regions. The graph shows output spectrum and current dynamic gain.
- **Bitcrusher:** Bits reduces amplitude resolution; Sample rate holds values between updates. Drive feeds the quantiser harder; Output sets the resulting level. Mix blends the degraded sound with the original. Useful for rough snares and vintage digital textures.
- **Delay:** Time sets free-running milliseconds. Sync and Division select host-tempo timing; dotted divisions are included. Feedback repeats echoes, Low/High cut darken or thin them, and Ping pong crosses feedback between channels. Mix starts dry; use 100% on a dedicated send path. Without a host tempo it uses 120 BPM. Long feedback settings can create long tails.
- **Ducker:** choose another instrument in the chain tile’s sidechain source selector. Threshold detects that source; Depth sets attenuation; Attack, Hold and Release shape recovery. Mix blends the ducked result. No external source means no ducking. For example, select the kick as source on a percussion instrument to leave space for kick hits. The source is tapped before its effects, as with other chain sidechains.
- **Room:** Predelay separates the hit from its ambience. Room size changes the reverb network’s decay; Damping softens high frequencies, and Low cut removes bass from the room feed. Share it on a send when several sounds should occupy the same space.
- **Resonator:** Frequency sets the ringing pitch, Decay sets its approximate 60 dB decay time, Damping filters the excitation, and Drive changes how strongly the hit excites it. Blend with Mix for metallic or pitched percussion.
- **Frequency Shifter:** Shift offsets spectral components by a fixed number of hertz, unlike semitone tuning which scales their frequencies. Positive/negative values shift up/down. It introduces a compensated 31-sample processing delay. Output and Mix control the result.

Saturation now offers **Soft**, asymmetric **Tube**, and **Hard** characters; Soft preserves previous behaviour. Tube removes DC from its asymmetric distortion. Compressor adds **SC low cut**, where zero is off: raising it stops bass dominating the detector. Its gain-reduction bar moves live alongside the transfer display.

Every built-in editor has **Level match**, an audition **Bypass**, and **A/B**. Level match compares rolling RMS levels and compensates processed level within ±12 dB; silence does not trigger unlimited gain. Bypass crossfades to latency-aligned dry audio while processing continues. A/B first copies the current settings into B; edit B and switch back to compare with A. These listening controls are temporary and turn off on editor close; only active effect parameters are saved. A/B snapshots remain in the loaded instance and are not stored in presets. The chain tile’s normal Bypass remains a separate processing switch.

EQ frequency, gain and Q edits now retain filter history and interpolate coefficients over 20 ms. This avoids repeatedly restarting the filters while dragging points. Band enable, filter type and stage-count changes crossfade between filter topologies.

## Instrument notes and portable instrument files

Volume and Pan on the right-hand instrument panel use rotary dials. Drag vertically, type a value below the dial, or double-click to restore 0 dB / centre. Pan has a bipolar arc from the centre position. Existing volume and pan ranges are preserved.

Open Notes from the right panel, the instrument tile's right-click menu, or the instrument editor. Notes save as you type, remain attached to that instrument when selection changes, and are included in the kit/library, DAW session, copies and instrument exports.

Import and Export are available in both Library and Kit views: use the right-panel buttons or right-click an instrument tile. The instrument editor also has Export and Notes buttons. A .druminstrument file contains the original sample audio, instrument name, notes, mixer and sound controls, envelopes, EQ, attack layer, LFO, macro mappings, output/send settings, and live built-in/VST3 effect chain settings. It is a single portable file; no sibling Samples folder is needed when moving it.

Import adds an independent instrument to the selected group without replacing a tile. New instrument/effect identities and the destination group's next keyboard position are assigned automatically. The included audio is collected into Drum's configured Samples folder. Missing samples prevent export without overwriting a previous export. An instrument with no assigned sample can still be saved and imported.

Shared return chains and global macro values belong to the destination kit, so the imported instrument uses that kit's sends and macro values. Its own send levels and macro mappings are retained. External sidechain sources reconnect by matching instrument identity or a unique name; unresolved sources become No Input and are reported after import. Self-sidechains follow the imported copy. Third-party VST3 processors must be available on the destination system.

## Standard rotary controls (0.22.0)

Mixer and built-in effect dials use the same grey face, white pointer, mint value arc and editable readout below. Bipolar controls show an arc from zero. Slow wheel scrolling makes fine adjustments; sustained faster scrolling increases sensitivity. A pause or direction reversal restores fine movement. Shift gives another tenfold precision reduction. Fractional trackpad input accumulates, inertial coast is ignored, and parameter bounds/steps and logarithmic mappings are retained.

Finger and stylus control uses direct vertical dragging; tap the value for exact numeric entry. Wheel acceleration applies to mouse/trackpad scrolling. Direct touch dragging retains fixed sensitivity and has not been device-validated in this update.

## Control standard and named macros (0.23.0)

Continuous parameter bars are now rotary dials across editing pages, EQ, envelope amounts, sends, macro values/ranges and chain sidechain controls. Binary Off/On selectors are tick boxes labelled with the function; genuine choices remain dropdowns. One isolated slow wheel notch changes the smallest displayed denomination (e.g. 0.1 dB, 1 Hz or 0.01 Q), including high in logarithmic ranges. Faster scrolling still accelerates.

In Edit > Macros, edit the name at the top of a macro card and press Enter or click away. The name is shared across that kit's instruments and saved with the kit/session. Clearing it restores Macro N. Drum notifies VST3 hosts of a parameter-title change; the stable macro IDs, automation assignments and values remain unchanged. Live controls caching of names in existing Configure panels, so a host may require refreshing its device view.

DJL Link is a native Live rack. Its unused visible macro count is minimized and its panel remains collapsed, without changing its 88 note chains or MIDI routing. Existing mapped rack macros are preserved. Live's published API does not provide per-parameter hiding for its native chain activation/speaker controls, so those automation entries cannot be removed safely by Drum. Live's Show Automated Parameters Only chooser option can filter unautomated helper entries after musical automation is created.


## Bypass, availability and playback feedback (0.23.1)

Turning a section off bypasses it and greys its dependent controls without resetting their values or graph shapes. Re-enable it to resume with the same settings. Attack, LFO, the tone filter and individual EQ bands follow this rule. Explicit Reset still restores defaults. Loop controls require Envelope playback and Loop enabled; Fade in applies in Trigger/Gate; Fade out also controls the sample-end fade in Envelope mode, so it stays available. Free LFO rate and host beat division are enabled only in their respective clock modes. Tempo Delay Time/Division and Stereo Width bass protection controls follow their dependencies too.

Every new MIDI note-on retriggers the Kit tile pulse, including repeated hits while a note remains held and hits released within one audio block. Held notes retain a quieter highlight. Library remains an independent preview. EQ point drags update Frequency, Gain and Q readouts, including after a dial previously held keyboard focus. The instrument EQ analyser rechecks its capture binding when the editor is refreshed or reopened.


## Spectrum levels and signal peak (0.23.2)

The spectrum shows the strongest stereo channel's FFT amplitudes, with a narrow 1/12-octave peak envelope for drawing on the logarithmic frequency axis. This replaces averaging dB values across neighbouring bins, which could suppress narrow high-frequency peaks. There is no frequency tilt or artificial gain compensation. A steady, bin-centred sine at 0 dBFS reads approximately 0 dBFS at its frequency; tones between bins can read about 1.4 dB lower because of the Hann window.

The separate Signal peak readout measures the largest absolute sample in either channel, holds for one second and then falls. It captures brief hits without averaging them across the FFT window. A short broadband wood hit can peak at +2 dBFS while its spectrum reads -20 dBFS or less: its energy is spread across frequencies and time. The spectrum uses an 8192-sample FFT (about 171 ms at 48 kHz), and is not a full-band output peak meter.

Instrument pre-chain identifies the selected instrument after its sound controls, EQ and mixer gain/balance, before its effect Chain and shared sends. The main output can differ because it also includes those effects, sends, other instruments and downstream host processing. A built-in EQ editor instead labels its own EQ output. Adjusting the display does not change the audio or prevent an output overload.


## Solo from every instrument tab (0.23.3)

Solo is always available in the instrument editor header, immediately before Notes, in the same position on all nine tabs. It controls the same saved instrument Solo state as the main instrument panel: changes from either button update both. Multiple instruments can remain soloed together, and Mute continues to take priority.


## Named sends (0.23.4)

Click a send name in Outputs & Sends > routing diagram, type a name such as Ambience, and press Enter or click away. Its letter remains visible (A: Ambience) on the diagram and pot caption. Names also appear in waveform choices, macro target choices and return effect window titles. Names belong to the kit, so every instrument shares them; kit/library files, Live Set state and duplicated kits retain them. Clear a name to restore Send A-F. Existing send levels, processing, IDs and automation mappings are retained.


## Utility Pan (0.23.4)

Utility includes a bipolar Pan dial, matching the instrument balance control: C preserves the stereo level, turning left attenuates the right channel, and turning right attenuates the left. It follows Utility's channel selection, width, inversion and gain stages, with smoothing while moving it. Double-click restores C. The value saves with the effect in kits, instrument exports and chain presets. Existing Utility settings load with Pan centred.


## Send-to-send routing and Echo (0.23.4)

Open Edit A-F from the routing diagram to edit a send's effects and destinations. Main is a tick box; the six destination dials send that return's processed output into other return chains, from minus infinity (silent) to 0 dB. Main and other sends can work together. Main off retains every send level. Self feeds and connections that would create a circular path are unavailable. Routing is saved with the kit and recalled in Live Sets; older kits keep each return feeding Main with its other destinations silent.

The diagram shows directed send-to-send connections on coloured rails. Enabled paths light up through successive returns; a return's Main wire stays grey when Main is unticked. The effect graph handles summing and latency compensation across serial and parallel paths. For example, route Delay on A into Room on B for reverberated repeats, with A's Main tick box choosing whether to include the delay directly as well.

The existing Tempo Delay is now labelled Delay. Its controls, saved IDs and sound remain the same: free milliseconds or host Sync, beat Division, Feedback, Low/High cut, Ping pong and Mix. Echo is a separate effect with darker default filtering, Drive for soft saturation and Wow/Wow rate for gentle timing modulation. Drive uses a bounded feedback stage; feedback is limited to 95%. Both effects can be added to instrument or shared send chains. Use 100% Mix on shared sends for a wet return.
