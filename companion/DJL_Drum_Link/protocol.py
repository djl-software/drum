"""Small, testable validation layer; no Live imports or executable messages."""
import math
import re

PREFIX = "DJL Link "
RACK_NAME = "DJL Drum Link v1 - Drum note names"
RACK_NAMES = {RACK_NAME, "DJL Drum Link"}

def token_for(device):
    found = [str(getattr(p, "original_name", p.name))[len(PREFIX):] for p in device.parameters if str(getattr(p, "original_name", p.name)).startswith(PREFIX)]
    if len(found) != 1 or not re.fullmatch(r"[0-9a-fA-F]{16}", found[0]):
        return None
    return found[0]

def names_from(payload, token, now):
    if not isinstance(payload, dict) or payload.get("protocol") != 1 or payload.get("token") != token:
        raise ValueError("Incompatible Drum link data")
    timestamp = payload.get("timestamp")
    if not isinstance(timestamp, (float, int)) or not math.isfinite(timestamp) or not -2 <= now-timestamp <= 10:
        raise ValueError("Drum link is inactive")
    notes = payload.get("notes")
    if not isinstance(notes, list) or len(notes) > 88:
        raise ValueError("Invalid instrument list")
    names = [note_name(note) for note in range(21, 109)]
    seen = set()
    ids = set()
    for item in notes:
        if not isinstance(item, dict):
            raise ValueError("Invalid instrument")
        key, name, identity = item.get("note"), item.get("name"), item.get("id")
        if type(key) is not int or not 21 <= key <= 108 or key in seen:
            raise ValueError("Invalid or duplicated note")
        if not isinstance(name, str) or not name or len(name) > 1024:
            raise ValueError("Invalid instrument name")
        if not isinstance(identity, str) or not identity or identity in ids:
            raise ValueError("Invalid instrument identity")
        seen.add(key); ids.add(identity); names[key-21] = name
    return names

def update_rack(rack, names):
    chains = list(rack.chains)
    if len(chains) != 88 or any(len(chain.devices) for chain in chains):
        raise ValueError("Use a fresh, unmodified DJL Drum Link rack")
    changed = 0
    for chain, name in zip(chains, names):
        if chain.name != name:
            chain.name = name
            changed += 1
    return changed


def note_name(note):
    return ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")[note % 12] + str(note // 12 - 2)


def compact_rack(rack):
    # One-time presentation only; never change MIDI/audio routing.
    view = getattr(rack, "view", None)
    if view is None:
        return False
    try:
        view.is_collapsed = True
        return True
    except (AttributeError, RuntimeError):
        return False
