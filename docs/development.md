# Development

The build foundation follows Meter: CMake 3.22+, C++20, JUCE 8.0.12, a static MSVC runtime, a VST3 target with an optional standalone development harness, and CTest processor tests.

## Sound editing (0.5, 28 October 2026)

`SoundParameters.h` defines scalar editing fields and metadata shared by validation, schema-5 persistence and the tabbed `SoundEditor`. Legacy schemas 1–4 retain neutral settings. The worker computes waveform peaks, trim/loop boundaries, zero-crossing snapping and normalisation. Rendering uses preallocated voices with three ADSR envelopes, two cascaded stereo TPT filter sections, per-voice LFOs, bounded loops and MIDI-channel pitch bend. Markers/mode/direction/loop are latched at note-on; other edits update voices without changing their playback positions. Disabled LFOs bypass their volume depth, and muted previews cannot release unrelated held sounds.

The processor declares Main plus 15 optional stereo output buses and six optional send buses. It derives buffer offsets from enabled buses, including a mono Main configuration. Sixteen stable `macro1`–`macro16` parameters provide host automation. The audio thread applies instrument-specific mappings to immutable bank settings at block boundaries; gain/balance/base-pitch ramps remain five milliseconds. Other modulation edits are not guaranteed click-free. All variable storage is allocated before processing, and no model locks or file operations occur in the callback. Normalise uses double-precision gain to avoid overflowing on very small floating-point samples.

`EditingTests.cpp` checks trimming/reverse, Gate and Classic release, bidirectional loops, ADSR, voice limits/retrigger, normalise, velocity, pitch envelope/bend, filter response/stability, LFO bypass, muted-preview isolation, macro updates, mono/aux/send offsets, schema migration/rejection, slicing and editor callbacks. The hosted VST3 test also exercises gate note-off, real bus routing and host macro automation. All six page snapshots and the 800×600 layout were inspected. Windows Release VST3/standalone builds and both CTest suites passed; Steinberg validation passed 47/47. Native Live trim/recall/audition evidence and the pending final installed-module update are recorded in `ableton-testing.md`. Native macOS and broader host performance testing remain outstanding.

See `sound-editor.md` for usage, routing semantics, automation scope and the remaining features needed for complete Ableton parity. The sections below preserve validation history for earlier previews.

## Layout

- `src/`: audio processor and editor. Add DSP and parameter/state handling here.
- `assets/`: embedded DJL branding.
- `tests/`: processor, state and editor smoke tests.
- `scripts/`: Windows and macOS build entry points and Windows development packaging.
- `docs/`: development and dependency notes.
- `build/`, `build-macos/`, `dist/`, `.local/`: generated files, excluded from Git.

Drum has its own bundle ID `com.djleslieson.drum` and four-character plugin code `DjDr`, distinct from Meter. The manufacturer code remains `DjlS`.

The processor is a MIDI instrument with no audio input and mono/stereo audio output. `IS_SYNTH`, MIDI input and the `Instrument|Drum` VST3 category agree with that layout. The kit/group/instrument model and editor follow the [product specification](product-specification.md). Version 0.3 added asynchronous sample loading and MIDI playback; 0.4 adds per-instrument volume, balance, tuning, mute, solo and choke groups.

VST3 builds by default. `DRUM_BUILD_STANDALONE=ON` enables the optional standalone development harness; a separate product edition comes later.

Windows uses Visual Studio 2022 x64. The macOS script targets universal arm64/x86_64 with Xcode and macOS 11+. Native macOS validation is still required. Packaging creates development artifacts only; installer/signing/release configuration can be added when product requirements are known.

Tests cover MIDI instrument identity, silent empty-engine output at 44.1/48/96 kHz, mono/stereo, zero/short/normal blocks, unsupported audio inputs and outputs, state round trip, malformed state and editor construction. DAW loading and audio-device runtime validation remain separate checks.

## Instrument scaffold validation — 26 October 2026

Windows x64 Release build passed using Visual Studio 2022, JUCE 8.0.12 and the default VST3-only configuration. CTest passed 1/1 processor suite. Steinberg VST3 SDK validator 3.8.1 passed 47 tests with zero failures. The generated module manifest reports `Instrument|Drum`.

These checks validate the scaffold and host interface. They do not validate sample playback, kit management, Ableton session behaviour, or the optional standalone/macOS builds.

## Management model and editor

`KitLibrary` owns a versioned library snapshot behind a lock for UI edits and DAW state operations. Edits apply to a candidate snapshot and publish only after validation. The audio callback uses a separately prepared sample bank and an atomic active-kit epoch; it never reads the locked management snapshot.

The editor polls model and sample-status revisions on the message thread so host restores and completed loads refresh an open window. Management edits and successful sample assignments notify the host that non-parameter state changed. Schema 4 preserves all kits, absolute sample paths and instrument controls. Schemas 1–3 migrate with neutral controls. Sample audio is not embedded.

Management tests exercise capacities, note uniqueness, identity preservation, copying/moving/removal, malformed state, legacy migration, full processor recall and UI callbacks. Layout snapshots cover normal, empty, minimum-size and dense libraries.

