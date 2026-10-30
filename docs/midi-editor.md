# MIDI editor - 0.16.0

Open a kit and choose **MIDI**. Use + Clip to add a clip, select another using the clip chooser, and type a name into the adjacent field. Save kit stores all clips alongside the instruments and effects. The whole kit also recalls with the DAW Set.

## Writing and editing notes

Double-click an empty row to add a note. Double-click a note to delete it. Drag the body to move it in time or to another instrument. Drag the right edge to change its duration. The next new note uses the last selected note's length. **B** toggles Draw: single-click adds/removes notes. **Alt** temporarily bypasses snapping when adding, moving or resizing.

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

Use the expression lane chooser for velocity, chance, velocity deviation or release velocity. Drag a marker vertically to edit the selected notes. The number controls apply to the selection, or to all notes when nothing is selected. Chance is the probability that a note plays on each loop. Velocity deviation varies its loudness around the chosen velocity. Release velocity is stored/exported, although Drum's sampler currently does not modulate its sound from release velocity.

Context-menu chance groups can play all grouped notes together, or choose one using their chance values as weights. Ungroup restores independent decisions. Group decisions are made per preview loop.

Choose a tool and press Apply: quantise starts or ends, legato, reverse, invert pitches, double/halve timing, duplicate, split at the grid, join notes, activate/deactivate, randomise velocity/chance, humanise timing, transpose, nudge, lengthen or shorten. Quantise uses Amount as strength. Randomisation and humanisation use Amount as depth. Tools affect the selection, or all notes if there is no selection. Ctrl+U quantises starts, Ctrl+E splits at the cursor, and Ctrl+J joins selected notes of each instrument.

## Import/export

Drag MIDI to the DAW or Explorer, or use Export. Export contains the current key assignments, timing, note lengths, velocities, release velocities, tempo, signature and full clip duration. MIDI files cannot store Drum's instrument identities, chance or velocity-deviation settings: export renders a stable realisation of those random values. The kit file retains the editable settings. Exported MIDI is not a live link to Drum.

Import reads tick-based `.mid`/`.midi` files into the current clip. It maps each note to the instrument currently assigned that MIDI key; unmatched keys are skipped and reported. Import replaces the current clip notes and can be undone. Drum does not create instruments automatically for unmatched keys.

This is Drum's own Notes editor. Its drum-oriented rows are limited to the kit's assigned instruments. It does not embed Live's editor, multi-track editing, tuning-system engine, MPE, automation or Max for Live tools.
