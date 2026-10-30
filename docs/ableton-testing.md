# Testing Drum 0.9 in Ableton Live

This is a Windows x64 development VST3 with sample loading, a full-screen sound editor, playback modes, modulation, routing and macros. It is ready for host testing, not a commercial release. See the [sound editor guide](sound-editor.md) for the exact supported controls and remaining parity work.

## Install and load

Copy the complete `DJL Drum.vst3` bundle from `build/DJLDrum_artefacts/Release/VST3/` into a VST3 location enabled in Live. On this machine, Meter already uses the custom folder `D:\DJL\OneDrive - DJ Leslieson\Abelton\Plug-Ins\vst3`. Drum's matching location is `DJL\Drum\DJL Drum.vst3` below that folder.

1. In Live's Settings/Preferences, open Plug-Ins and rescan the enabled VST3 folder. If it is not detected, hold Alt while clicking Rescan for a full scan.
2. Find **DJL Drum** in Plug-Ins and place it on a MIDI track.
3. Open the editor and drag audio from Live's **Samples** browser into empty instrument-grid space. One or more files create named instruments in the selected group. Drop a single file onto a card to replace that instrument's sample. You can also create an instrument (or use Add example kit) and click **Load sample**. Use actual audio files such as WAV or AIFF; Live's Drums category also includes kit/device presets, which are not audio samples.
4. Wait for **Ready**. Use **Play** to audition it.
5. Assign **MIDI 36** for an easy first test. Drum calls this **C2**; Live calls it **C1**. The numeric MIDI note is the same. New empty slots initially start at MIDI 21, so their default keys may be below your usual drum pattern.
6. Draw the matching note in a MIDI clip, or arm the track and play it from your controller. Vary velocity and repeat notes to check overlapping hits. Live's track needs to be active and routed to your audio output.
7. Add another sample on a different note, then another kit. Switching kits should silence the previous kit and use the new assignments.
8. Adjust volume, pan and tuning while a longer hit plays. Changes should be smooth and should not restart the sample. Tuning changes its playback duration.
9. Try Mute and Solo on instruments in different organisational groups. Multiple solos are allowed; mute takes priority. Unmuting should not resume an old hit.
10. Set open and closed hi-hats to choke group 1. Trigger the open hat then the closed hat: the closed hat should cut the open hat. Off allows overlapping hits; matching choke groups also choke repeated hits of the same instrument.
11. Save the Live Set, close and reopen it, then check names, groups, note assignments, active kit, controls and sample playback.

Official setup reference: [Ableton: Using VST plug-ins on Windows](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows).

## What to expect

- Mono/stereo WAV, AIFF, FLAC, OGG and MP3; maximum 30 seconds and 64 MiB decoded per file, 512 MiB of unique sample data including waveform caches in the active kit.
- Velocity-sensitive Trigger playback is the default; Gate and Classic respond to note-off. Up to 128 voices; Stop clears them all.
- Volume: silence at the minimum (-60 dB), up to +12 dB. Pan: -100% left to +100% right, implemented as stereo balance. Tuning: -48 to +48 semitones in 0.01 increments, plus fine tuning. Double-click sliders to reset to their defaults.
- Mute and Solo apply across the active kit, including manual Play. Choke groups 1–16 are independent of organisational groups and work across MIDI channels, using a short 2 ms release. Main supports mono/stereo; auxiliary and send buses are stereo.
- Loading or replacing a sample changes the active bank and cuts existing voices. Renaming or moving an instrument within the kit preserves its MIDI assignment and does not cut audio.
- Samples stay at their original absolute paths. Keep those files available. Use **Replace** to relink a missing file. Live's Collect All and Save is not a Drum sample-collection mechanism.
- Wait for Ready after loading a Set or switching kits before recording/exporting. Offline-export loading synchronisation is not implemented yet.

The example kit supplies names and layout only. No commercial sample library is bundled. Version 0.5 adds the editing and routing capabilities below; the historical 0.3/0.4 validation notes describe the earlier builds.

## Testing the 0.5 editor

