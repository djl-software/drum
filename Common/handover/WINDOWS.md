# Windows handover

Last updated: 2026-11-16. Read [LIVE.md](LIVE.md) and the root [AGENTS.md](../../AGENTS.md) before changing code. This file is the authoritative Windows build and handover guide.

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

## Branches and next actions on the PC

Repository: `https://github.com/djl-software/drum.git`.
`development/windows` starts from Mac checkpoint `5d17d44` and adds Windows build preparation in `909c107`. `main` was deliberately made identical to `development/mac`; Windows-specific preparation belongs on `development/windows`.

```powershell
git clone --branch development/windows https://github.com/djl-software/drum.git
cd drum
git status
powershell -NoProfile -ExecutionPolicy Bypass -File .\Windows\scripts\build.ps1
```

If the repository already exists, commit or preserve any local work before switching branches, then fetch and switch to `development/windows`. Do not overwrite PC changes with a reset. Keep a Windows checkout separate from a Mac checkout when both machines are in use.

1. Install the prerequisites above and run the build script. Retain the configure/build/CTest output and record the actual tool versions and commit in LIVE.md.
2. Fix MSVC/Win32 failures on the Windows branch. Apply the source-isolation rule in AGENTS.md before any potentially unsafe change to Common.
3. Package only after compilation and CTest pass. The package script is not itself proof that a build is valid.
4. Test the plugin in Windows Live with a disposable set before using a production set. Keep Live closed while replacing the plugin bundle.
5. Record each feature below as Windows-tested, Windows-port-needed, or unresolved. Mac results do not establish Windows behavior.

## Feature handover and Windows parity work

| Area | Current source/status | Windows follow-up |
| --- | --- | --- |
| Attack FSD at 0 dB; eight attack types; Tune before Filter | Shared editor/render changes in Common | Compile and run attack tests; check UI and output on Windows. |
| Instrument/send VST3 selection and browser drops | Windows uses Common/src/LiveBrowserBridge.cpp; Mac uses its own native bridge | Test picker insertion, repeated instrument/send drops, pointer feedback and window visibility in Windows Live. |
| Track-local instrument tabs, effects, routing and sends for same-kit tracks | Mac preservation logic in Mac/src/TrackSendState.h, integrated from Common | Explicit Windows parity work; test two tracks using the same kit, plus host save/recall and linked-kit updates. |
| Independent group chain, six private returns, destinations to Main/send/group | Mac/src/GroupGraph.inc, GroupMix.inc, GroupEditor.h; shared model and routing validation in Common | Windows retains its older DSP path. Port deliberately; do not assume model serialization means Windows audio routing is implemented. |
| Group fader controls dry and all owned sends/tails without cutting another group | Mac per-group shared-return histories and terminal group faders | Windows must be audited/ported and tested separately. Test minimum, partial gain, shaped sends, cascades, separate outputs and retained tails. |
| Back navigation; mixer overview opens group; group button inside group opens processing | Common UI integration with Mac group editor | Verify Windows navigation and implement group setup only with its actual DSP support. |
| Kit group tiles latch distinct triggered instruments; Play resets latch | Shared SampleEngine/UI changes | Verify host Play, Drum Play, Pattern Play, Stop, note-off and completed tails. |
| Done buttons in effect/Notes/send/group subwindows | Mac wrapper/helper and gated labels | Windows still uses its existing close behavior. Port with native editor resize/lifetime tests. |
| Mixer can expand to the last channel but not beyond it | Mac/src/EditorWidth.h with shared strip-count helper | Windows retains its previous limits. Port both hosted and floating constraints and test smaller displays and group views. |
| Mixer size, position, selected view saved per instance | Mac/src/EditorPresentation.h, native capture/restore in Mac/src/HostFloat.mm, Mac-gated processor session data | Windows-specific placement implementation and host recall tests required. |
| Float above Live but below other foreground apps | Mac native activation-dependent level for Float and its children | Audit existing Windows behavior separately; preserve browser-drop visibility without globally pinning Drum over other apps. |

## Validation and limits

No Windows binary has been compiled, no Windows CTest run has completed, and no Windows Live validation has been performed for this snapshot. PowerShell scripts were source-audited on the Mac; PowerShell/MSVC execution still needs the PC. The native C# browser helper is legacy tooling; do not make its installation a prerequisite for the in-plugin C++ browser adapter without a deliberate change.

Latest Mac checks for the snapshot: mac_window_ordering, drum_processor, mixer and vst3_instrument_chains passed; the mixer test was repeated after adding the recreated-editor/Live-state recall regression. An earlier full 12-test Mac run also passed before the later window-only changes. Tests are in Common/tests and Mac/tests; paths to temporary logs on the Mac are not a substitute for PC build evidence.

The attempted Windows VM setup was stopped at the user's request. UTM was downloaded and signature-checked but not installed. The Windows ISO download was cancelled; any partial file in ignored Windows/work is unusable installation media. No VM, Windows license acceptance or Windows compiler installation was completed. Build on the physical PC instead.

The Windows chat was created under the Drum project on the Mac. Chat visibility does not relocate the execution host or clone the repository. If starting a PC-local chat, ask it to read AGENTS.md, Common/handover/LIVE.md and this file first; these tracked files carry the development context across machines.