### Management validation — 26 October 2026

Windows Release builds of the VST3 and optional review app passed. The expanded CTest suite passed, including direct keyboard entry into the name editor, UI callbacks and round-trip state restoration. Steinberg validator 3.8.1 passed 47 tests with zero failures. Actual editor renders were inspected at 1280×900 and 1080×860, including empty and full 8-kit/16-group/88-instrument states. The review app was opened and example-kit creation, group selection and cancellation of the group-removal confirmation checked in the live window.

Audio generation, sample loading, host automation of sound controls and native macOS validation remain outside this management preview.

## Sample playback implementation (0.3)

`SampleEngine` owns a loader thread, decoded sample cache and immutable active-kit banks. File access, decoding, allocation and reclamation happen on the worker. An atomic published pointer and a single-reader hazard pointer keep banks alive across audio callbacks and sounding voices. The render callback uses 128 fixed voices, raw MIDI event bytes and linear sample-rate conversion, without locks, file access or dynamic allocation. Sample changes stop existing voices at the next bank publication; kit changes and state restores also invalidate the old bank immediately, keeping it silent while the new bank loads. Naming and organisational changes that preserve the bank do not interrupt audio.

Hits respond to MIDI note-on velocity on all channels, at the supplied sample offset. Note-off and velocity-zero note-on do not stop a one-shot. CC 120/123 stop that channel's voices; the UI Stop button stops all voices. The oldest voice is stolen when all 128 are busy. Stereo is preserved, mono is duplicated for stereo output, and stereo is averaged for mono output. A 2 ms end ramp reduces discontinuities at trimmed sample ends. There is no limiter; simultaneous hits can exceed full scale.

Files must be mono/stereo, 8–384 kHz, at most 30 seconds and 64 MiB decoded. Invalid replacements preserve the previous assignment. The active bank accepts at most 512 MiB of unique samples; entries over that limit retain their paths but are silent with an error. The request queue allows eight outstanding loads. Saved absolute references reload on project recall; Replace relinks missing files. Automatic relocation, sample collection, undo and offline-render loading synchronisation remain future work. Allow the Ready status before exporting.

Playback tests cover timing, velocity, overlap, resampling duration, stereo/mono output, audition and panic, invalid/oversized files, switching kits, missing-file recovery, saved-state recall and the voice limit. A separate CTest case loads the built VST3 through JUCE's host, triggers a sample, saves its wrapper state and verifies playback in a new instance. This does not substitute for an Ableton Live session test.

### Playback validation — 26 October 2026

Windows x64 Release VST3 and standalone harness built successfully. CTest passed both suites (2/2), including compiled-VST3 sample output and session recall. Steinberg VST3 SDK validator passed 47 tests with zero failures. Actual editor renders were inspected at normal and minimum sizes. In the running standalone harness, the native Load sample picker loaded a stereo WAV, displayed its filename and duration, changed to Replace and enabled Play. Native Ableton playback/recall and macOS remain unverified.

## Instrument controls and import (0.4)

Render banks now include immutable controls and a sample-generation counter. Control-only publications retain the same samples and generation, allowing voices to keep playing across bank changes. Their buffers remain owned by the new bank. Volume/balance/pitch targets ramp over 5 ms per voice; sample position is preserved. New hits start at the current settings. Mapping/sample changes still stop old voices. A 2 ms fade is used for mute/solo suppression and choke; CC panic and UI Stop remain immediate. Choke assignment is kit-wide and independent of MIDI channel or organisational group. Muted/solo-suppressed triggers cannot choke other voices.

Volume uses dB gain with the minimum silent; stereo balance attenuates the opposite channel and preserves unity at centre. Tuning multiplies the resampling step by `2^(semitones/12)` and changes the reported tail. Multiple solos are additive, with mute taking priority. Controls are snapshot/state values, not host automation parameters yet.

The worker processes one queued import per pass, with at most 88 requests outstanding. New instruments are created only after successful decoding, using one validated model transaction for name, free MIDI note and path. Single-card replacement preserves controls. Bulk drops create instruments in the selected group and reject a selection that exceeds remaining kit capacity. Invalid or unsupported files do not create slots. Files remain external references. Importing Live device presets is outside the audio-sample workflow requested by the user.

`ControlTests.cpp` checks gain and balance, tuning duration, smoothing without retriggering, mute/solo across groups, no resurrection after unmute, choke timing and isolation, self-choking, muted-trigger behaviour, copying, persistence, legacy migration, invalid values, editor callbacks and bulk drop behaviour. Hosted VST3 tests also save and restore non-default controls and verify the resulting channel levels. Native macOS validation and automated DAW parameter lanes remain pending.

The editor scales its workspace below 1080×860 and permits resizing down to 800×600. Initial dimensions are bounded using the primary display work area; the host controls window position. Compact, normal and dense snapshots were inspected. Windows Release builds, both CTest suites and all 47 Steinberg checks passed. Live 12 Suite browser drops were verified with Core Library WAV and AIFF files, including card replacement and a native volume edit. See `ableton-testing.md` for the separate unreadable AIFF browser entry and the remaining host-testing boundary.
