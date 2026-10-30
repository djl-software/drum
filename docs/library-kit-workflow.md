# Libraries and Kits - 0.15.0

A **Library** is a named collection of up to 88 instruments in up to 16 groups. Library tabs show the name followed by **LIB**. Up to eight libraries can be open in the shared collection; additional libraries can be saved as files and opened when needed.

A **Kit** is the selection played by one Drum instance on a DAW MIDI track. Only one kit is open per instance. Each instrument and its effect chain are independent copies: editing a kit never edits its source library.

## Build a kit

1. Load Drum on a MIDI track. Choose **New kit**, or **Open kit** for an existing `.drumkit` file.
2. Give the kit a name. Open **Libraries**, select a library, group and instrument, then press **Add**. If no kit is open, Drum prompts you to create or open one first.
3. The instrument joins the group with the same name, creating that group if necessary. Matching group names from different libraries are combined. New groups are appended; groups can be moved with Up/Down.
4. Keyboard keys are consecutive from A0 / MIDI 21, following group order and instrument order. Adding to an earlier group shifts later groups. Check existing MIDI clips after changing the order.
5. Edit each sound and its chain as usual. Sidechain sources reset on import because the source instrument may not exist in this kit; select a kit-local source afterwards.
6. Use **Save kit** or **Save kit as**. Load the same file in other instances for independently editable copies, or use different kits on each track. Opening or creating a kit replaces the current one after confirmation, so save edits first.

Live MIDI still comes from the track containing that Drum instance. The library keyboard auditions sounds without redirecting incoming track MIDI.

## Portable folders

The configured main location on this PC is:

`D:\DJL\OneDrive - DJ Leslieson\Abelton\Drum`

- `Libraries/collections.xml`: shared open libraries; **Save all** updates these.
- `Libraries/*.drumlib`: named library files; **Save library as** exports the selected library, and **Open library** opens it.
- `Kits/*.drumkit`: saved kits, including groups, controls, macros and chains.
- `Samples/`: collected source audio, shared between saved files where contents match.

Save operations collect the sample audio and write relative paths. Move or back up the **whole Drum folder**, including Samples. Saving to another location uses a Samples folder beside the document's containing folder; keep the same Libraries/Kits/Samples arrangement. Missing samples prevent a portable save without replacing the previous document.

DAW project state also saves the open kit. It does not automatically collect files: use Save kit first for a portable copy. Shared library drafts are retained in the instance state, and library windows attempt to save shared edits on closing. Conflicting changes to one library from separate instances are rejected; changes to different libraries can merge.

Third-party VST3 modules, licences and any external files managed internally by those effects are not bundled. Install those plugins separately on another PC. Built-in effects use the same saved format across platforms. A future tablet version will not host external VST3 effects; portable storage does not make a VST3 chain executable there.

The selected root is stored in `%APPDATA%/DJL/Drum/data-root.txt`; without a configured root, Drum uses Documents/DJL/Drum. The folder structure and XML data are platform-neutral.

## Older sessions

An old session's active collection opens as the kit with its groups preserved. Other old collections are retained for migration into the shared libraries. A 0.14 kit that was already flattened cannot recover its original groups automatically; new additions retain their source group.

## Patterns and MIDI export

Choose **Patterns** from the Kit page. Each kit can store sixteen named patterns. The initial editor is a sixteenth-note step sequencer with one, two or four bars in 4/4.

- Click a cell to add or remove a hit. Shift-drag vertically on a cell to change velocity (1-127).
- Choose the pattern length and name. Reducing length removes hits beyond the new end.
- **Play pattern** loops the pattern at the header BPM; **Stop pattern** sends note-offs. This is a local audition, independent of DAW transport. Stop pattern audition before playing an exported clip to avoid doubling the hits. Editing the pattern or changing its key map restarts its audition from the beginning; sound-control changes do not.
- **Drag MIDI** drags a standard MIDI file to the DAW or Explorer. **Export MIDI...** saves one manually. Drag files remain in the Drum/MIDI folder.
- Hits reference instrument identities. Reordering sounds or groups updates playback and the next export to use the correct current notes. Removing an instrument removes its hits from Drum patterns.
- Patterns are saved inside the kit and DAW state, including velocities. Export includes the tempo, 4/4 signature and exact loop length. Gate length is 90% of a sixteenth note in this initial editor; Trigger-mode drum sounds still play their full sample.

An exported clip belongs to the DAW and is an independent copy. After changing the kit mapping, replace that clip with a fresh export. A DAW may import the MIDI file's tempo or keep its own tempo. Direct updates of existing Ableton clips, and the Setup checkbox **Automatically update Ableton clips**, are deferred with the companion integration. No automatic clip updates are claimed by this build.

The editor and file format are shared by VST3 and desktop standalone builds. The tablet application and a free-time piano-roll editor remain future work.
