# Development

The build foundation follows Meter: CMake 3.22+, C++20, JUCE 8.0.12, a static MSVC runtime, VST3 and standalone targets, and CTest processor tests.

## Layout

- `src/`: audio processor and editor. Add DSP and parameter/state handling here.
- `assets/`: embedded DJL branding.
- `tests/`: processor, state and editor smoke tests.
- `scripts/`: Windows and macOS build entry points and Windows development packaging.
- `docs/`: development and dependency notes.
- `build/`, `build-macos/`, `dist/`, `.local/`: generated files, excluded from Git.

Drum has its own bundle ID `com.djleslieson.drum` and four-character plugin code `DjDr`, distinct from Meter. The manufacturer code remains `DjlS`.

The starter is a transparent mono/stereo audio effect with no MIDI or parameters. Decide the instrument/effect requirements before implementing Drum DSP; an instrument needs the synth/MIDI flags and bus layout changed together. No Meter measurement engine, activation service, signing keys or release automation is included.

Windows uses Visual Studio 2022 x64. The macOS script targets universal arm64/x86_64 with Xcode and macOS 11+. Native macOS validation is still required. Packaging creates development artifacts only; installer/signing/release configuration can be added when product requirements are known.

Tests cover unchanged audio at 44.1/48/96 kHz, mono/stereo, zero/short/normal blocks, unsupported layouts, state round trip, malformed state and editor construction. DAW loading and audio-device runtime validation remain separate checks.