1. Select a loaded instrument and click **Edit sound**. In 0.6, Edited is the default waveform view and updates as controls change. Choose Source to drag Start/End markers, then switch back to Edited and audition it. Change Direction to Reverse. Scroll down to reach crossfade, fades and slicing; the waveform stays visible above the controls on every tab.
2. Compare Trigger (ignores note-off), Gate (uses Fade out on release), and Classic (amplitude ADSR). Loop is active only in Classic. Try a sustained MIDI note and release it; manual Play uses a one-second gate.
3. Use the envelope, filter and LFO pages to shape a sound. Filter drive follows sample gain. Velocity and pitch-bend range default to the old behavior. Tuning now spans -48 to +48 semitones.
4. Use **Slice to new group** on a rhythmic sample. Equal-sized slices take free keys in a new group. The original remains unchanged. This is region slicing, without automatic transient detection or tempo warping.
5. On **Outputs & sends**, route an instrument to Out 2. On a Live audio track, select the Drum track in Audio From and the corresponding plug-in output in the lower chooser; set Monitor to In. Check that the instrument reaches that track. Test Send A similarly, adding an effect on the receiving track. There are Main + 15 auxiliary stereo destinations and six stereo send buses. Sends do not contain built-in effects.
6. On **Macros**, assign a target and range to Macro 1. Expose Macro 1 through Live's Configure controls and record/draw automation. The macro overrides the mapped base value. Repeat mappings on several instruments to control them together. There are 16 host parameters, with mappings stored per instrument.
7. Save and reopen the Set, checking the edited boundaries, envelopes, routing and macro mappings. Old projects load neutral editing defaults. Main may be mono/stereo; enabled extra buses are stereo.

### Native check — 28 October 2026

The 0.5 editor loaded the existing 0.4 test Set in Live 12 Suite, preserving both samples and the -17.4 dB instrument level. The waveform End marker was dragged to approximately 69.09%; manual Play produced -19.4 dB peaks on the Drum track and Main. A separate `Drum Editing Test.als` was saved under `.local/Editing Test/Drum Editing Test Project/`, reopened, and the trim and playback verified again. The macro page displayed the 16 controls at their restored default values.

Computer Use was stopped by the user during the final update. The tested native instance uses module SHA-256 `3fb1d7c4f46b8084ce3ed2edeb34380251bf8a0ec750556b7604035634b6c76a`. The final packaged build also fixes disabled-LFO volume, muted-preview isolation and state-restore notifications; that module must be copied after Live unloads the plug-in. The previous 0.4 installation is backed up under `.local/installed-pre-0.5-20261028/`.

Automated processor tests cover every editing field's persistence, Gate/Classic release, loops in both directions, ADSR, pitch, filters, LFO bypass, voice limits, macros, routing, slicing and UI wiring. The compiled VST3 test verifies gate timing, auxiliary/send audio and host macro automation as well as earlier playback/recall. Native MIDI performance, Live output-track routing and automation-lane recording still require their separate host checks; they are not implied by the audition test.

## Validation boundary

### Free envelope and pitch/key review (0.9)

Installed on 29 October 2026 with Live closed. The two installed bundle files match the tested build. Module SHA-256: `0dc21f9d9cb503c9322729a5b152d457aa25622e33ba39fc2f00c2745cce078a`. Backup: `.local/installed-pre-0.9-20261029-091843/`. CTest 2/2 and Steinberg validator 47/47 passed; native Live review remains pending.

1. Click a line to add a point; move it in both axes; double-click to remove. Hold and drag a segment to bend it around the grabbed location.
2. Right-click a non-final point to set S. Compare held notes and short note-offs in Classic; inspect the release section.
3. With pitch amount zero, add a point (no sound change), then move it (+12 st enabled). Check Edited waveform cycle spacing and listen. Try negative amounts and the graph-header amount slider.
4. Confirm Trigger MIDI note and Preview pitch are separate. Noise may be uncertain. Choose a manual key and check the Camelot code; save/reopen to verify its recall.
5. Use ADSR to restore numeric control, test a timing macro, and check compact view. Review the complete PDF manual in the package for how/why guidance.

### Draggable envelope review (0.8)

Version 0.8.0 was installed with Live closed on 29 October 2026. Every installed bundle file matches the tested build; module SHA-256 is `e9beccd0defd8b56af131fc8f2daa15591baa50036aa28bd3e213526ed7cff124`. The previous bundle is backed up under `.local/installed-pre-0.8-20261029-013342/`. CTest passed 2/2 and Steinberg validation passed 47/47. Normal/compact envelope layouts and the filter graph were inspected. Live was left closed; native review remains pending.

