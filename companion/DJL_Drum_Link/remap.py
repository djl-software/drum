"""Remap Live's native MidiNoteVector in place; never delete/recreate notes."""


def clip_list(track):
    clips = []
    for slot in track.clip_slots:
        if slot.has_clip and slot.clip.is_midi_clip:
            clips.append(slot.clip)
    for clip in track.arrangement_clips:
        if clip.is_midi_clip and clip not in clips:
            clips.append(clip)
    return clips


def mapping(payload):
    return {item["id"]: item["note"] for item in payload["notes"]}


class ClipState:
    def __init__(self, assigned):
        self.assigned = dict(assigned)
        self.bindings = {}  # note ID -> [instrument ID, last pitch, orphaned, previous mute]

    def prepare(self, clip, assigned):
        notes = clip.get_all_notes_extended()
        reverse = {pitch: identity for identity, pitch in self.assigned.items()}
        bindings = {key: list(value) for key, value in self.bindings.items()}
        present = set()
        changes = []
        for note in notes:
            key = note.note_id
            present.add(key)
            binding = bindings.get(key)
            # A manual pitch edit reassigns the note to its current instrument.
            if binding is None or note.pitch != binding[1] or (binding[0] is None and note.pitch in reverse):
                binding = [reverse.get(note.pitch), note.pitch, False, bool(note.mute)]
                bindings[key] = binding
            identity, last_pitch, orphan, previous_mute = binding
            if identity is None:
                continue
            before = (note.pitch, bool(note.mute))
            if identity in assigned:
                note.pitch = assigned[identity]
                if orphan:
                    note.mute = previous_mute
                binding[1:] = [note.pitch, False, bool(note.mute)]
            else:
                if not orphan:
                    binding[3] = bool(note.mute)
                note.mute = True
                binding[2] = True
            if before != (note.pitch, bool(note.mute)):
                changes.append((key, note.pitch, bool(note.mute)))
        bindings = {key: value for key, value in bindings.items() if key in present}
        return notes, changes, bindings

    def apply(self, clip, assigned):
        notes, changes, bindings = self.prepare(clip, assigned)
        if changes:
            # Keeping the native vector and note IDs preserves per-note expression.
            clip.apply_note_modifications(notes)
            actual = {n.note_id: (n.pitch, bool(n.mute)) for n in clip.get_all_notes_extended()}
            if any(actual.get(key) != (pitch, mute) for key, pitch, mute in changes):
                raise RuntimeError("Live did not apply the complete note remap; stopped to avoid repeating a swap")
        self.bindings = bindings
        self.assigned = dict(assigned)
        return len(changes)


class TrackState:
    def __init__(self, payload):
        self.kit = payload["kitId"]
        self.assigned = mapping(payload)
        self.clips = {}
        self.failed = False
        self.track = None

    def sync(self, track, payload, song):
        if self.failed:
            raise RuntimeError("Remapping paused after a clip error. Disable/re-enable the Setup option after checking Live Undo.")
        assigned = mapping(payload)
        if self.track is not None and self.track != track:
            self.clips.clear(); self.assigned = assigned
        self.track = track
        clips = clip_list(track)
        self.clips = {clip: state for clip, state in self.clips.items() if clip in clips}
        # Opening a different kit establishes a new baseline, not a deletion of all old notes.
        if payload["kitId"] != self.kit:
            self.kit = payload["kitId"]; self.assigned = assigned; self.clips.clear()
        pending = []
        for clip in clips:
            if clip not in self.clips:
                self.clips[clip] = ClipState(self.assigned)
            state = self.clips[clip]
            if clip.is_recording:
                continue
            if state.assigned != assigned or not state.bindings:
                pending.append((clip, state))
        changed = 0
        if pending:
            song.begin_undo_step()
            try:
                for clip, state in pending:
                    changed += state.apply(clip, assigned)
            except Exception:
                self.failed = True
                raise
            finally:
                song.end_undo_step()
        self.assigned = assigned
        return changed
