# DJL Drum

Drum is a VST3 drum instrument for DJLeslieson, intended to replace drum-rack workflows with a clearer kit/group/instrument interface. Uses the same CMake/C++20/JUCE 8.0.12 foundation as Meter.

The design supports up to 8 named kits, each with 1–16 groups and up to 88 instruments, with one active kit at a time. See the [product specification](docs/product-specification.md).

Version 0.5 adds a tabbed sound editor: waveform trimming, reverse, normalise, sample gain, Trigger/Gate/Classic playback, looping, envelopes, filters, LFO modulation, slicing, output routing and 16 automatable macros. Volume, pan, tuning, mute, solo, choke groups and bulk sample drag-and-drop remain in the kit workspace. Project state recalls the whole library, active kit, sample paths, editing controls and macro mappings.

Version 0.5.1 improves waveform detail: display-resolution peaks, separate stereo lanes, individual-sample zoom and visible + / - / Fit buttons. Ctrl + wheel zooms at the pointer; Shift + wheel scrolls.

Version 0.6 keeps the waveform above all six Edit Sound tabs while the controls scroll below it. **Edited** previews a hit through the current sound controls and macros, updating as you edit. **Source** retains the original waveform and trim markers; **Send A–F** preview the send signals before external effects. The edited view uses a fixed amplitude scale so gain changes remain visible.

Version 0.6.1 adds **Y zoom** (0.25x–128x), **Fit Y**, a taller waveform and a draggable height divider. **Auto channels** draws mono or identical L/R output in one full-height lane; **Stereo L / R**, **Left only** and **Right only** offer other display layouts. These controls change the picture only. Use Alt + wheel for vertical zoom, Ctrl + wheel for time zoom, and Shift + wheel to scroll.

Version 0.7 adds per-instrument **Source channels: Stereo / Mono** on the Sample page, plus an **Attack layer** page for a short Click, Pulse or Noise with level, tone and decay controls. Mono averages the source channels before filtering; pan and stereo modulation follow it. Edited and Send waveforms include both changes. Existing projects keep Stereo playback and the attack layer Off.

Version 0.8 adds draggable **amplitude, pitch and filter envelope graphs**. Drag their A/D/S/R points or connecting lines to shape the sound; numeric controls and the Edited waveform update together. Each graph has its own time zoom and Fit. Inactive envelopes are labelled, and macro-controlled stages follow automation without overwriting their base settings.

Version 0.9 replaces fixed envelope handles with **2–32 editable points and curved segments**, including a movable sustain/hold point. Pitch amount is visible beside its graph, and audible graph edits switch to Edited view. A new readout separately shows the MIDI trigger, estimated processed pitch, and musical key/Camelot with uncertainty or an explicit manual label. Old projects retain their original ADSR behaviour.

Version 0.10 adds waveform time graticules, compact single-row controls and a red sample-gain headroom warning. Envelope graphs sit above the larger waveform; quick line drags insert/move points, held drags bend segments, and points can cross neighbours. Audible amplitude edits enable Classic playback, filter edits enable the tone filter, and optional ADSR sliders stay out of the main view. Filter now also offers an eight-band stereo EQ with draggable frequency/gain, Q, eight response types and independent band bypass.

The complete [illustrated PDF manual](output/pdf/DJL-Drum-User-Manual-0.10.0-Windows.pdf) covers setup, every workflow/control, sound recipes, troubleshooting and beginner-friendly explanations of why to use each feature. Its Meter-style layout is available as the reusable [DJL manual layout skill](skills/djl-manual-layout/SKILL.md).

![Editable envelope points and curves](docs/images/drum-envelopes.png)

Use **Add example kit** to review a populated layout. Its named instrument slots start empty. Select a slot and choose **Load sample**, or drop one audio file onto an instrument card to replace its sound. Drop one or more audio samples from Ableton's Samples browser or Explorer into empty workspace space to create named instruments in the selected group, with free MIDI notes. **Play** auditions the selected instrument; incoming MIDI triggers its assigned note with velocity-sensitive, overlapping one-shot playback. WAV, AIFF, FLAC, OGG and MP3 are supported, with mono/stereo files up to 30 seconds and 64 MB decoded. The active kit allows 512 MB of unique sample data including waveform caches, and 128 simultaneous voices.

