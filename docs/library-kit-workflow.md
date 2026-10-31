# Libraries and Kits - 0.16.10

A **Library** is a named collection of up to 88 instruments in up to 16 groups. Library tabs show the saved filename without its extension. An unsaved edit adds **\*** on the right. Up to eight libraries can be open in the shared collection; additional libraries can be saved as files and opened when needed.

A **Kit** is the selection played by one Drum instance on a DAW MIDI track. Only one kit is open per instance. Each instrument and its effect chain are independent copies: editing a kit never edits its source library.

## Build a kit

1. Load Drum on a MIDI track. Choose **New kit**, or **Open kit** for an existing `.drumkit` file.
2. Give the kit a name. Open **Libraries**, select a library, group and instrument, then press **Add**. If no kit is open, Drum prompts you to create or open one first.
3. The instrument joins the group with the same name, creating that group if necessary. Matching group names from different libraries are combined. New groups are appended; groups can be moved with Up/Down.
4. Keyboard keys are consecutive from A-1 / MIDI 21, following group order and instrument order. Adding to an earlier group shifts later groups. Check existing MIDI clips after changing the order.
5. Edit each sound and its chain as usual. Sidechain sources reset on import because the source instrument may not exist in this kit; select a kit-local source afterwards.
6. Use **Save kit** or **Save kit as**. Load the same file in other instances for independently editable copies, or use different kits on each track. Opening or creating a kit replaces the current one after confirmation, so save edits first.

Live MIDI still comes from the track containing that Drum instance. The library keyboard auditions sounds without redirecting incoming track MIDI.

## Portable folders

The configured main location on this PC is:

`D:\DJL\OneDrive - DJ Leslieson\Abelton\Drum`

- `Libraries/collections.xml`: legacy shared collections, loaded initially for compatibility.
- `Libraries/*.drumlib`: named library files; **Library > Load** opens one, and the tab menu saves it.
- `Kits/*.drumkit`: saved kits, including groups, controls, macros and chains.
- `Samples/`: collected source audio, shared between saved files where contents match.

Save operations collect the sample audio and write relative paths. Move or back up the **whole Drum folder**, including Samples. Saving to another location uses a Samples folder beside the document's containing folder; keep the same Libraries/Kits/Samples arrangement. Missing samples prevent a portable save without replacing the previous document.

DAW project state also saves the open kit. It does not automatically collect files: use Save kit first for a portable copy. Open library tabs, file associations and unsaved drafts are retained in the instance state. Closing the plugin window does not silently save library files. Use Close to choose whether to save or discard edits.

Third-party VST3 modules, licences and any external files managed internally by those effects are not bundled. Install those plugins separately on another PC. Built-in effects use the same saved format across platforms. A future tablet version will not host external VST3 effects; portable storage does not make a VST3 chain executable there.

The selected root is stored in `%APPDATA%/DJL/Drum/data-root.txt`; without a configured root, Drum uses Documents/DJL/Drum. The folder structure and XML data are platform-neutral.

## Older sessions

An old session's active collection opens as the kit with its groups preserved. Other old collections are retained for migration into the shared libraries. A 0.14 kit that was already flattened cannot recover its original groups automatically; new additions retain their source group.

## MIDI editing and export (0.16.0)

Choose **MIDI** from the Kit page. Saved sequences are called **clips**. The step grid from 0.15 is replaced by a piano roll; existing clips migrate without changing their sound assignments. See [MIDI editor controls](midi-editor.md).

Create collections with **Library > New**, even with no kit open. A new library has an unsaved tab; choose its filename on its first Save.

The MIDI editor stores up to sixteen clips per kit, up to 8192 notes and 1024 quarter-note beats per clip. It supports free note placement/lengths, selection, clipboard operations, undo/redo, velocity/chance/release editing, loops, and note-editing tools. Drum clips follow instrument identities when the kit order changes. Removing an instrument removes its MIDI notes.

An exported clip belongs to the DAW and is an independent copy. After changing the kit mapping, replace it with a fresh MIDI export. With the Live companion connected, **Setup > Automatically update Ableton clips** can follow subsequent instrument moves in the linked track's Session and Arrangement clips. Enable it before editing the kit. Other DAWs and exported files remain independent; see `midi-editor.md` for scope and deletion behaviour.

The editor and saved format are shared by VST3 and desktop standalone builds. MPE, clip automation and Live's MIDI generators are outside the Notes-editor scope chosen for this update. The tablet application remains future work.


## Toolbar and screen navigation (0.16.8)

The kit screen starts with **Update**, **Kit** and **Library**. Update refreshes the current kit's Live connection. Kit opens a conventional dropdown with New, Load, Save and Save as. Library on the Kit screen opens the Library page directly, without a dropdown. On the Library page, the Library dropdown provides New/Load/Save actions. Library takes the place of the kit screen within the same Drum window, using the same group, pad, editing and keyboard layout. **Close**, immediately right of Library, returns to the kit. If any libraries have unsaved changes, choose Save all, Quit without saving, or Cancel. Save all asks for filenames where needed; cancelling or a save error keeps the Library screen open.

Drag a pad onto another pad in the current group to move it to that position. The intervening pads shift and MIDI keys are reassigned contiguously. The marker above the target pad shows the drop position. For another group, use the existing destination-group control. Instrument settings, effects and internal MIDI identities stay with the moved pad. External clip remapping still requires the Live companion and its enabled Setup option.

**Kit > Save** saves to `<kit name>.drumkit`, in the current kit's folder or the portable Kits folder for a new kit. Renaming the kit changes the next Save filename and leaves the previous saved file in place. Replacing another existing file asks first. Save as still allows choosing a destination.

The MIDI toolbar button is temporarily hidden. Stored MIDI clips and the editor implementation are retained.

## Library tabs (0.16.10)

The brighter Libraries heading identifies the screen. Tabs occupy the row below Library and Close, replacing the editable library-name field. Each tab reserves space for the complete filename and an unsaved marker; the row scrolls horizontally when needed. Up to eight libraries can be open.

Right-click a tab for **Save**, **Save as**, **Duplicate** or **Close**. Save is disabled when there are no changes. Save as writes a new filename and updates the same tab, leaving the old file intact. Duplicate immediately opens a separate unsaved copy. Close removes only the open tab, never the file; unsaved changes offer Save, Close without saving or Cancel. There is no Remove action. Closing the final tab leaves an empty screen with New/Load available.

Quit without saving restores saved libraries and closes new unsaved copies. Reopening a library already open from the same path selects its tab; a conflicting unsaved version must be saved or closed first. Save your Live Set to retain the open tabs and their drafts across restarts.

## Library Groups panel (0.16.11)

Use the large **+** beside GROUPS to add a group. The group count is at the bottom of the panel. Click a group row to select it; click its dropdown arrow (or right-click the row) for Rename, Duplicate or Delete. Rename opens a name field; Duplicate selects the new copy. Delete requires confirmation because the group and its instruments are removed and remaining keyboard assignments shift. The final group cannot be deleted, and adding/duplicating observes the 16-group and 88-instrument limits. The old bottom name field and group buttons are removed from the Library screen.