1. Open Edit sound > Envelopes & pitch. In Trigger/Gate, the amplitude graph should state it is inactive. Click **Use Classic** to enable it explicitly.
2. Drag A horizontally, D horizontally/vertically, S vertically and R horizontally. Try dragging the connecting lines too. Numeric values and the Edited waveform should update; audition the result.
3. Try the graphs' own +/-/Fit controls and fine numeric edits below them. Their axes are independent of the waveform. Sustain is a held-note illustration, not a fixed duration: test actual MIDI note lengths in Classic.
4. Give the pitch envelope a positive then negative amount and shape its graph. On Filter, enable filtering and a nonzero Filter envelope amount, then shape its curve. Negative amounts should draw downwards from zero.
5. Map a macro to Amp attack, decay or release. The corresponding point should follow the macro and become locked, while other points remain editable. Removing the mapping should restore direct access to the stored base value.
6. Resize the window to compact size, save/reopen the Set, and confirm envelope values and playback recall correctly.

### Mono and attack-layer review (0.7)

Version 0.7.0 was installed with Live closed on 28 October 2026. Every installed bundle file matches the tested build; module SHA-256 is `a2211e5f0e593c3a4579e8d48285019eea23f07bf678a18ee72219044d3fa4f8`. The previous bundle is backed up under `.local/installed-pre-0.7-20261028-231930/`. CTest passed 2/2 and Steinberg validation passed 47/47. Normal/compact attack controls and the mono Sample view were inspected. The following native Live checks remain for review; Live was left closed.

1. Open Edit sound > Sample on a stereo hit. Toggle Source channels between Stereo and Mono. Mono should centre the source while preserving subsequent instrument pan. Source view retains the original stereo file; Edited > Auto channels collapses identical processed L/R to one lane.
2. Open Attack layer, select On and try Click at -18 dB, 1.8 kHz and 8 ms. Compare Off/On using Play or MIDI. Try Pulse for a tonal knock and Noise for a rougher attack; adjust tone, level and decay. Zoom into the first few milliseconds in Edited. The sample's tail should stay unchanged after the layer ends.
3. Confirm the layer follows instrument volume, pan, mute/solo, choke and sends. A long Classic amplitude attack or sample Fade in also softens the layer. Loops should not repeat the click at each wrap.
4. On Envelopes & pitch, use Classic ADSR for amplitude-envelope changes and a nonzero Pitch envelope amount for pitch-envelope changes. On LFO, choose On and a nonzero depth; Filter depth also needs Filter On. Edited should update; Source should remain unchanged.
5. For native Ableton audio effects, route one instrument to Out 2 and receive that output on a Live audio track with Monitor In. Put EQ Eight, Compressor or other Live audio effects on the receiving track. Use Out 3 and a separate track for another independent chain. Instruments sharing an output share effects; there are 16 main/aux destinations and six sends, not 88 independent effect chains. External effects are not included in Drum's Edited waveform. Drum does not accept native Live devices or third-party VST3 effects dragged into its editor.
6. Save/reopen the Set and verify each instrument's Source channels and Attack settings, plus external effect tracks. Open an older Set and verify Stereo/attack Off defaults with prior sample edits preserved.

### Vertical zoom update — 28 October 2026

Version 0.6.1 was installed after Live closed. Every installed bundle file matched the tested build; x64 module SHA-256 is `ca567921697f64ba518757bc573bc63fe6bce35b35d8e43507276b295682c7b2`. The previous installation is backed up under `.local/installed-pre-0.6.1-20261028-224224/`. CTest passed 2/2 and Steinberg validation passed 47/47. Quiet stereo, mono and compact editor renders were checked.

In Edited view, use **Fit Y** or the **Y zoom** selector to inspect a quiet hit. Adjust controls and check that the scale stays fixed while the shape/level changes. Drag the divider below the waveform to resize it. Auto channels should use a single lane for identical L/R audio; use Left only or Right only to inspect a stereo channel at full height. These operations change the display only. The new version has not yet been reviewed in a native Live session; Live was left closed after installation.

### Persistent preview update — 28 October 2026

Version 0.6.0 was installed after the user closed Live. Every bundle file matched the tested build; x64 module SHA-256 is `3624451784f6a0e3fef5973ccc3ac9be97e59ab5d12b458ba073a2b0cada3fe2`. The previous installation is backed up under `.local/installed-pre-0.6-20261028-220836/`. CTest passed 2/2 and Steinberg validation passed 47/47. The preview was compared sample-for-sample with audition DSP at 44.1/48/96 kHz, and editor renders were reviewed on all tabs and at compact size.

Reopen the Set, enter Edit sound and change envelope/filter/LFO controls while switching tabs. Edited should update without pressing Play, with the waveform fixed above the scrolling controls. Source retains trim-marker dragging. Try a macro change and a Send view as well. This version has not yet been checked in a native Live session; Live was left closed after installation.

