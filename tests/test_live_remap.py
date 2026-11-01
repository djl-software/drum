import copy
import importlib.util
from pathlib import Path
from types import SimpleNamespace as Item
import unittest
spec=importlib.util.spec_from_file_location("remap",Path(__file__).resolve().parents[1]/"companion/DJL_Drum_Link/remap.py")
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)

class Clip:
    def __init__(self,pitches):
        self.notes=[Item(note_id=i,pitch=p,mute=False,velocity=90,duration=.3,probability=.6,expression=[1,2]) for i,p in enumerate(pitches)]
        self.is_midi_clip=True;self.is_recording=False;self.calls=0;self.ignore=False
    def get_all_notes_extended(self):return copy.deepcopy(self.notes)
    def apply_note_modifications(self,notes):
        self.calls+=1
        if not self.ignore:self.notes=copy.deepcopy(notes)
class Song:
    def __init__(self):self.depth=0
    def begin_undo_step(self):self.depth+=1
    def end_undo_step(self):self.depth-=1

def payload(a=21,b=22):return dict(kitId="kit",notes=[dict(id="kick",note=a),dict(id="snare",note=b)])
def track(session,arrangement=()):return Item(clip_slots=[Item(has_clip=True,clip=c) for c in session],arrangement_clips=list(arrangement))

class RemapTests(unittest.TestCase):
    def test_swap_keeps_expression_and_note_ids(self):
        c=Clip([21,22,70]);state=r.ClipState({"kick":21,"snare":22});state.apply(c,{"kick":22,"snare":21})
        self.assertEqual([n.pitch for n in c.notes],[22,21,70])
        self.assertEqual([n.note_id for n in c.notes],[0,1,2])
        self.assertEqual(c.notes[0].expression,[1,2]);self.assertEqual(c.notes[0].probability,.6)
        self.assertEqual(state.apply(c,{"kick":22,"snare":21}),0);self.assertEqual(c.calls,1)
    def test_delete_mutes_without_reassigning_and_restore(self):
        c=Clip([21,22]);state=r.ClipState({"kick":21,"snare":22})
        state.apply(c,{"snare":21});self.assertEqual([(n.pitch,n.mute) for n in c.notes],[(21,True),(21,False)])
        state.apply(c,{"snare":22});self.assertTrue(c.notes[0].mute);self.assertEqual(c.notes[0].pitch,21)
        state.apply(c,{"kick":23,"snare":22});self.assertFalse(c.notes[0].mute);self.assertEqual(c.notes[0].pitch,23)
    def test_insert_shift_then_reorder_keeps_instruments(self):
        a,b=Clip([21,22]),Clip([22]);t=track([a],[b]);song=Song();state=r.TrackState(payload());state.sync(t,payload(),song)
        inserted=dict(kitId="kit",notes=[dict(id="hat",note=21),dict(id="kick",note=22),dict(id="snare",note=23)])
        state.sync(t,inserted,song);self.assertEqual([n.pitch for n in a.notes],[22,23]);self.assertEqual(b.notes[0].pitch,23)
        moved=dict(kitId="kit",notes=[dict(id="snare",note=21),dict(id="kick",note=22),dict(id="hat",note=23)])
        state.sync(t,moved,song);self.assertEqual([n.pitch for n in a.notes],[22,21]);self.assertEqual(b.notes[0].pitch,21)
        self.assertEqual(a.notes[0].duration,.3);self.assertEqual(a.notes[0].velocity,90)
    def test_previously_unassigned_notes_follow_new_instrument(self):
        c=Clip([23]);state=r.ClipState({"kick":21});state.apply(c,{"kick":21,"hat":23})
        state.apply(c,{"kick":21,"hat":22});self.assertEqual(c.notes[0].pitch,22)
    def test_original_mute_preserved(self):
        c=Clip([21]);c.notes[0].mute=True;state=r.ClipState({"kick":21});state.apply(c,{});state.apply(c,{"kick":22});self.assertTrue(c.notes[0].mute)
    def test_manual_pitch_rebind(self):
        c=Clip([21]);state=r.ClipState({"kick":21,"snare":22});state.apply(c,state.assigned)
        c.notes[0].pitch=22;state.apply(c,{"kick":22,"snare":23});self.assertEqual(c.notes[0].pitch,23)
    def test_session_arrangement_and_idempotence(self):
        a,b=Clip([21]),Clip([22]);t=track([a],[a,b]);song=Song();state=r.TrackState(payload());state.sync(t,payload(),song)
        self.assertEqual(state.sync(t,payload(22,21),song),2);self.assertEqual([a.notes[0].pitch,b.notes[0].pitch],[22,21])
        self.assertEqual(state.sync(t,payload(22,21),song),0);self.assertEqual(song.depth,0)
    def test_recording_deferred(self):
        c=Clip([21]);t=track([c]);s=Song();state=r.TrackState(payload());state.sync(t,payload(),s);c.is_recording=True
        state.sync(t,payload(22,21),s);self.assertEqual(c.calls,0);c.is_recording=False
        state.sync(t,payload(22,21),s);self.assertEqual(c.notes[0].pitch,22)
    def test_failed_verification_stops_retries(self):
        c=Clip([21]);t=track([c]);s=Song();state=r.TrackState(payload());state.sync(t,payload(),s);c.ignore=True
        with self.assertRaises(RuntimeError):state.sync(t,payload(22,21),s)
        self.assertEqual(s.depth,0)
        with self.assertRaises(RuntimeError):state.sync(t,payload(22,21),s)
        self.assertEqual(c.calls,1)
    def test_new_kit_does_not_mute_previous_notes(self):
        c=Clip([21]);s=Song();state=r.TrackState(payload());state.sync(track([c]),payload(),s)
        state.sync(track([c]),dict(kitId="new",notes=[]),s);self.assertFalse(c.notes[0].mute);self.assertEqual(c.calls,0)

if __name__=="__main__":unittest.main()
