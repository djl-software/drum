# Drum product specification

Initial design recorded 26 October 2026. This document distinguishes confirmed requirements, proposed interaction rules and outstanding scope decisions. It describes the intended product; the current build includes management and an initial interface, with sample playback still pending.

## Purpose and release order

Drum replaces the drum-rack workflow in Ableton Live and other VST3-capable DAWs, presenting instruments and their controls more clearly. VST3 is the first delivery format. A separate standalone edition follows later.

## Confirmed structure

| Level | Requirement |
| --- | --- |
| Plugin instance | Up to 8 user-created kits, each with an editable name |
| Playback | One active kit at a time; switching kits changes the playable kit |
| Kit | Up to 88 instruments, divided among 1–16 organisational groups |
| Group | Variable number of instruments; groups share the kit's 88-key capacity |
| Instrument | Consistent controls and capabilities, using the standard drum-rack workflow as the reference |

Eight kits can therefore store up to 704 instrument definitions, with at most 88 instrument assignments in the active kit. Instrument count is distinct from polyphony: several hits may overlap on the same instrument. Voice limits will be designed separately.

Groups organise instruments and their key ranges. A choke assignment is an independent playback control; grouping instruments together must not make them cut one another off.

## Proposed mapping and editing rules

These are design proposals, not additional user-confirmed requirements:

- Show an 88-key overview with group colours, group boundaries, instrument names, and MIDI note numbers.
- Initially use consecutive notes for each group, allocating its range from the number of instruments it contains. Do not divide 88 equally by the number of groups. For example, groups containing 4, 6 and 3 instruments occupy 4, 6 and 3 consecutive assignments; 75 slots remain available.
- A conventional piano range, MIDI notes 21–108, is a candidate default. The starting note and ability to remap are still to be decided; 88 instruments is a capacity limit, not a restriction imposed by MIDI.
- Preserve existing note assignments during ordinary organisation edits. Offer an explicit remapping action when compacting or changing ranges, showing which assignments will change so existing DAW patterns are not silently altered.
- Use stable internal IDs for kits, groups and instruments. Renaming and reordering must not break saved parameters or automation.
- Allow empty groups while constructing a kit, but retain at least one group. Reject a ninth kit, seventeenth group or eighty-ninth instrument without losing existing work.
- Persist all kits, memberships, mappings, settings and the active kit in the DAW project. Sample references and missing-file recovery need a defined policy.
- Define how sounding voices finish on a kit change before implementing live switching; the previous kit must not receive new triggers after the switch.

## Proposed interface

Keep the hierarchy visible: kit selector across the top, groups at the left, the selected group's instruments in the main area, and a consistent instrument editor beside or below them. Keep the full keyboard overview available to explain where every group is mapped. Display counts such as `32 / 88 instruments` and `5 / 16 groups`.

Instrument selection should reveal its controls in place. Group selection should provide an overview without unexpectedly changing the active kit. Include clear active-kit and selected-instrument indicators, and audition controls.

## Functional reference checklist

The user's requirement is equivalent drum-rack capabilities with clearer presentation. A sampled pad also has instrument controls, so the comparison covers both the rack and its sound source. This checklist is a research baseline, not a claim of implemented parity.

Rack-level reference:

- Naming/colour, sample or instrument assignment, audition, copying and reordering.
- Incoming note assignment and played-note mapping.
- Instrument activation/mute, solo, level and pan.
- Independent choke assignment, including 16 choke groups.
- Per-instrument sends, up to six return chains, and output routing.
- Layered chains and instrument/effect processing order.
- Up to 16 macros, parameter mappings and variations.

Reference: [Ableton Live 12 Racks manual](https://www.ableton.com/en/live-manual/12/instrument-drum-and-effect-racks/).

Sample-instrument reference:

- Sample loading/replacement, waveform, start/end or length, reverse and looping.
- Trigger/gate behaviour, envelopes, tuning and velocity response.
- Filter and modulation controls.
- Voice/retrigger behaviour, slicing and tempo-related playback.
- Compare both Drum Sampler and Simpler; their controls differ. Layering/multisampling and playback effects need explicit scope decisions.

Reference: [Ableton Live 12 instrument manual](https://www.ableton.com/en/live-manual/12/live-instrument-reference/).

The implementation must define the source engine and effects supplied by Drum. Loading third-party plugins, importing Live rack presets, and integrating directly with Live's native devices are separate capabilities requiring investigation; equivalent operation is not evidence of file or device compatibility. VST3 routing must also be checked in each target host.

## Implementation sequence

1. Capture the brief and configure a VST3 MIDI instrument (complete).
2. Implement and test the kit/group/instrument state model and mapping rules; create a navigable interface prototype (implemented in 0.2 for review).
3. Add sample loading and sample-accurate playback, voice management and the common instrument controls.
4. Extend mixing, choking, effects, routing and automation against the reference checklist.
5. Validate session recall, host compatibility, performance and usability; add the separate standalone edition later.

## Decisions still open

The confirmed capacities and single-active-kit behaviour are settled. Before sample-engine and mapping implementation, resolve the default note range, remapping workflow, source engine/features (Drum Sampler, Simpler or a combined set), effects/plugin hosting, and whether group audio buses are required in addition to organisational grouping.

## Initial management implementation (0.2)

The first interface uses MIDI notes 21–108 and labels MIDI 60 as C4. This convention is visible alongside numeric notes; DAWs may use different octave labels. New instruments take a free key, preferably after the previous instrument in the same group. Adding to an existing group, moving between groups and changing display order preserve other assignments, so a group can acquire non-contiguous keys. The key map shows actual assignments. A dedicated range-compaction workflow remains a future design decision.

The library opens with one empty kit and one group. The optional example adds a separate Studio Kit with four groups and twelve empty instrument slots. Copies receive fresh IDs; copied instruments/groups receive unused notes in the same kit, while a copied kit preserves its internal note map. Instrument counts include empty slots.

Saved state contains stable IDs, names, group colours, instrument notes, order and the active kit. State is validated before replacement; invalid documents preserve the current library. The original empty scaffold state migrates to one empty kit. Selection of the editor's group/instrument is local UI state and resets when reopening the editor.

Organisational groups and instruments can be reordered with explicit buttons. Deletion retains at least one kit and one group per kit. The UI confirms removal of a kit, group or instrument. There is no undo history yet.
