# Testing Drum 0.3 in Ableton Live

This is a Windows x64 development VST3, with sample loading and one-shot MIDI playback. It is ready for host testing, not a commercial release.

## Install and load

Copy the complete `DJL Drum.vst3` bundle from `build/DJLDrum_artefacts/Release/VST3/` into a VST3 location enabled in Live. On this machine, Meter already uses the custom folder `D:\DJL\OneDrive - DJ Leslieson\Abelton\Plug-Ins\vst3`. Drum's matching location is `DJL\Drum\DJL Drum.vst3` below that folder.

1. In Live's Settings/Preferences, open Plug-Ins and rescan the enabled VST3 folder. If it is not detected, hold Alt while clicking Rescan for a full scan.
2. Find **DJL Drum** in Plug-Ins and place it on a MIDI track.
3. Open the editor, create an instrument (or select a slot in Add example kit), and click **Load sample**. You can also drop one audio file onto its card.
4. Wait for **Ready**. Use **Play** to audition it.
5. Assign **MIDI 36** for an easy first test. Drum calls this **C2**; Live calls it **C1**. The numeric MIDI note is the same. New empty slots initially start at MIDI 21, so their default keys may be below your usual drum pattern.
6. Draw the matching note in a MIDI clip, or arm the track and play it from your controller. Vary velocity and repeat notes to check overlapping hits. Live's track needs to be active and routed to your audio output.
7. Add another sample on a different note, then another kit. Switching kits should silence the previous kit and use the new assignments.
8. Save the Live Set, close and reopen it, then check names, groups, note assignments, active kit and sample playback.

Official setup reference: [Ableton: Using VST plug-ins on Windows](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows).

## What to expect

- Mono/stereo WAV, AIFF, FLAC, OGG and MP3; maximum 30 seconds and 64 MiB decoded per file, 512 MiB of unique decoded data in the active kit.
- Velocity-sensitive one-shot playback on every MIDI channel. Note-off does not stop a hit. Up to 128 voices; Stop clears them all.
- One stereo output, with mono also supported. Adjust overall level on Live's track; individual gain/pan and other sound controls come next.
- Loading or replacing a sample changes the active bank and cuts existing voices. Renaming or moving an instrument within the kit preserves its MIDI assignment and does not cut audio.
- Samples stay at their original absolute paths. Keep those files available. Use **Replace** to relink a missing file. Live's Collect All and Save is not a Drum sample-collection mechanism.
- Wait for Ready after loading a Set or switching kits before recording/exporting. Offline-export loading synchronisation is not implemented yet.

The example kit supplies names and layout only. No commercial sample library is bundled. This preview does not yet have envelopes, tuning, choke groups, effects, multiple outputs or automation for sound controls.

## Validation boundary

Automated tests exercise the processor and compiled VST3, including MIDI-triggered output and recall into a fresh hosted instance. Steinberg's validator checks the VST3 interface separately. Native Ableton session, controller and audio-device testing still need to be completed using the checklist above.

On 26 October 2026, version 0.3.0 was installed in the custom location above. The installed x64 module matched the tested build, SHA-256 `42b7c8f00376abebaf05b3f904e4023017ded27ed5baa6cb81d81563d261b50a`. CTest passed 2/2 and Steinberg validation passed 47/47. The native file picker also loaded a WAV successfully in the development harness.
