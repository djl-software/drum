# Live Drum handover

Last updated: 2026-11-16. This is the handover of record across branches, chats and machines. Read the root [AGENTS.md](../../AGENTS.md), [Windows handover](WINDOWS.md) and [Mac handover](MAC.md) as appropriate. Update this file in the same change as source ownership, implementation or validation changes.

## Repository and branch state

- Repository: https://github.com/djl-software/drum.git.
- Five platform roots: Windows, Mac, Android, iPad, Common. Shared CMake project: Common/CMakeLists.txt. There are no validated Android/iPad build targets in this handover.
- Mac implementation checkpoint: `5d17d44`. Windows branch starts from that exact snapshot and adds PC build preparation in `909c107`.
- User decision: main must match development/mac, not development/windows. Before the handover-policy update, both remote main and development/mac were `5d17d44`; development/windows was `909c107`. Use `git log`/remote refs for subsequent documentation commit IDs.
- Prior main is preserved at tag `backup/main-before-mac-2026-11-16` (baseline `1397bbf`).
- Windows chat exists under the Drum project. To continue on a PC, clone/fetch the Windows branch and start the PC-local chat by reading these tracked notes; remotely opening the Mac chat does not move compilation to the PC.

## Current outcome and next action

The Mac changes are implemented and have Mac test evidence. Windows build scripts and packaging were audited, but Windows compilation, CTest and Live testing are still pending on the user's PC. Follow WINDOWS.md for exact prerequisites, commands, outputs and the feature-parity matrix. Windows-only additions must remain on the Windows branch.

No Windows VM was installed. The user stopped setup and chose the physical PC. UTM installer media was downloaded and verified but not installed; the Microsoft ISO download was interrupted. Ignored Windows/work may contain a partial ISO. Do not use it or commit it.

## Product behavior to preserve

- Attack reaches FSD at 0 dB for all eight types; Tune precedes Filter in the instrument tabs.
- Mac instrument/send VST3 drops and pickers insert into Drum without hiding the relevant window; auxiliary editors open above their Drum owner. Send highlighting follows the selected instrument's actual routing.
- On Mac, each Live track retains its own instrument-tab processing, effects, sends, returns and group settings even when tracks use the same kit.
- Mac groups process member instruments after instrument chains; group chains/private returns are independent. Instrument returns can route to Main, other sends or a group, subject to cycle validation.
- The Mac group fader controls all owned dry and send output, including existing wet histories, without cutting another group's contributions. Cascades/own-group routing must not apply the same group gain repeatedly.
- Back retraces the origin; mixer overview group names open their instruments, and the group button within that view opens group processing. Done closes Mac effect/Notes/send/group subwindows.
- Kit group activity latches distinct triggered instruments and resets on host/Drum/Pattern Play; Stop, note-off and ended tails retain the latch.
- Mac mixer width is capped at its final visible strip and screen width. Hosted and floating geometry are remembered separately per instance; reopening restores the mixer view/group. UI state is serialized in DrumSession/MacEditor. Off-screen saved placement is recovered onto a visible screen.
- Float stays above Live while Live is foreground, but its entire window family drops to normal level when Live is inactive so other foreground apps may cover it.

Windows parity is not implied by shared models or by the branch containing Mac source files. Consult the per-feature Windows table before claiming a feature is available there.

## Existing source ownership and migration register

These entries describe the inherited baseline. No new source files were moved by the handover-policy documentation change. Most entries are **partial extractions**: remaining Common integration still exists and must be split at file level before a future potentially unsafe platform-specific change, per AGENTS.md.

