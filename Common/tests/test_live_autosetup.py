import importlib
import sys
import types
import unittest
from pathlib import Path
from types import SimpleNamespace as Item
package=types.ModuleType("djl_setup_test");package.__path__=[str(Path(__file__).resolve().parents[1]/"companion/DJL_Drum_Link")];sys.modules[package.__name__]=package
m=importlib.import_module("djl_setup_test.autosetup")
class Device:
    def __init__(self,name,kind="PluginDevice"):
        self.name=name;self.class_name=kind;self.parameters=[]
class View:
    def __init__(self,track):self.selected_track=track
    def select_device(self,device):self.selected_track.view.selected_device=device
class SetupTests(unittest.TestCase):
    def fixture(self):
        device=Device("DJL Drum");track=Item(devices=[device],view=Item(selected_device=device))
        song=Item(is_playing=False,view=View(track))
        leaf=Item(name="DJL Drum Link.adg",children=[],is_loadable=True)
        node=leaf
        for name in reversed(m.PRESET_PATH[:-1]):node=Item(name=name,children=[node],is_loadable=False)
        browser=Item(user_library=Item(children=[node]),hotswap_target=None,calls=0)
        def load(item):
            browser.calls+=1;self.assertIs(item,leaf)
            target=song.view.selected_track;index=target.devices.index(target.view.selected_device)
            target.devices.insert(index,Device("DJL Drum Link","MidiEffectGroupDevice"))
        browser.load_item=load
        return song,browser,track,device
    def test_insert_before_once_and_restore_selection(self):
        song,browser,track,device=self.fixture();other=Item(view=Item(selected_device=Device("other")))
        song.view.selected_track=other;old=other.view.selected_device;setup=m.AutoSetup()
        setup.ensure(song,browser,track,{})
        self.assertEqual([d.name for d in track.devices],["DJL Drum Link","DJL Drum"])
        self.assertIs(song.view.selected_track,other);self.assertIs(other.view.selected_device,old)
        setup.ensure(song,browser,track,{});self.assertEqual(browser.calls,1)
    def test_defer_playback_and_hotswap(self):
        song,browser,track,_=self.fixture();setup=m.AutoSetup();song.is_playing=True
        setup.ensure(song,browser,track,{});song.is_playing=False;browser.hotswap_target=object()
        setup.ensure(song,browser,track,{});self.assertEqual(browser.calls,0)
    def test_misplaced_map_does_not_duplicate(self):
        song,browser,track,_=self.fixture();track.devices.append(Device("DJL Drum Link"))
        self.assertIn("Move",m.AutoSetup().ensure(song,browser,track,{}));self.assertEqual(browser.calls,0)
    def test_failed_load_never_loops_and_explicit_retry(self):
        song,browser,track,device=self.fixture();token="0123456789abcdef";device.parameters=[Item(name="DJL Link "+token)]
        def fail(item):browser.calls+=1
        browser.load_item=fail;setup=m.AutoSetup()
        setup.ensure(song,browser,track,{});setup.ensure(song,browser,track,{})
        self.assertEqual(browser.calls,1)
        setup.ensure(song,browser,track,{token:1});self.assertEqual(browser.calls,2)
    def test_missing_preset_does_not_load(self):
        song,browser,track,_=self.fixture();browser.user_library.children=[]
        self.assertIn("index",m.AutoSetup().ensure(song,browser,track,{}));self.assertEqual(browser.calls,0)
if __name__=="__main__":unittest.main()
