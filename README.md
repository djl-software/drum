# DJL Drum

Drum is a VST3 drum instrument for DJLeslieson, intended to replace drum-rack workflows with a clearer kit/group/instrument interface. Uses the same CMake/C++20/JUCE 8.0.12 foundation as Meter.

The design supports up to 8 named kits, each with 1–16 groups and up to 88 instruments, with one active kit at a time. See the [product specification](docs/product-specification.md).

Version 0.3 adds sample loading and MIDI-triggered playback to the kit, group and instrument workspace. Create, rename, duplicate and remove items; move instruments between groups; reorder groups and instruments; and assign keys across MIDI notes 21–108. Project state recalls the whole library, active kit and sample paths.

Use **Add example kit** to review a populated layout. Its named instrument slots start empty. Select a slot and choose **Load sample**, or drop one audio file onto an instrument card. **Play** auditions the selected instrument; incoming MIDI triggers its assigned note with velocity-sensitive, overlapping one-shot playback. WAV, AIFF, FLAC, OGG and MP3 are supported, with mono/stereo files up to 30 seconds and 64 MB decoded. The active kit allows 512 MB of unique decoded sample data and 128 simultaneous voices.

See [testing in Ableton](docs/ableton-testing.md). Samples remain external files: keep them at their saved locations, or use **Replace** to locate moved files. This preview has one stereo output (mono also supported), fixed one-shot playback and no gain/pan, envelopes, tuning, choke, effects or multi-output controls yet. A separate standalone product is planned later.

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

The editor shows kit tabs, a group list, instrument cards, an instrument details panel and an 88-key overview. Rename by pressing Enter or clicking away; Escape cancels the edit. Click an assigned key to select its instrument. Already assigned keys are disabled in another instrument's note selector. Remove actions ask for confirmation; at least one kit and one group per kit are retained.

Build the optional review app with `./scripts/build.ps1 -Standalone`, then open `build/DJLDrum_artefacts/Release/Standalone/DJL Drum.exe`. This is a development harness for interface review. The VST3 remains the product target.

Tests write `drum-layout.png`, `drum-layout-minimum.png`, `drum-layout-dense.png`, `drum-layout-empty.png` and `drum-playback.png` into the build directory, rendering the actual editor. CTest also loads the compiled VST3, verifies MIDI-triggered sample output, and checks saved-state recall in a fresh instance.
