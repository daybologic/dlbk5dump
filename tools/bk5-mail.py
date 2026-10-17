#!/usr/bin/env python3
"""Show printable, unassigned text runs from BK5/BKMAIL.DT5."""

from pathlib import Path


data = Path("BK5/BKMAIL.DT5").read_bytes()
start = None
count = 0
for offset, value in enumerate(data + b"\0"):
    printable = 32 <= value < 127
    if printable and start is None:
        start = offset
    elif not printable and start is not None:
        text = data[start:offset].decode("cp437", "replace").strip()
        if len(text) >= 3:
            print("BKMAIL_TEXT offset=0x{:x} text={!r}".format(offset, text))
            count += 1
        start = None
print("BKMAIL_TEXT_RUNS count={}".format(count))
