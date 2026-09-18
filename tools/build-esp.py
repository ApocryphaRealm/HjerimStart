#!/usr/bin/env python3
r"""Author Alternate Perspective Hjerim Start.esp.

There is no Creation Kit on this machine and this plugin does not need one: it holds exactly ONE
record, and it exists only so Alternate Perspective has something to name in its menu and something
to start. All of the behaviour is in HjerimStart.dll, which watches for that quest starting. The
plugin is written here as raw TES4 records, so the result is byte-for-byte reproducible and
reviewable in a diff.

What it contains:

    TES4                             header, one master: Skyrim.esm, light-flagged (ESPFE)
    GRUP QUST
      QUST 0x01000800  APS_HjerimStart   "Hjerim"

The quest is deliberately inert - no stages, no aliases, no script. It is not start-game-enabled, so
nothing happens until Alternate Perspective starts it; the DLL stops it again as soon as the start
sequence has run.

FormID note: the plugin declares one master, so its own record takes on-disk index 01 and the record
is 0x01000800. Alternate Perspective is given "0x800" in the JSON, because it calls
Game.GetFormFromFile, which takes the FormID without the plugin's index.

Usage:  python build-esp.py [out.esp]
"""
import os
import struct
import sys

AUTHOR = "ApocryphaRealm"
DESCRIPTION = "Alternate Perspective Hjerim Start"

FORM_VERSION = 44  # SSE

QUESTS = [
    (0x01000800, "APS_HjerimStart", "Hjerim"),
]


def sub(tag: bytes, data: bytes) -> bytes:
    """One subrecord: 4-byte tag, 2-byte size, payload."""
    if len(data) > 0xFFFF:
        raise ValueError("subrecord %s is too long for a 16-bit size" % tag)
    return tag + struct.pack("<H", len(data)) + data


def record(tag: bytes, formid: int, data: bytes, flags: int = 0) -> bytes:
    """One record: 24-byte header then the subrecords. Never compressed - these are tiny."""
    return (tag + struct.pack("<I", len(data)) + struct.pack("<I", flags) +
            struct.pack("<I", formid) + struct.pack("<I", 0) +
            struct.pack("<H", FORM_VERSION) + struct.pack("<H", 0) + data)


def quest_record(formid: int, edid: str, full: str) -> bytes:
    # DNAM is the quest's own data: flags(u16) priority(u8) unused(u8) unknown(i32) type(i32).
    #   flags 0x0000 - NOT start-game-enabled. AP starts it; nothing else should.
    #   priority 0, type 0 (None) - it is not a main/side/misc quest and must not appear in the journal.
    dnam = struct.pack("<HBBiI", 0x0000, 0, 0, 0, 0)
    body = (sub(b"EDID", edid.encode("cp1252") + b"\x00") +
            sub(b"FULL", full.encode("cp1252") + b"\x00") +
            sub(b"DNAM", dnam) +
            sub(b"ANAM", struct.pack("<I", 0)))  # next alias id - there are no aliases
    return record(b"QUST", formid, body)


# Every plugin of ours is an ESPFE: a .esp carrying the light flag (the owner's standing rule). The
# record sits between 0x000800 and 0x000FFF, which is the light range, so the flag is all it takes.
ESL_FLAG = 0x00000200


def tes4_header(record_count: int, next_object_id: int) -> bytes:
    # HEDR: version(f32) numRecords(i32) nextObjectID(u32)
    hedr = struct.pack("<fiI", 1.7, record_count, next_object_id)
    body = (sub(b"HEDR", hedr) +
            sub(b"CNAM", AUTHOR.encode("cp1252") + b"\x00") +
            sub(b"SNAM", DESCRIPTION.encode("cp1252") + b"\x00") +
            sub(b"MAST", b"Skyrim.esm\x00") +
            sub(b"DATA", struct.pack("<Q", 0)))
    return record(b"TES4", 0, body, flags=ESL_FLAG)


def main(out_path: str) -> int:
    quests = b"".join(quest_record(formid, edid, full) for formid, edid, full in QUESTS)
    group = (b"GRUP" + struct.pack("<I", len(quests) + 24) + b"QUST" +
             struct.pack("<I", 0) + struct.pack("<I", 0) + struct.pack("<I", 0))
    next_id = (max(q[0] for q in QUESTS) & 0xFFFFFF) + 1
    data = tes4_header(len(QUESTS), next_id) + group + quests

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)

    print("wrote %s (%d bytes)" % (out_path, len(data)))
    for formid, edid, full in QUESTS:
        print("  QUST %08X  %-18s \"%s\"   -> JSON id 0x%X" % (formid, edid, full, formid & 0xFFFFFF))
    return 0


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    default = os.path.join(here, "..", "dist", "Alternate Perspective Hjerim Start.esp")
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(default)))
