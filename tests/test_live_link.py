import importlib.util
from pathlib import Path
from types import SimpleNamespace as Item
import unittest
spec = importlib.util.spec_from_file_location("protocol", Path(__file__).resolve().parents[1]/"companion/DJL_Drum_Link/protocol.py")
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)

class LinkTests(unittest.TestCase):
    def payload(self):
        return dict(protocol=1, token="0123456789abcdef", timestamp=100, notes=[dict(id="kick", note=21, name="Kick"),dict(id="snare", note=22, name="Snare")])
    def test_names_and_reorder(self):
        data=self.payload(); names=p.names_from(data,data["token"],101)
        self.assertEqual(names[:3],["Kick","Snare","B-1"])
        data["notes"][0]["note"]=22; data["notes"][1]["note"]=21
        self.assertEqual(p.names_from(data,data["token"],101)[:2],["Snare","Kick"])
    def test_empty_keys_and_compact_view(self):
        data=self.payload();data["notes"]=[];names=p.names_from(data,data["token"],101)
        self.assertEqual(names[0],"A-1");self.assertEqual(names[3],"C0");self.assertEqual(names[-1],"C7")
        rack=Item(view=Item(is_collapsed=False));self.assertTrue(p.compact_rack(rack));self.assertTrue(rack.view.is_collapsed)
        self.assertFalse(p.compact_rack(Item()))
    def test_invalid_and_stale(self):
        for kind in ("stale","foreign","duplicate","outside","identity"):
            data=self.payload()
            if kind=="stale": data["timestamp"]=50
            if kind=="foreign": data["token"]="different"
            if kind=="duplicate": data["notes"][1]["note"]=21
            if kind=="outside": data["notes"][1]["note"]=128
            if kind=="identity": data["notes"][1]["id"]="kick"
            with self.assertRaises(ValueError): p.names_from(data,"0123456789abcdef",101)
    def test_live_preset_filename_alias(self):
        self.assertIn("DJL Drum Link",p.RACK_NAMES)
        self.assertIn(p.RACK_NAME,p.RACK_NAMES)
        device=Item(parameters=[Item(name="Renamed",original_name="DJL Link 0123456789abcdef")])
        self.assertEqual(p.token_for(device),"0123456789abcdef")

    def test_instances_and_parameter_value(self):
        a=Item(parameters=[Item(name="DJL Link 0123456789abcdef",value=0)])
        b=Item(parameters=[Item(name="DJL Link abcdef0123456789",value=1)])
        self.assertNotEqual(p.token_for(a),p.token_for(b))
        self.assertIsNone(p.token_for(Item(parameters=[Item(name="DJL Link ../bad")])) )
    def test_no_repeated_writes_and_deletion(self):
        rack=Item(chains=[Item(name="",devices=[]) for n in range(88)])
        data=self.payload(); names=p.names_from(data,data["token"],101)
        self.assertEqual(p.update_rack(rack,names),88)
        self.assertEqual(p.update_rack(rack,names),0)
        data["notes"].pop()
        self.assertEqual(p.update_rack(rack,p.names_from(data,data["token"],101)),1)
        self.assertEqual(rack.chains[1].name,"A#-1")
        rack.chains[0].devices.append("effect")
        with self.assertRaises(ValueError): p.update_rack(rack,names)

if __name__=="__main__": unittest.main()
