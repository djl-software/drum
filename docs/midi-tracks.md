# MIDI tracks and the shared kit library (0.14.0)

Kits are reusable collections of drum sounds and their effects. A MIDI track holds independent copies of selected sounds, with its own keyboard order. The global library supports eight kits, each with up to sixteen groups and eighty-eight instruments; each track supports eighty-eight sounds in one flat collection.

## Create a track

1. Create a MIDI track in Ableton and load Drum. Drum does not create Live tracks automatically.
2. Name the Drum track, for example **Kicks**.
3. Open **Kit library** or **Add from kits**. Select a kit, group and instrument, then press **Add to track**. Repeat for each sound you need.
4. Close the library and play the track's assigned keys. Sounds are assigned consecutively from A0 / MIDI 21. Adding, deleting or reordering pads shifts later keys, so review existing MIDI clips after reorganising sounds.
5. Repeat with another Drum instance on separate Live tracks for **Snares and claps** and **Percussion**. Each instance receives MIDI from its own Live track; identical note numbers on different tracks can play different sounds.

The library keyboard is for organising and auditioning collections. Selecting a different library kit does not change the sounds triggered by the Live track. Library audition audio is mixed into that instance's main output.

## Copies and effects

Adding a sound copies its sample reference, editing controls and ordered effect chain. Subsequent track edits do not change the source kit or other track instances. Each copy has its own effect instances and saved settings. Third-party effects still need to be installed and licensed.

Sidechain sources are reset to **No Input** when importing a sound: the source instrument from the library may not exist on the destination track. Choose the appropriate local source after adding the sounds you need. Track macro values remain local to the track.

## Saving

- **Save library** writes shared collections to `%APPDATA%\DJL\Drum\Library\kits.xml`. Closing the library also attempts to save its edits. Another open library view can use **Reload library** to see the latest saved collections.
- Saving the Live Set stores that instance's track sounds and effects. Reopening the Set does not replace the shared library with an older snapshot. Unsaved library drafts are also retained in the instance's saved state.
- Changes to different kits from separate instances can merge. Conflicting edits to the same kit are rejected rather than overwriting another instance's changes. Reload discards the local draft only after confirmation.
- Audio is referenced by path, not embedded or collected. Keep samples in a stable location and back up both samples and the library file.

## Existing projects

On opening an older Drum state, the active kit becomes the instance's MIDI track, retaining the flattened pad order. Drum also attempts to import the original kits into the global library. If the library is full or a kit conflicts with a shared version, the original kits remain embedded in that instance's project state. The library status reports this; keep the original Set until the collections have been verified. Opening Kit library retries the import.

The earlier PDF manual predates this separation; use this guide for the current library/track workflow.
