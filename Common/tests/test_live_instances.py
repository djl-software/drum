import importlib
from pathlib import Path
import sys
import tempfile
import types
import json
import unittest

package = types.ModuleType("drum_guard_tests")
package.__path__ = [str(Path(__file__).resolve().parents[1] / "companion/DJL_Drum_Link")]
sys.modules[package.__name__] = package
Guard = importlib.import_module(package.__name__ + ".instances").InstanceGuard


class Device:
    def __init__(self, number, name="DJL Drum"):
        self.name, self.class_name, self.chains = name, "PluginDevice", []
        self.parameters = [types.SimpleNamespace(name="Device On", min=0, value=1),
                           types.SimpleNamespace(name="DJL Link %016x" % number)]


class Track:
    def __init__(self, name, devices):
        self.name, self.devices = name, devices
        self._live_ptr = id(self)
        self.deleted = []

    def delete_device(self, index):
        self.deleted.append(self.devices.pop(index))


class InstanceTests(unittest.TestCase):
    def test_two_tracks_even_with_same_name_are_independent(self):
        a, b = Track("Drums", [Device(1)]), Track("Drums", [Device(2)])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard()
            for track in (a, b):
                self.assertEqual(guard.enforce(track, folder, 10), [])
                self.assertEqual(len(track.devices), 1)
            for number in (1, 2):
                data = json.loads((Path(folder) / ("%016x.track.json" % number)).read_text())
                self.assertTrue(data["allowed"])
                self.assertEqual(data["trackName"], "Drums")

    def test_new_duplicate_before_existing_device_is_removed(self):
        old, new = Device(1), Device(2)
        track = Track("Kick", [old])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard(); guard.enforce(track, folder, 10)
            track.devices.insert(0, new)
            guard.enforce(track, folder, 11)
            self.assertEqual(track.devices, [old])
            self.assertEqual(track.deleted, [new])
            self.assertEqual(old.parameters[0].value, 1)

    def test_playing_blocks_duplicate_then_removes_when_stopped(self):
        old, new = Device(1), Device(2)
        track = Track("Hats", [old, new])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard()
            self.assertTrue(guard.enforce(track, folder, 10, True))
            self.assertEqual(len(track.devices), 2)
            self.assertEqual(new.parameters[0].value, 0)
            self.assertEqual(old.parameters[0].value, 1)
            data = json.loads((Path(folder) / "0000000000000002.track.json").read_text())
            self.assertFalse(data["allowed"])
            guard.enforce(track, folder, 11, False)
            self.assertEqual(track.devices, [old])

    def test_moving_blocked_device_to_another_track_restores_it(self):
        old, new = Device(1), Device(2)
        track = Track("Hats", [old, new]); other = Track("Percussion", [])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard(); guard.enforce(track, folder, 10, True)
            self.assertEqual(new.parameters[0].value, 0)
            track.devices.remove(new); other.devices.append(new)
            guard.enforce(track, folder, 11, True); guard.enforce(other, folder, 11, True)
            self.assertEqual(new.parameters[0].value, 1)
            self.assertEqual(old.parameters[0].value, 1)

    def test_selected_track_state_is_explicit(self):
        track = Track("Hats", [Device(1)])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard(); guard.enforce(track, folder, 10, selected=False)
            path = Path(folder) / "0000000000000001.track.json"
            self.assertFalse(json.loads(path.read_text())["selected"])
            guard.enforce(track, folder, 11, selected=True)
            self.assertTrue(json.loads(path.read_text())["selected"])

    def test_nested_duplicate_and_rename(self):
        old, new = Device(1), Device(2)
        chain = Track("Chain", [new])
        rack = types.SimpleNamespace(name="Instrument Rack", class_name="InstrumentGroupDevice", parameters=[], chains=[chain])
        track = Track("Hats", [old, rack])
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard(); guard.enforce(track, folder, 10)
            self.assertEqual(chain.devices, [])
            track.name = "Percussion"; guard.enforce(track, folder, 11)
            data = json.loads((Path(folder) / "0000000000000001.track.json").read_text())
            self.assertEqual(data["trackName"], "Percussion")


if __name__ == "__main__":
    unittest.main()
