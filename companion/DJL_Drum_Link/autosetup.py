"""Load the installed MIDI rack once, through Live's own browser API."""
from .protocol import RACK_NAMES, token_for

PRESET_PATH = ("Presets", "MIDI Effects", "MIDI Effect Rack", "DJL Drum", "DJL Drum Link")

def find_preset(browser):
    item = browser.user_library
    for name in PRESET_PATH:
        matches = [child for child in item.children if str(child.name) in (name, name+".adg")]
        if len(matches) != 1:
            return None
        item = matches[0]
    return item if item.is_loadable else None

class AutoSetup:
    def __init__(self):
        self.attempts = {}

    def ensure(self, song, browser, track, requests):
        if getattr(browser,"hotswap_target",None) or song.is_playing:
            return None
        devices = list(track.devices)
        for index, device in enumerate(devices):
            token = token_for(device)
            if str(device.class_name) != "PluginDevice" or not (token or str(device.name)=="DJL Drum"):
                continue
            if index and str(devices[index-1].name) in RACK_NAMES:
                continue
            # Never stack another map when an existing map is misplaced.
            if any(str(d.name) in RACK_NAMES for d in devices):
                return "Move the existing DJL Drum Link rack immediately before Drum."
            request = requests.get(token,0)
            if self.attempts.get(device) == request:
                continue
            item = find_preset(browser)
            if item is None:
                return "Waiting for Live to index the installed DJL Drum Link preset."
            self.attempts[device] = request
            old_track = song.view.selected_track
            old_device = getattr(old_track.view,"selected_device",None)
            try:
                song.view.selected_track = track
                song.view.select_device(device)
                browser.load_item(item)
            finally:
                song.view.selected_track = old_track
                if old_device is not None:
                    song.view.select_device(old_device)
            # Loading may complete on a later Live tick. Never retry automatically.
            return "DJL Drum Link rack requested; checking its placement."
        return None
