"""Track-scoped Drum ownership. No matching by track name or kit filename."""
import json
import os
from .protocol import token_for


def drum_devices(owner, depth=0):
    if depth > 8:
        return
    for device in list(owner.devices):
        token = token_for(device)
        if str(device.class_name) == "PluginDevice" and (token or str(device.name) == "DJL Drum"):
            yield owner, device, token
        for chain in getattr(device, "chains", ()):
            yield from drum_devices(chain, depth + 1)


class InstanceGuard:
    def __init__(self):
        self.owners = []
        self.blocked = []

    def prune(self, tracks):
        self.owners = [(track, device) for track, device in self.owners if track in tracks]

    def enforce(self, track, folder, now, playing=False, selected=None):
        entries = list(drum_devices(track))
        if not entries:
            self.owners = [(t, d) for t, d in self.owners if t != track]
            return []
        previous = next((d for t, d in self.owners if t == track), None)
        keeper = next((d for _, d, _ in entries if d == previous), entries[0][1])
        self.owners = [(t, d) for t, d in self.owners if t != track] + [(track, keeper)]
        errors = []
        os.makedirs(folder, exist_ok=True)
        for owner, device, token in entries:
            allowed = device == keeper
            if token:
                payload = {"protocol": 1, "token": token, "timestamp": now,
                           "trackName": str(track.name), "trackId": str(getattr(track, "_live_ptr", id(track))),
                           "allowed": allowed, "selected": selected}
                path = os.path.join(folder, token + ".track.json")
                with open(path + ".tmp", "w", encoding="utf-8") as stream:
                    json.dump(payload, stream)
                os.replace(path + ".tmp", path)
            if allowed:
                for held, parameter, previous in self.blocked:
                    if held == device:
                        parameter.value = previous
                self.blocked = [(held, p, value) for held, p, value in self.blocked if held != device]
                continue
            # Block the new device immediately; leave the established device alone.
            for parameter in device.parameters:
                if str(getattr(parameter, "original_name", parameter.name)) == "Device On":
                    if not any(held == device for held, _, _ in self.blocked):
                        self.blocked.append((device, parameter, parameter.value))
                    parameter.value = parameter.min
            if playing:
                errors.append("Duplicate Drum on '%s' blocked; it will be removed when playback stops." % track.name)
                continue
            try:
                devices = list(owner.devices)
                index = next(i for i, candidate in enumerate(devices) if candidate == device)
                self.blocked = [(held, p, value) for held, p, value in self.blocked if held != device]
                owner.delete_device(index)
            except Exception as exc:
                errors.append("Remove the extra Drum device on '%s': %s" % (track.name, exc))
        return errors
