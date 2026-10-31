"""Live Control Surface: synchronise only DJL's named-key racks.
No external process, MIDI ports, network access or audio processing.
"""
import json
import os
import time
from _Framework.ControlSurface import ControlSurface
from .protocol import RACK_NAMES, token_for, names_from, update_rack


def create_instance(c_instance):
    return DrumLink(c_instance)


class DrumLink(ControlSurface):
    def __init__(self, c_instance):
        super(DrumLink, self).__init__(c_instance)
        self._folder = os.path.join(os.environ.get("APPDATA", os.path.expanduser("~/Library")), "DJL", "Drum", "LiveLink")
        self._last_poll = 0
        self._last_status = None
        self._last_diagnostic = None
        self.log_message("DJL Drum Link started")

    def _pairs(self, devices, depth=0):
        if depth > 8:
            return
        devices = list(devices)
        for index, device in enumerate(devices):
            if str(device.name) in RACK_NAMES and str(device.class_name) == "MidiEffectGroupDevice":
                if index+1 < len(devices):
                    token = token_for(devices[index+1])
                    if token:
                        yield device, token
            for chain in getattr(device, "chains", ()):
                for pair in self._pairs(chain.devices, depth+1):
                    yield pair

    def update_display(self):
        super(DrumLink, self).update_display()
        now = time.time()
        if now-self._last_poll < 0.5:
            return
        self._last_poll = now
        connected, errors = 0, []
        diagnostic = []
        try:
            for track in self.song().tracks:
                devices = list(track.devices)
                for index, device in enumerate(devices):
                    if "DJL" in str(device.name) or "Drum Link" in str(device.name):
                        entry = {"index": index, "name": str(device.name), "class": str(device.class_name), "chains": len(getattr(device,"chains",()))}
                        entry["parameters"] = [{"name": str(p.name), "original": str(getattr(p,"original_name",p.name))} for p in device.parameters][:24]
                        diagnostic.append(entry)
                for rack, token in self._pairs(track.devices):
                    try:
                        path = os.path.join(self._folder, token+".json")
                        if os.path.getsize(path) > 128*1024:
                            raise ValueError("Oversized link data")
                        with open(path, "r", encoding="utf-8-sig") as stream:
                            names = names_from(json.load(stream), token, now)
                        update_rack(rack, names)
                        connected += 1
                    except Exception as exc:
                        errors.append(str(exc))
            if diagnostic != self._last_diagnostic:
                self._last_diagnostic = diagnostic
                os.makedirs(self._folder, exist_ok=True)
                path = os.path.join(self._folder,"diagnostic.json")
                with open(path+".tmp", "w", encoding="utf-8") as stream:
                    json.dump({"timestamp": now, "devices": diagnostic}, stream)
                os.replace(path+".tmp",path)
            status = {"connected": connected, "errors": errors[:8]}
            if status != self._last_status:
                self._last_status = status
                self.log_message("DJL Drum Link: "+json.dumps(status))
        except Exception as exc:
            self.log_message("DJL Drum Link scan: "+str(exc))

    def disconnect(self):
        self.log_message("DJL Drum Link stopped")
        super(DrumLink, self).disconnect()
