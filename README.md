# DJL Drum

- [Windows](Windows/): PowerShell build and packaging scripts, C# browser bridge and its tests, Windows manuals.
- [Mac](Mac/): macOS build and installer scripts, development packages, Mac manuals and local validation work.
- [Android](Android/): reserved for Android-specific implementation.
- [iPad](iPad/): reserved for iPad-specific implementation.
- [Common](Common/): shared C++/JUCE source, CMake project, tests, assets, Ableton companion, documentation and manual tooling.

See the [project guide](Common/README.md) and [development notes](Common/docs/development.md).

Run from the repository root:

```powershell
./Windows/scripts/build.ps1
```

```sh
bash Mac/scripts/build-macos.sh
```

For direct CMake builds, use `cmake -S Common -B Mac/build-macos` or `cmake -S Common -B Windows/build`. Existing build directories must be configured again after the move.

Shared files that contain conditional platform implementations remain together in Common to avoid duplicating source.
