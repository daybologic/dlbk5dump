#!/usr/bin/env python3
"""Check coverage and referential integrity of the intermediate BK5 GEDCOM."""

import re
import subprocess
import sys
from pathlib import Path


def dump_people():
    return subprocess.check_output(
        ["./dlbk5dump", "--dump-id-records"],
        text=True, stderr=subprocess.DEVNULL).splitlines()


def dump_raw_people():
    return subprocess.check_output(
        ["./dlbk5dump", "--dump-records"],
        text=True, stderr=subprocess.DEVNULL).splitlines()


def printable_runs(path):
    data = Path(path).read_bytes()
    result = []
    start = None
    for offset, value in enumerate(data + b"\0"):
        if 32 <= value < 127 and start is None:
            start = offset
        elif not (32 <= value < 127) and start is not None:
            text = data[start:offset].decode("cp437", "replace").strip()
            if len(text) >= 3:
                result.append((start, text))
            start = None
    return result


def main():
    gedcom = sys.stdin.read() if len(sys.argv) == 1 else open(
        sys.argv[1], encoding="utf-8").read()
    ged_lines = gedcom.splitlines()
    person_lines = [line for line in dump_people()
                    if line.startswith("PERSON_ID_RECORD ")]
    raw_lines = [line for line in dump_raw_people()
                 if line.startswith("PERSON_RECORD ")]

    named = [line for line in person_lines
             if 'status=decoded' in line and 'name=""' not in line]
    failed = [line for line in person_lines
              if 'status=compressed-decode-failed' in line
              or 'status=address-out-of-range' in line
              or 'status=deleted-or-uncompressed' in line]
    unnamed = [line for line in person_lines
               if 'status=decoded' in line and 'name=""' in line]
    date_fields = []
    family_refs = set()
    for line in person_lines:
        match = re.search(r" dates=(.*?)(?: source=|$)", line)
        if match:
            date_fields.extend(value for value in match.group(1).split(',')
                               if value)
        match = re.search(r" families=([^ ]*) ", line)
        if match and match.group(1):
            family_refs.update("@F{:x}@".format(int(value, 16))
                              for value in match.group(1).split(',')
                              if int(value, 16) != 0)

    individuals = [line.split()[1] for line in ged_lines
                   if re.match(r"^0 @I[^ ]+@ INDI$", line)]
    families = [line.split()[1] for line in ged_lines
                if re.match(r"^0 @F[^ ]+@ FAM$", line)]
    refs = [line.split()[-1] for line in ged_lines
            if re.match(r"^1 (?:HUSB|WIFE|CHIL|FAMC|FAMS) @[^ ]+@$", line)]
    dates_in_gedcom = [line for line in ged_lines
                       if line.startswith("2 DATE ")]
    raw_date_fields = sum(
        len([value for value in match.group(1).split(',') if value])
        for line in raw_lines
        for match in [re.search(r" dates=(.*)$", line)] if match)
    raw_date_notes = [line for line in ged_lines
                      if line.startswith("1 NOTE BK5 raw date at ")]
    mail_runs = printable_runs("BK5/BKMAIL.DT5")
    mail_notes = [line for line in ged_lines
                  if line.startswith(("1 CONT BKMAIL offset ",
                                       "2 CONT BKMAIL offset "))]
    altkey_notes = [line for line in ged_lines
                    if line.startswith(("1 CONT ALTKEYS slot ",
                                         "2 CONT ALTKEYS slot "))]
    unresolved_lookup = [line for line in ged_lines
                         if "[not decoded]" in line]
    missing_families = sorted(family_refs - set(families))
    dangling = sorted(set(refs) - set(individuals) - set(families))

    print("AUDIT diagnostic_records={}".format(len(person_lines)))
    print("AUDIT named_decoded={} undecodable={} unnamed_decoded={}".format(
        len(named), len(failed), len(unnamed)))
    print("AUDIT diagnostic_date_fields={} gedcom_dates={}".format(
        len(date_fields), len(dates_in_gedcom)))
    print("AUDIT raw_date_fields={} raw_date_notes={}".format(
        raw_date_fields, len(raw_date_notes)))
    print("AUDIT mail_runs={} mail_notes={} altkey_notes={}".format(
        len(mail_runs), len(mail_notes), len(altkey_notes)))
    print("AUDIT unresolved_lookup_notes={}".format(len(unresolved_lookup)))
    print("AUDIT individuals={} families={} duplicate_individuals={} "
          "duplicate_families={}".format(
              len(individuals), len(families),
              len(individuals) - len(set(individuals)),
              len(families) - len(set(families))))
    print("AUDIT family_refs={} missing_families={} dangling_refs={}".format(
        len(family_refs), len(missing_families), len(dangling)))
    if missing_families:
        print("AUDIT_MISSING_FAMILIES {}".format(" ".join(missing_families)))
    if dangling:
        print("AUDIT_DANGLING_REFS {}".format(" ".join(dangling)))
    if (missing_families or dangling
            or raw_date_fields != len(raw_date_notes)
            or len(mail_runs) != len(mail_notes)
            or len(altkey_notes) != 1
            or unresolved_lookup):
        return 1
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print("bk5-audit: {}".format(error), file=sys.stderr)
        raise SystemExit(1)
