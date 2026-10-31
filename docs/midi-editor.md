# MIDI editor - 0.16.4

Open a kit and choose **MIDI**. Use + Clip to add a clip, select another using the clip chooser, and type a name into the adjacent field. Save kit stores all clips alongside the instruments and effects. The whole kit also recalls with the DAW Set.

Clip, playback/loop and MIDI files are grouped in the left column. Grid/view and note editing tools sit above the piano roll. Note values sit below the expression lane. Each group has a labelled border.

All MIDI numeric boxes support vertical dragging and mouse-wheel adjustment, with Shift for finer movement. Double-click or press Enter while focused to type a value; Enter accepts and Escape cancels. Up/Down adjusts one step. A drag is one undoable edit.

## Writing and editing notes

Double-click an empty row to add a note. Double-click a note to delete it. Drag the body to move it in time or to another instrument. Drag the right edge to change its duration. New notes use the selected grid length, including triplets (1/32 creates a 1/32 note). With Grid off, new notes use the last selected note's length. **B** toggles Draw: single-click adds/removes notes. **Alt** temporarily bypasses snapping when adding, moving or resizing.

Click selects one note. Shift-click adds/removes notes from the selection; Ctrl-click adds to it. Drag a rectangle in empty space for marquee selection. Ctrl-drag duplicates notes. Ctrl+A selects all; Delete removes selected notes. The **Select...** menu supports inversion, matching instruments, inactive notes, chance/deviation conditions, every second/fourth note, and velocity/chance/duration/time ranges.

Ctrl+C/X/V copies, cuts and pastes within this editor; click an empty position to place the paste cursor. Ctrl+D duplicates notes. Ctrl+Z undoes and Ctrl+Shift+Z or Ctrl+Y redoes. Undo history is per current editing session and resets when changing clips. Up/Down move notes to adjacent kit instruments; Left/Right nudge them by the grid. Shift+Left/Right shortens/lengthens notes. **0** toggles note activation. Inactive notes remain editable but do not play or export.

## Grid, navigation and preview

Choose grid off or quarter through 64th notes; Triplet changes the subdivision. Ctrl+1 narrows the grid, Ctrl+2 widens it, Ctrl+3 toggles triplets, Ctrl+4 toggles snapping. Wheel scrolls vertically; Alt+wheel scrolls horizontally. Ctrl+wheel zooms horizontally at the pointer; Shift+wheel changes row height. Scrollbars provide both axes. The context menu includes Fit clip horizontally.

**Fold** shows only instruments containing notes. The root/scale choosers highlight scale pitches; they do not retune drum instruments. **Preview** auditions the corresponding drum when clicking its row or adding/selecting a note.

## Timing and loops

Clip beats sets the clip duration, in quarter-note beats. Reducing it removes notes beyond the end and trims crossing notes; Undo restores them. The time-signature chooser sets the bar ruler and exported signature. Loop start/end are also quarter-note beats. Drag the loop-bar ends above the piano roll to adjust them, or use their number controls.

Play starts local MIDI preview at the loop start (or at beat zero with Loop off). Loop repeats the selected region. With Loop off, playback stops at the clip end. Preview uses the header BPM and is independent of DAW transport; stop it before playing an exported clip to avoid doubled hits. MIDI edits restart preview; sound-only edits do not. Held generated notes are released when stopping or crossing a loop boundary.

Right-click for Set loop to selection, Duplicate loop, or time edits. A marquee defines a time range. Insert time inserts that length at the cursor/range start; Delete time removes it and closes the gap; Duplicate time inserts a copy after the range. Without a range, the current grid division is used.

## Velocity, chance and note tools

The **View** menu sits inline with the bottom note values. Tick velocity, chance, velocity deviation and/or release velocity to show several lanes together. Click outside the menu to close it. Use Hide/Show to collapse and restore the current lane selection. Untick lanes or choose Hide all to recover piano-roll space; hiding lanes does not remove their values. Drag the top border of each lane up/down to resize it. Each lane shows a numbered value axis, horizontal value lines and vertical time divisions aligned with the notes. The expression controls and compact value fields are grouped together under the chart. Each field shows its name on the left, number on the right, and a filled bar representing its value. Click a lane header or its empty background to select that lane; its header is highlighted and the notes display that lane's values where there is room. Click the selected header/background again to deselect it and remove note values. Editing a marker selects its lane. **Randomize**, immediately right of View, changes that lane for selected notes (or all notes when none are selected), within the **Random +/-** range. Chance uses percentage points; other lanes use MIDI values. Undo restores a randomization.

Drag a marker vertically to edit the selected notes. The number controls apply to the selection, or to all notes when nothing is selected. Chance is the probability that a note plays on each loop. Velocity deviation varies its loudness around the chosen velocity. Release velocity is stored/exported, although Drum's sampler currently does not modulate its sound from release velocity.

Context-menu chance groups can play all grouped notes together, or choose one using their chance values as weights. Ungroup restores independent decisions. Group decisions are made per preview loop.

Choose a tool and press Apply: quantise starts or ends, legato, reverse, invert pitches, double/halve timing, duplicate, split at the grid, join notes, activate/deactivate, randomise velocity/chance, humanise timing, transpose, nudge, lengthen or shorten. Quantise uses Amount as strength. Randomisation and humanisation use Amount as depth. Tools affect the selection, or all notes if there is no selection. Ctrl+U quantises starts, Ctrl+E splits at the cursor, and Ctrl+J joins selected notes of each instrument.

## Import/export

