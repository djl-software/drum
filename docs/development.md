# Development

The build foundation follows Meter: CMake 3.22+, C++20, JUCE 8.0.12, a static MSVC runtime, a VST3 target with an optional standalone development harness, and CTest processor tests.

## Layout

- `src/`: audio processor and editor. Add DSP and parameter/state handling here.
- `assets/`: embedded DJL branding.
- `tests/`: processor, state and editor smoke tests.
- `scripts/`: Windows and macOS build entry points and Windows development packaging.
- `docs/`: development and dependency notes.
- `build/`, `build-macos/`, `dist/`, `.local/`: generated files, excluded from Git.

Drum has its own bundle ID `com.djleslieson.drum` and four-character plugin code `DjDr`, distinct from Meter. The manufacturer code remains `DjlS`.

The processor is a MIDI instrument with no audio input and mono/stereo audio output. `IS_SYNTH`, MIDI input and the `Instrument|Drum` VST3 category agree with that layout. The empty engine clears output to silence. The kit/group model, samples, parameters and playback engine are still to be implemented according to the [product specification](product-specification.md).

VST3 builds by default. `DRUM_BUILD_STANDALONE=ON` enables the optional standalone development harness; a separate product edition comes later.

Windows uses Visual Studio 2022 x64. The macOS script targets universal arm64/x86_64 with Xcode and macOS 11+. Native macOS validation is still required. Packaging creates development artifacts only; installer/signing/release configuration can be added when product requirements are known.

Tests cover MIDI instrument identity, silent empty-engine output at 44.1/48/96 kHz, mono/stereo, zero/short/normal blocks, unsupported audio inputs and outputs, state round trip, malformed state and editor construction. DAW loading and audio-device runtime validation remain separate checks.

## Instrument scaffold validation — 26 October 2026

Windows x64 Release build passed using Visual Studio 2022, JUCE 8.0.12 and the default VST3-only configuration. CTest passed 1/1 processor suite. Steinberg VST3 SDK validator 3.8.1 passed 47 tests with zero failures. The generated module manifest reports `Instrument|Drum`.

These checks validate the scaffold and host interface. They do not validate sample playback, kit management, Ableton session behaviour, or the optional standalone/macOS builds.
