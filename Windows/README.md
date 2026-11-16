# Drum for Windows

Work on `development/windows`. Its starting point is the Mac development checkpoint `5d17d44`. Build commands below run from the repository root. Keep the five platform folders together: Windows scripts configure the shared project in `Common/`.

## Build prerequisites

Use Windows 11 x64, or Windows 11 Arm in a VM on an Apple Silicon Mac. The build targets **x64** in either case, for Windows x64 VST3 hosts.

Install Git for Windows, CMake 3.22 or newer, and Visual Studio 2022 (or its Build Tools) with **Desktop development with C++**, the MSVC x64/x86 tools, **C++ CMake tools for Windows**, and a Windows 10/11 SDK. Windows on Arm needs the x64 cross-compilation tools as well. Run tests from an interactive Windows desktop; they create native editor windows.

No separate VST SDK download is needed. CMake fetches the pinned JUCE 8.0.12 checkout, which includes VST3 support. An existing checkout can be supplied with `-JuceSource C:\Dependencies\JUCE` for an offline dependency build.

## Build, test and package

In PowerShell, from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Windows\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Windows\scripts\package.ps1 -BuildDirectory .\Windows\build
```

The execution-policy setting applies only to that PowerShell process. The build script configures Visual Studio 2022/x64, builds Release, and runs CTest. It fails if configuration, compilation or tests fail. It finds CMake on PATH or in a Visual Studio/Build Tools installation, without assuming the Community edition.

For another build directory or generator:

```powershell
.\Windows\scripts\build.ps1 -BuildDirectory C:\Builds\Drum -JuceSource C:\Dependencies\JUCE
# Visual Studio 2026 requires a CMake version with its generator (4.2+):
.\Windows\scripts\build.ps1 -Generator 'Visual Studio 18 2026' -BuildDirectory .\Windows\build-vs2026
```

Use a fresh build directory when changing generators. Optional `-Standalone` adds the development app; pass it to both build and package scripts.

Built plugin: `Windows/build/DJLDrum_artefacts/Release/VST3/DJL Drum.vst3`.
Packaged plugin: `Windows/dist/DJL Drum.vst3`.

Close Live before installing. Copy the **entire** `.vst3` directory to `C:\Program Files\Common Files\VST3\DJL Drum.vst3` (administrator permission may be required), then enable/rescan VST3 in Live. This is a development package, not a signed installer. The package also contains the Drum Link rack and MIDI Remote Script under `companion/`.

The native Ableton browser adapter is compiled into the VST3 from `Common/src/LiveBrowserBridge.cpp`; the older C# helper and its separate build script are optional legacy tooling, not prerequisites for this build.

## Scope and validation

This branch includes the Mac source snapshot, but Mac-only behavior remains behind `JUCE_MAC` and under `Mac/src/`. In particular, recent Mac group routing/DSP, track-local processing isolation, window geometry recall, Done footers and native Float/browser behavior are not automatically ported to Windows. Windows keeps its existing platform implementation. Windows feature parity is follow-up work in the Windows chat.

The scripts and source selection have been audited on macOS. A successful Windows compile, CTest run and Live test are still required; a macOS build cannot validate Win32, MSVC or Windows plug-in hosting. Windows cannot load a macOS VST3 bundle.