Drag MIDI to the DAW or Explorer, or use Export. Export contains the current key assignments, timing, note lengths, velocities, release velocities, tempo, signature and full clip duration. MIDI files cannot store Drum's instrument identities, chance or velocity-deviation settings: export renders a stable realisation of those random values. The kit file retains the editable settings. Exported MIDI is not a live link to Drum.

Import reads tick-based `.mid`/`.midi` files into the current clip. It maps each note to the instrument currently assigned that MIDI key; unmatched keys are skipped and reported. Import replaces the current clip notes and can be undone. Drum does not create instruments automatically for unmatched keys.

This is Drum's own Notes editor. Its drum-oriented rows are limited to the kit's assigned instruments. It does not embed Live's editor, multi-track editing, tuning-system engine, MPE, automation or Max for Live tools.


## Agreed compatibility target (31 October 2026)

Standard MIDI (`.mid`) is the agreed interchange format; Ableton Live Clip (`.alc`) import is outside this target. Use the Live 12 manual as the behavioural reference for separate Notes, Envelopes and MPE views. Pressure belongs to the MPE view, not the Notes lane selector.

The current 0.16.4 update changes Notes lanes only. Envelopes/MPE editing and lossless MIDI event import/export are still outstanding. The current importer reads notes matched to kit keys; it does not preserve controller events, channel assignment, tempo maps or MPE. Do not use it for a lossless expressive MIDI round trip yet.

Acceptance for the follow-on work must compare imported/exported event timestamps and values, channels, note-on/off velocities, overlapping notes, controller/program messages, pitch bend, aftertouch, tempo and signature events, and MPE channel relationships. Unknown or unsupported content must be retained or explicitly reported, never silently discarded. Test at multiple source tick resolutions and with multitrack files. Notes edits must keep associated per-note expression aligned. MIDI playback support must be tested separately from file preservation; retaining a controller does not guarantee the sampler responds to it.

References: [Notes](https://www.ableton.com/en/live-manual/12/editing-midi/), [Clip Envelopes](https://www.ableton.com/en/live-manual/12/clip-envelopes/), [MPE](https://www.ableton.com/en/live-manual/12/editing-mpe/), [MIDI and Live Clips](https://www.ableton.com/en/live-manual/12/managing-files-and-sets/#midi-files).


## Named instruments in Ableton Live

**Use kit** on the kit screen creates `Drum/Ableton Maps/DJL Drum Link.adg` and reveals it in the file browser. Put this MIDI Effect Rack **immediately before DJL Drum** on the same track, once per Drum instance. Keep its rack name and key zones unchanged. It contains 88 empty chains, one for each key from MIDI 21 to 108, with no extra instruments, samples or audio effects. Unoccupied keys show `(unused)`.

Install the bundled `DJL_Drum_Link` folder under your Ableton User Library's `Remote Scripts` folder. Restart Live, then select **DJL Drum Link** in an unused Control Surface slot under Settings > Link, Tempo & MIDI. Leave Input and Output as **None**. One script serves all Drum tracks. See [Ableton's installation instructions](https://help.ableton.com/hc/en-us/articles/209072009-Installing-third-party-remote-scripts).

The companion reads each Drum instance's current instrument names and assignments twice a second and updates only its immediately preceding DJL mapping rack. It stays connected with Drum's editor closed. It does not launch an external helper or access the network. An instance identifier in a host-visible plugin parameter (its value has no sound effect) prevents two tracks from sharing the wrong map; duplicated/restored instances get fresh identifiers automatically.

Open a Live MIDI clip and turn **Fold off**. The rack requests **Show Names in MIDI Editor**; if necessary enable that option in its chain-list context menu. Save the Live Set to keep the rack. Rack loading has been confirmed in Live 12.4.16. Version 0.16.6 also corrects visibility of the link identifier; live name synchronisation and remapping need an acceptance check after installing it.

Names follow additions, deletions, renames and reordering automatically. MIDI key zones remain fixed.

### Automatically update Ableton clips

On the kit screen, open **Setup** and tick **Automatically update Ableton clips**. The setting is off by default and saved with this Drum instance in the Live Set. Enable it before changing the kit order and allow the companion a second to connect. The current kit is the starting mapping; it cannot reconstruct edits made while the companion was off.

Subsequent instrument moves and insertions remap the linked track's existing Session and Arrangement MIDI clips by instrument identity. Simultaneous swaps are applied by note ID, keeping timing, duration, velocity, probability and per-note expression attached to the same notes. Unassigned pitches remain unchanged. There must be exactly one linked Drum instance on the track for clip remapping. Take-lane clips, clips on other tracks and exported `.mid` files are not changed.

Deleting an instrument mutes its notes, rather than making them trigger the instrument that closes the gap. Restoring that instrument identity during the same connected session restores its notes' previous mute state. Recording clips defer changes until recording finishes; avoid rearranging the kit while recording new notes. Opening a different kit, restarting the companion or enabling the option establishes a fresh baseline; it does not guess historic note identities.

Each update uses a Live undo step. If Live rejects or incompletely applies an update, remapping pauses and reports an error in Live's status bar. Check the clips/Live Undo, then turn the Setup option off and on to establish a fresh baseline. Disabling the option leaves clip notes where they are; automatic name updates continue. Drum's internal MIDI clips already retain instrument identities independently.

The native note-ID remapping path still needs an in-Live acceptance check after installing 0.16.6.

If the companion is disabled, the rack retains its last names and still passes notes on the assigned key range. Stale/malformed publications and edited racks containing effects are ignored. Do not stack mapping racks or insert another device between the mapping rack and Drum. No controller/MPE events or audio are processed by the companion.
