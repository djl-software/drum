# Mac handover

Last updated: 2026-11-16. Read [LIVE.md](LIVE.md) and the root [AGENTS.md](../../AGENTS.md) before changing code.

Build and installer scripts are in `Mac/scripts/`; native window and Ableton browser-drag implementations are in `Mac/src/`. Shared C++/JUCE code, assets and the CMake project remain in `Common/`.

From the repository root, run `bash Mac/scripts/build-macos.sh`. This uses Xcode and builds both arm64 and x86_64. Use a separate build folder for another CMake generator.

The native window regression is `drum_tests --mac-windows`. Attack calibration is `drum_tests --channel-attack`. Installed-effect compatibility can be checked with `drum_tests --chain-smoke /Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3` (run inside a build or scratch folder). GUI tests require access to the Mac display.

Development packages are in `test_builds/`, Mac manuals in `output/pdf/`, and local scratch work in `work/`. Generated files are excluded from Git.

Ableton Live browser drops use a Mac-only, per-host mouse receiver and native browser hit testing. This handles Live's nested drag loop without requesting global input or cross-process accessibility access. A drop onto the frontmost visible Drum chain cancels Live's own insertion; a unique enabled VST3 audio effect is inserted directly, while ambiguous/native-device names retain the chooser. The plug-in database is opened read-only.

Live 12 verification (2026-11-13, Apple Silicon): dragged ValhallaVintageVerb VST3 into both an instrument chain and shared Send A; repeated both drops after restarting with the final build; removed the duplicate test effects. Float stayed visible, Edit A opened above Drum, and the hosted Valhalla editor opened above Send A. The saved test session is under `work/`.

Browser-drag presentation temporarily attaches the main Drum window above the Live browser window, preserving existing send/editor children and restoring levels and ownership when the gesture ends. On entering a chain, a non-interactive native pointer and named plug-in badge follows the drag after Live cancels its internal insertion. Native tests cover ordering, restoration and feedback movement; Live testing confirmed a drop with Float off and Live covering the instrument window, plus a shared Send A drop.

Same-file kit links on Mac preserve each instance’s instrument/group send levels, independent send processing and send macro mappings, return names/effects/mixer/destinations. Host state stores these per-track values; normal linked sound and instrument-chain edits still propagate. Regression coverage exercises both sync directions and host recall. Outputs & Sends now highlights routes from the selected instrument, stopping at muted or silenced returns rather than showing unrelated kit sources.

Group processing is implemented in GroupGraph.inc, GroupMix.inc and GroupEditor.h under Mac/src. The Mac graph gives every group a post-member chain and six private returns. Shared serialization uses schema 28 for group processing/destinations and retains schema 27 when those additions are unused; non-Mac builds retain their existing DSP path. The routing diagram now offers Main, another send and named group destinations with cycle validation. Instrument tabs and group processing are track-local even for same-file linked kits.

Legacy active group sends copy their reachable shared return chains into private group returns, with stable independent effect IDs. Existing saved project 003 was preserved during Live validation. Browser drops into both a group chain and group Send A were accepted; temporary test effects were removed.

Back navigation: instrument Back restores the Mixer when opened from a strip, retaining the selected group; Mixer Back restores an instrument tab, MIDI editor or Library origin. Exit returns to the kit. Mixer overview group names open the instrument view; pressing the group name within that view opens group processing. Group setup and standalone send editors have Back/Exit controls. Navigation regression checks cover originating views and button placement.

Kit group tiles latch the distinct instruments triggered during a playback pass. Live transport Play and Drum/Pattern Play reset the latch; note-off, finished tails and Stop retain the counts. Pad/piano highlights stay momentary. Mixer group navigation is overview > group instruments > group processing; Back retraces these levels.

The Mac group fader now controls member dry audio, separately routed member outputs, instrument send returns and private group returns at their final output branches. Shared send settings remain shared, but active source groups have independent processing histories, so minimum also silences existing wet tails without muting another group's sends. Cascaded returns and returns routed back through their owning group receive the group gain once. Return chains are instantiated on first use and retained until kit recall/change to preserve tails when a send level is turned off. Shared effect editor changes update these processing instances in place. Windows retains its existing mixer and send processing.


Later window work: Mac/src/DoneFooter.h adds Done to hosted effect and Notes windows; standalone send/group close labels use Done. Mac/src/EditorWidth.h permits screen-dependent widening but caps the visible mixer at its final strip. Mac/src/EditorPresentation.h and Mac/src/HostFloat.mm remember hosted and floating geometry separately, restore the mixer view/group on editor recreation, and serialize this per-instance UI state under MacEditor in DrumSession. Float and its child windows drop to normal native window level when Live is inactive, allowing other apps above them. Native placement/inactive-family-level tests and recreated-editor/host-state recall tests passed; mixer reopening at its prior size was checked in Live.

The saved Live validation set was Intoxicating2026-11-13_003.als on the Mac. Do not assume its path exists on another machine or that its production contents may be changed for tests. Preserve the user's current set and use disposable test material for signal-routing checks. Native GUI actions use the supported computer-use tool; do not replace that with shell-driven input injection.

The Windows branch shares the Mac baseline but does not implement all Mac-gated behavior. Treat the Windows parity table in WINDOWS.md as pending work, not as a request to change Mac sources. The user requires main to match development/mac. Never merge Windows-specific behavior into main merely to publish Windows progress.
