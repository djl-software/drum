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

## MIDI editing and export (0.16.0)

Choose **MIDI** from the Kit page. Saved sequences are called **clips**. The step grid from 0.15 is replaced by a piano roll; existing clips migrate without changing their sound assignments. See [MIDI editor controls](midi-editor.md).

Create collections with **New library** directly from the main screen, even with no kit open. The library window also has a clearly labelled **New library** button. New collections are named Library 2, Library 3, etc., and can be renamed in the Library field. Use Save all to save the shared collections, or Save library as for an individual portable file.

The MIDI editor stores up to sixteen clips per kit, up to 8192 notes and 1024 quarter-note beats per clip. It supports free note placement/lengths, selection, clipboard operations, undo/redo, velocity/chance/release editing, loops, and note-editing tools. Drum clips follow instrument identities when the kit order changes. Removing an instrument removes its MIDI notes.

An exported clip belongs to the DAW and is an independent copy. After changing the kit mapping, replace it with a fresh MIDI export. The planned Setup checkbox **Automatically update Ableton clips** remains deferred with the companion integration. This build does not rewrite existing DAW clips.

The editor and saved format are shared by VST3 and desktop standalone builds. MPE, clip automation and Live's MIDI generators are outside the Notes-editor scope chosen for this update. The tablet application remains future work.