Volume ranges from silence to +12 dB, pan from left to right, and tuning from -48 to +48 semitones, plus fine tuning in the editor. Double-click a slider to reset it. Solo works across the active kit and allows multiple solos; mute takes priority. Choke Off allows overlap. Instruments sharing choke group 1–16 cut earlier hits in that choke group, independently of organisational groups and MIDI channels. Tuning changes sample speed and duration; pan is stereo balance.

Select an instrument and click **Edit sound**. Seven scrollable pages keep the controls accessible at smaller window sizes. See the [sound editor guide](docs/sound-editor.md) and [testing in Ableton](docs/ableton-testing.md). Samples remain external files: keep them at their saved locations, or use **Replace** to locate moved files. Main supports mono/stereo, with 15 additional stereo outputs and six post-fader send outputs for DAW effects. Ableton's native effects can process these outputs on receiving Live tracks; native Live devices and third-party VST3 effects cannot currently be loaded inside Drum. A separate standalone product is planned later.

This is a substantial editing preview, not complete Ableton device parity. Warp/time-stretch, transient/manual slicing, layered device chains, built-in return effects, glide/spread, envelope looping, macro variations and Live device/preset importing remain outside this build. Existing audio-sample drag-and-drop is supported.

![Drum sound editor](docs/images/drum-sound-editor.png)

![Per-instrument attack layer](docs/images/drum-attack-layer.png)

![Detailed stereo waveform](docs/images/drum-waveform.png)

## Windows build

Install Visual Studio 2022 with Desktop development with C++, a Windows SDK, and CMake 3.22+.

```powershell
./scripts/build.ps1
# Optional existing JUCE checkout / alternate build location:
./scripts/build.ps1 -BuildDirectory D:/Builds/Drum -JuceSource D:/Dependencies/JUCE
```

The script configures x64, builds Release and runs CTest. Without `-JuceSource`, CMake downloads the pinned JUCE release.

Outputs under the build directory:

- `DJLDrum_artefacts/Release/VST3/DJL Drum.vst3` (copy the entire bundle for DAW testing).

```powershell
./scripts/package.ps1 -BuildDirectory ./build
```

This copies the VST3 development bundle to ignored `dist/`. No installer or signing is configured yet.

An optional standalone development harness can be built with `./scripts/build.ps1 -Standalone` and packaged with `./scripts/package.ps1 -BuildDirectory ./build -Standalone`. Its output is `DJLDrum_artefacts/Release/Standalone/DJL Drum.exe`; it is not the planned standalone product. Use a fresh output directory when changing package formats to avoid retaining files from an older package.

## macOS build

With Xcode command-line tools and CMake installed:

```bash
bash scripts/build-macos.sh
# Optional: JUCE_SOURCE_DIR=/path/to/JUCE bash scripts/build-macos.sh /path/to/build
```

Builds a universal VST3 target. Set `DRUM_BUILD_STANDALONE=ON` for the optional development harness. Requires native macOS validation.

See [development notes](docs/development.md) and [third-party notes](docs/third-party.md).

## Reviewing the layout

![Drum kit workspace](docs/images/drum-workspace.png)

The editor shows kit tabs, a group list, instrument cards, an instrument details panel and an 88-key overview. Resize from its bottom-right corner; the layout scales down to 800×600 so the host browser can remain visible. The initial size adapts to the primary display's work area. Rename by pressing Enter or clicking away; Escape cancels the edit. Click an assigned key to select its instrument. Already assigned keys are disabled in another instrument's note selector. Remove actions ask for confirmation; at least one kit and one group per kit are retained.

Build the optional review app with `./scripts/build.ps1 -Standalone`, then open `build/DJLDrum_artefacts/Release/Standalone/DJL Drum.exe`. This is a development harness for interface review. The VST3 remains the product target.

Tests write `drum-layout.png`, `drum-layout-minimum.png`, `drum-layout-compact.png`, `drum-layout-dense.png`, `drum-layout-empty.png`, `drum-playback.png` and `drum-controls.png` into the build directory, rendering the actual editor. CTest also loads the compiled VST3, verifies MIDI-triggered sample output, and checks saved-state recall in a fresh instance.