### Waveform update — 28 October 2026

After the user closed Live, version 0.5.1 was installed in the custom VST3 location above. Every installed bundle file was checked against the tested build. The x64 module SHA-256 is `c5013b274d92a83235fbe5653258a76863baeb0388fea92bc6bb58985319f748`. The previous installation is backed up under `.local/installed-pre-0.5.1-20261028-214228/`. This supersedes the earlier pending-copy note and includes the final 0.5 playback fixes.

CTest passed 2/2 and Steinberg validation passed 47/47. Actual editor renders were checked at normal/compact sizes, 128x zoom and 2x display resolution. Reopen Live to review the installed waveform: **Ctrl + wheel** zooms at the pointer, **Shift + wheel** scrolls, and **Fit** restores the whole file. This patch's native Live session check remains pending; Live was left closed.

Automated tests exercise the processor and compiled VST3, including MIDI-triggered output and recall into a fresh hosted instance. Steinberg's validator checks the VST3 interface separately. Hardware-controller, MIDI-clip and wider audio-device testing remain to be completed using the checklist above.

On 26 October 2026, version 0.3.0 was installed in the custom location above. The installed x64 module matched the tested build, SHA-256 `42b7c8f00376abebaf05b3f904e4023017ded27ed5baa6cb81d81563d261b50a`. CTest passed 2/2 and Steinberg validation passed 47/47. The native file picker also loaded a WAV successfully in the development harness.

Version 0.4.0 was subsequently built, installed and checked in Live 12 Suite. CTest passed 2/2 and Steinberg validation passed 47/47. The installed module matches the tested build, SHA-256 `2022e4b3254191f80b1255c2676e52e569b14d3f057f8b8ea90659df6a016074`. Direct browser drops of Core Library `Tom 909 Tran 5.wav` and `Tom 909 Mid.aif` created named instruments on MIDI 21 and 22. Dropping `Tom 909 Tran 4.wav` onto the first card replaced its sample while preserving its name and note. The native volume slider changed that instrument to -17.4 dB. The editor was resized and positioned beside the browser; its scaled layout and all controls remained visible.

Known compatibility limit: the separate browser entry `Tom-909-Mid.aif` returned an unsupported-audio error and created no instrument. Its underlying encoding/source has not been identified; the `.aif` extension alone does not guarantee compatibility. Standard Core Library AIFF loading succeeded. For an unreadable source, try exporting the sound from Live as a standard PCM WAV and importing that file. Do not assume every Pack/browser entry is directly decodable by Drum.

The native test Set was saved and reopened on 27 October 2026. Both instruments, their sample references and the -17.4 dB volume setting recalled successfully with Ready status. Manual Play produced output on Live's track and Main meters before and after recall. This checks audition through Live's audio engine; it does not substitute for a controller or MIDI-clip performance test. The local test Set is retained under `.local/Controls Test/` and is excluded from Git and packages.


## Review the instrument Chain (0.12.1)

1. Keep clips/controller input on the single Drum MIDI track. Open a loaded instrument's Edit sound > Chain.
2. Add an installed VST3 audio effect from Explorer or using Load VST3. Open its controls and audition the instrument. Confirm another instrument stays unaffected.
3. Add a second effect, drag its header to swap order, bypass each, then remove both and compare the unchanged dry sound.
4. Save/reopen a test Set and confirm effect order, settings and bypass. Try switching kits and copying an instrument; copies must edit independently.
5. Test high-latency modes and reverb/delay tails, plus any chosen outputs/sends. The waveform and pitch/key readout show built-in processing before Chain.

No extra Live tracks are needed for internal VST3 chains. Native Live effects still require Live routing. Live's browser drag exported neither a file path nor plugin identity in the Windows check. Use Load VST3 or a .vst3 file drop instead.

## Windows browser bridge review (0.12.2)

1. Keep Edit sound > Chain visible. Drag Pro-G or Pro-C 2 from Live’s browser onto the named target. Choose a module if duplicate installations are offered.
2. Check the effect is appended once; open it and audition only the intended instrument. Save/reopen and verify settings.
3. Cancel a drag, change instrument or close the editor during a drag: no unintended effect should be added. Normal card buttons, reorder and Explorer file drops must still work.
4. Native Live devices and non-VST3 entries must not create an effect. If a browser entry cannot be resolved, use Load VST3.

The external prototype successfully received Pro-G and Pro-C 2 browser drops with correct names despite empty OLE data. Integrated-host acceptance is a separate check; do not treat the prototype as proof of the complete plugin workflow.
