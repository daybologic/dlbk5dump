#!/usr/bin/env python3
"""Compare active and backup BK5 option text without interpreting settings."""

from pathlib import Path


def read_options(path):
    result = {}
    for raw in Path(path).read_bytes().splitlines():
        line = raw.decode("cp437", "replace").rstrip()
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = value
    return result


active = read_options("BK5/BKOPTION.DT5")
backup = read_options("BK5/BKOPTION.BAK")
keys = sorted(set(active) | set(backup))
differences = 0
for key in keys:
    current = active.get(key, "<missing>")
    saved = backup.get(key, "<missing>")
    if current != saved:
        print("BKOPTION_DIFFERENCE key={!r} active={!r} backup={!r}".format(
            key, current, saved))
        differences += 1
print("BKOPTION_SUMMARY active_keys={} backup_keys={} differences={}".format(
    len(active), len(backup), differences))