| Original/shared location | Current platform implementation | Baseline and build selection | Status / remaining work |
| --- | --- | --- | --- |
| Common/src/platform/HostFloat.cpp; Common/src/LiveBrowserBridge.cpp | Mac/src/HostFloat.mm; Mac/src/LiveBrowserBridge.mm; Mac/src/LiveBrowserDrop.h | Mac checkpoint 5d17d44. Common/CMakeLists.txt selects these Objective-C++ files only for desktop Apple; non-Apple selects the original Common C++ files. | Platform alternatives exist; Windows implementation still resides in Common and needs a file-level split if next modified with cross-platform risk. Android/iPad variants are not validated. |
| Common/src/EffectRack.cpp/.h | Mac/src/GroupGraph.inc, GroupFader.h, GroupRouting.h, SharedReturnTargets.inc | 5d17d44. JUCE_MAC-gated includes; Windows retains the prior graph and processing-lane count. | Partial graph extraction; whole EffectRack files remain Common. Per-group shared-return histories and group terminal faders are Mac-only. |
| Common/src/SampleEngine.cpp/.h | Mac/src/GroupMix.inc; Mac lane helpers from GroupRouting.h | 5d17d44. JUCE_MAC selects group mixing/shaped-send lanes; other builds keep their old path. | Partial extraction; shared rendering and conditional integration remain. Audit Windows group/send behavior before a parity port. |
| Common/src/PluginProcessor.cpp and shared linked-kit logic | Mac/src/TrackSendState.h | 5d17d44. Included/applied only for JUCE_MAC track-local state handling. | Partial extraction; Windows has not been given the same track-isolation behavior. |
| Common/src/MixerPage.cpp and ChainEditor integration | Mac/src/GroupEditor.h | 5d17d44. Mac-only group-setup UI; shared model helpers exist in KitLibrary. | Partial UI extraction. Windows group UI and DSP must be ported together. |
| Common/src/EffectRack.cpp, PluginEditor.cpp and platform/DoneFooter.h | Mac/src/DoneFooter.h | 5d17d44. Platform helper wraps content only on Mac; other platforms receive original content. | Mac-only Done footer. Common wrapper/labels remain integration points. |
| Common/src/PluginEditor.cpp/.h, PluginProcessor.cpp/.h, DrumCompanionWindow.h | Mac/src/EditorWidth.h; Mac/src/EditorPresentation.h; native functions in Mac/src/HostFloat.mm | 5d17d44. JUCE_MAC-gated UI constraints, placement memory and session serialization. | Partial extraction. Windows window-size/position persistence and foreground behavior require native implementation and tests. |
| Common tests and desktop source selection | Mac/tests/WindowTests.mm | 5d17d44. Added to drum_tests only for desktop Apple; MixerTests.cpp compiled as Objective-C++ on Apple. | Mac native window tests do not establish Windows native behavior. |

### Required record for each future source split

Append an entry with: date/change reference; original Common file(s); pre-change baseline revision; active platform and its new path; unchanged copy path for **each** other affected platform; files removed from Common; exact build/include selection; actual compile/runtime checks by platform; unvalidated platforms; next porting action. A partial extraction must be labelled partial, never reported as a completed move.

## Validation evidence and limits

- Latest Mac snapshot checks: mac_window_ordering, drum_processor, mixer and vst3_instrument_chains passed (four tests); mixer was rerun successfully after adding the saved-state/recreated-editor regression.
- Earlier complete Mac suite: 12/12 passed after the group-fader fix, before the later UI/window-only changes. Do not present that older full run as a full rerun of every later change.
- Live checks: instrument/send browser drops and picker/editor ordering; group processing drops; latch/Play reset; mixer final-strip width cap; reopening into the mixer at its previous size. Native tests also cover placement roundtrip, host-state recall and lowering the inactive Float family.
- Windows: no compile, executable, CTest run or Windows Live verification for this snapshot. PowerShell scripts still need actual PC execution.
- Android/iPad: no build or runtime validation recorded.
- Mac UI testing previously hit an unresponsive Live session during a save; its root cause was not established. Do not label it a fixed plugin defect or infer Windows behavior from it.

## Handover policy change

On 2026-11-16 the user required all handover notes to be common and required uncertain/risky shared-source changes to become platform-specific, with unchanged copies for the other affected platforms and every move recorded here. Root AGENTS.md makes this mandatory. Existing Mac notes were consolidated into MAC.md; Windows build instructions and the full PC continuation notes are in WINDOWS.md. Platform READMEs are pointers, not separate handovers. This change updates documentation and packaging references, not audio/UI behavior.
