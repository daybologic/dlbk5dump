#!/usr/bin/env python3
"""Show non-empty fixed slots in the BK5 alternate-key file."""

from pathlib import Path


PATH = Path("BK5/ALTKEYS.DT5")
SLOT_SIZE = 162
data = PATH.read_bytes()
if len(data) % SLOT_SIZE:
    raise SystemExit("ALTKEYS.DT5 size is not a multiple of 162")

nonempty = 0
for slot in range(len(data) // SLOT_SIZE):
    raw = data[slot * SLOT_SIZE:(slot + 1) * SLOT_SIZE]
    text = raw.split(b"<", 1)[0].decode("cp437", "replace").strip()
    if text:
        print("ALTKEYS_SLOT slot={} text={!r}".format(slot, text))
        nonempty += 1
print("ALTKEYS_SLOTS total={} nonempty={}".format(len(data) // SLOT_SIZE,
                                                   nonempty))
