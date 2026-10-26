# DJL Drum

JUCE audio-plugin development scaffold for DJLeslieson. Builds a VST3 plugin and standalone app using the same CMake/C++20/JUCE 8.0.12 foundation as Meter.

The initial processor passes mono/stereo audio through unchanged. The editor is a development placeholder; Drum sound generation/processing is not implemented yet.

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
- `DJLDrum_artefacts/Release/Standalone/DJL Drum.exe`.

```powershell
./scripts/package.ps1 -BuildDirectory ./build
```

This copies the development builds to ignored `dist/`. No installer or signing is configured yet.

## macOS build

With Xcode command-line tools and CMake installed:

```bash
bash scripts/build-macos.sh
# Optional: JUCE_SOURCE_DIR=/path/to/JUCE bash scripts/build-macos.sh /path/to/build
```

Builds universal VST3/standalone targets. Requires native macOS validation.

See [development notes](docs/development.md) and [third-party notes](docs/third-party.md).
