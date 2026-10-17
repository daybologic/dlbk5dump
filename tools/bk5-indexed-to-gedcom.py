#!/usr/bin/env python3
"""Export the currently decoded BK5 key-0 records as a cautious GEDCOM.

This is intentionally an intermediate export.  It does not discard the
diagnostic source address or unknown date slots, and it states that raw-only
records still need to be merged before the export can be considered complete.
"""

import re
import subprocess
import sys


PERSON_RE = re.compile(
    r'^PERSON_ID_RECORD id=(\d+) address=0x([0-9a-f]+) status=decoded '
    r'decoded_length=(\d+) name="(.*?)" sex=(.) families=([^ ]*) '
    r'occupation="(.*?)" notes="(.*?)" location_refs=([^ ]*) '
    r'source_refs=([^ ]*) '
    r'dates=(.*?)(?: source=([^ ]+))?$'
)
RAW_PERSON_RE = re.compile(
    r'^PERSON_RECORD offset=(\d+)(?: person_id=(\d+))? '
    r'leading_numeric=0x[0-9a-f]+ '
    r'raw_name="(.*?)" sex=(.) families=([^ ]*) dates=(.*)$'
)
MARRIAGE_RE = re.compile(r'^MARRIAGE_RECORD .* date="([^"]*)"$')
MARRIAGE_HEADER_RE = re.compile(
    r'spouse_ref_1=0x([0-9a-f]+) spouse_ref_2=0x([0-9a-f]+)'
    r'(?: family_id=0x([0-9a-f]+))?'
)
MARRIAGE_COMPACT_RE = re.compile(
    r'compact_ref=0x([0-9a-f]+)'
)
def gedcom_date(value):
    """Normalize the month in a BK5 date without changing source files."""
    fields = value.strip().split()
    if len(fields) >= 3:
        fields[1] = fields[1][:3].upper()
    return " ".join(fields)


def run_dump(*args):
    return subprocess.check_output(["./dlbk5dump", *args], text=True,
                                   stderr=subprocess.DEVNULL)


def parse_people(text):
    people = {}
    unresolved = []
    for line in text.splitlines():
        match = PERSON_RE.match(line)
        if match:
            person_id, address, length, name, sex, families, occupation, notes, location_refs, source_refs, dates, source = match.groups()
            refs = [] if not families else [int(value, 16)
                                            for value in families.split(',')]
            date_values = {}
            for value in dates.split(',') if dates else []:
                if ':' not in value:
                    continue
                offset, date = value.split(':', 1)
                date_values[offset] = date
            people[int(person_id)] = {
                "address": address,
                "length": int(length),
                "name": name.strip(),
                "sex": sex if sex in "MF" else "",
                "occupation": occupation.strip(),
                "notes": [value for value in notes.split('|') if value],
                "location_refs": [int(value) for value in location_refs.split(',')
                                  if value],
                "source_refs": [int(value) for value in source_refs.split(',')
                                if value],
                "families": refs,
                "dates": date_values,
                "source": source or "indexed-key0",
            }
        elif line.startswith("PERSON_ID_RECORD "):
            unresolved.append(line)
    return people, unresolved


def parse_lookup_file(path):
    """Read the source descriptions referenced by BKPERSON.DT5.

    Source records begin with a big-endian source number followed by printable
    text.  The surrounding Btrieve record metadata is deliberately ignored;
    the text is retained only when the record boundary is unambiguous.
    """
    data = open(path, "rb").read()
    candidates = {}
    for offset in range(4, len(data) - 8):
        if data[offset - 1] != 0:
            continue
        source_id = int.from_bytes(data[offset:offset + 4], "big")
        if not 1 <= source_id <= 1000:
            continue
        first = offset + 4
        if sum(32 <= value < 127 for value in data[first:first + 3]) < 3:
            continue
        end = first
        while end < len(data) and 32 <= data[end] < 127:
            end += 1
        text = data[first:end].decode("cp437", "replace").rstrip()
        if len(text) >= 3 and source_id not in candidates:
            candidates[source_id] = text
    return candidates


def parse_altkeys(path):
    """Preserve non-empty fixed ALTKEYS slots without inventing ownership."""
    data = open(path, "rb").read()
    slot_size = 162
    if len(data) % slot_size:
        raise ValueError("ALTKEYS.DT5 size is not a multiple of 162")
    result = []
    for slot in range(len(data) // slot_size):
        raw = data[slot * slot_size:(slot + 1) * slot_size]
        text = raw.split(b"<", 1)[0].decode("cp437", "replace").strip()
        if text:
            result.append((slot, text))
    return result


def parse_printable_runs(path, minimum=3):
    """Return printable text runs with offsets, without assigning ownership."""
    data = open(path, "rb").read()
    result = []
    start = None
    for offset, value in enumerate(data + b"\0"):
        printable = 32 <= value < 127
        if printable and start is None:
            start = offset
        elif not printable and start is not None:
            text = data[start:offset].decode("cp437", "replace").strip()
            if len(text) >= minimum:
                result.append((start, text))
            start = None
    return result


def parse_raw_people(text):
    people = []
    for line in text.splitlines():
        match = RAW_PERSON_RE.match(line)
        if not match:
            continue
        offset, person_id, name, sex, families, dates = match.groups()
        refs = [] if not families else [int(value, 16)
                                        for value in families.split(',')]
        date_values = []
        for value in dates.split(',') if dates else []:
            if ':' in value:
                date_values.append(tuple(value.split(':', 1)))
        people.append({
            "xref": person_id or "R" + offset,
            "person_id": int(person_id) if person_id else None,
            "offset": offset,
            "name": name.strip(),
            "raw_name": name.strip(),
            "sex": sex if sex in "MF" else "",
            "families": refs,
            "dates": date_values,
        })
    return people


def raw_display_name(person):
    """Remove the observed ten-byte sort key and trailing code when safe."""
    raw_name = person["raw_name"]
    if raw_name.startswith("*") or len(raw_name) <= 11:
        return raw_name
    candidate = raw_name[10:-1].strip()
    if candidate and candidate[0].isalpha() and " " in candidate:
        return candidate
    return raw_name


def parse_marriages(text, people):
    result = {}
    unresolved = []
    pending_date = None
    for line in text.splitlines():
        match = MARRIAGE_RE.match(line)
        if match:
            pending_date = match.group(1)
            continue
        match = MARRIAGE_COMPACT_RE.search(line)
        if match and pending_date is not None:
            compact_ref = int(match.group(1), 16)
            refs = people.get(compact_ref, {}).get("families", [])
            family_ids = [refs[1]] if len(refs) >= 2 and refs[1] else []
            if len(family_ids) == 1 and family_ids[0] not in result:
                result[family_ids[0]] = {
                    "date": pending_date,
                    "spouses": [compact_ref],
                    "inferred_family_id": True,
                    "inference": "Family ID recovered from compact BKMARR key and the person's FAMS reference; spouse pair remains layout-incomplete.",
                }
            else:
                unresolved.append((compact_ref, 0, pending_date, []))
            pending_date = None
            continue
        match = MARRIAGE_HEADER_RE.search(line)
        if match and pending_date is not None:
            spouse_1, spouse_2, family_id = match.groups()
            spouse_1 = int(spouse_1, 16)
            spouse_2 = int(spouse_2, 16)
            inferred = False
            if family_id is None:
                refs_1 = {value for value in people.get(spouse_1, {}).get("families", [])
                          if value}
                refs_2 = {value for value in people.get(spouse_2, {}).get("families", [])
                          if value}
                common = refs_1 & refs_2
                if len(common) == 1:
                    family_id = next(iter(common))
                    inferred = True
                else:
                    unresolved.append((spouse_1, spouse_2, pending_date,
                                       sorted(common)))
                    pending_date = None
                    continue
            if not inferred:
                family_id = int(family_id, 16)
            result[family_id] = {
                "date": pending_date,
                "spouses": [spouse_1, spouse_2],
                "inferred_family_id": inferred,
                "inference": "Family ID recovered from the common reference in both spouse records." if inferred else "",
            }
            pending_date = None
    return result, unresolved


def family_tag(family_id):
    return "@F{:x}@".format(family_id)


def family_roles(person_id, refs, marriages):
    """Assign family roles using decoded marriage spouses where available."""
    result = []
    for index, family_id in enumerate(refs):
        if not family_id:
            continue
        marriage = marriages.get(family_id)
        if marriage is not None:
            role = "FAMS" if person_id in marriage["spouses"] else "FAMC"
        else:
            role = "FAMC" if index == 0 else "FAMS"
        result.append((family_id, role))
    return result


DATE_SLOTS = ("0x111", "0x120", "0x12f", "0x13e", "0x14d")
DATE_TAGS = ("BIRT", "BAPM", "DEAT", "BURI", None)


def emit(line=""):
    print(line)


def main():
    people, unresolved = parse_people(run_dump("--dump-id-records"))
    locations = parse_lookup_file("BK5/BKLOCATE.DT5")
    sources = parse_lookup_file("BK5/BKSOURCE.DT5")
    altkeys = parse_altkeys("BK5/ALTKEYS.DT5")
    mail_text = parse_printable_runs("BK5/BKMAIL.DT5")
    raw_people = parse_raw_people(run_dump("--dump-records"))
    marriages, unresolved_marriages = parse_marriages(
        run_dump("--dump-marriages"), people)
    raw_by_id = {person["person_id"]: person for person in raw_people
                 if person["person_id"] is not None}
    raw_unmerged = [person for person in raw_people
                    if person["person_id"] is None
                    or person["person_id"] not in people]
    placeholders = set()
    marriage_families_by_person = {}
    for marriage in marriages.values():
        for person_id in marriage["spouses"]:
            marriage_families_by_person.setdefault(person_id, []).append(
                next(family_id for family_id, value in marriages.items()
                     if value is marriage))
            if person_id not in people and person_id not in raw_by_id:
                placeholders.add(person_id)

    emit("0 HEAD")
    emit("1 SOUR dlbk5dump")
    emit("1 NOTE This is an intermediate export of decoded BK5 fixed records.")
    emit("1 NOTE Raw-only and unresolved records require a later reconciliation pass.")
    emit("1 NOTE Family reference order is preserved from BKPERSON.DT5.")
    emit("1 NOTE {} non-empty ALTKEYS.DT5 slots are preserved without a person link.".format(
        len(altkeys)))
    for slot, text in altkeys:
        emit("1 CONT ALTKEYS slot {}: {}".format(slot, text))
    emit("1 NOTE {} unassigned BKMAIL.DT5 text runs are preserved.".format(
        len(mail_text)))
    for offset, text in mail_text:
        emit("1 CONT BKMAIL offset 0x{:x}: {}".format(offset, text))
    emit("1 NOTE {} marriage records lacked an unambiguous family ID and were not linked.".format(
        len(unresolved_marriages)))
    for spouse_1, spouse_2, date, common in unresolved_marriages:
        emit("1 CONT Unlinked BKMARR date={} spouse_ref_1={} spouse_ref_2={} common_family_refs={}".format(
            gedcom_date(date), spouse_1, spouse_2,
            ",".join("0x{:x}".format(value) for value in common) or "none"))
    emit("1 NOTE BK5 date slots 1-4 are exported as BIRT, BAPM, DEAT, and BURI;")
    emit("1 NOTE slot 5 remains an audit note because it is the other-date field.")
    emit("1 CHAR UTF-8")

    for source_id, description in sorted(sources.items()):
        emit("0 @S{}@ SOUR".format(source_id))
        emit("1 TITL BK5 source {}: {}".format(source_id, description))

    for person_id in sorted(people):
        person = people[person_id]
        emit("0 @I{}@ INDI".format(person_id))
        emit("1 NAME {}".format(person["name"]))
        if person["sex"]:
            emit("1 SEX {}".format(person["sex"]))
        if person["occupation"]:
            emit("1 OCCU {}".format(person["occupation"]))
        for note in person["notes"]:
            emit("1 NOTE BK5: {}".format(note))
        for slot, location_id in enumerate(person["location_refs"], 1):
            if not location_id:
                continue
            description = locations.get(location_id, "[not decoded]")
            emit("1 NOTE BK5 location slot {} ({}): {}".format(
                slot, location_id, description))
        for slot, source_id in enumerate(person["source_refs"], 1):
            if not source_id:
                continue
            description = sources.get(source_id, "[not decoded]")
            emit("1 NOTE BK5 source slot {} ({}): {}".format(
                slot, source_id, description))
        for slot, offset in enumerate(DATE_SLOTS):
            date = person["dates"].get(offset)
            if not date:
                continue
            tag = DATE_TAGS[slot]
            if tag is None:
                emit("1 NOTE BK5 date slot {} ({}): {}".format(
                    slot + 1, offset, gedcom_date(date)))
                continue
            emit("1 {}".format(tag))
            emit("2 DATE {}".format(gedcom_date(date)))
            if slot < len(person["location_refs"]):
                location_id = person["location_refs"][slot]
                if location_id:
                    emit("2 PLAC {}".format(
                        locations.get(location_id, "[not decoded]")))
        for offset, date in sorted(person["dates"].items()):
            if offset not in DATE_SLOTS:
                emit("1 NOTE BK5 date at {}: {}".format(
                    offset, gedcom_date(date)))
        refs = person["families"]
        roles = family_roles(person_id, refs, marriages)
        for family_id, role in roles:
            emit("1 {} {}".format(role, family_tag(family_id)))
        if len(roles) < len([value for value in refs if value]):
            emit("1 NOTE Additional BK5 family references: {}".format(
                ", ".join(family_tag(value) for value in refs
                          if value and family_tag(value) not in
                          {family_tag(family_id) for family_id, _ in roles})))
        for family_id in marriage_families_by_person.get(person_id, []):
            if len(refs) < 2 or refs[1] != family_id:
                emit("1 FAMS {}".format(family_tag(family_id)))
        emit("1 NOTE BK5 fixed record address 0x{}; expanded length {}".format(
            person["address"], person["length"]))
        if person_id in raw_by_id:
            raw = raw_by_id[person_id]
            emit("1 NOTE BK5 raw record offset {}".format(raw["offset"]))
            emit("1 NOTE BK5 raw name: {}".format(raw["raw_name"]))
            for date_offset, date in raw["dates"]:
                emit("1 NOTE BK5 raw date at {}: {}".format(
                    date_offset, gedcom_date(date)))
            extra_families = [family_id for family_id in raw["families"]
                              if family_id and family_id not in refs]
            if extra_families:
                emit("1 NOTE BK5 raw family references not present in fixed record: {}".format(
                    ", ".join(family_tag(value)
                              for value in extra_families)))

    for person in raw_unmerged:
        emit("0 @I{}@ INDI".format(person["xref"]))
        emit("1 NAME {}".format(raw_display_name(person)))
        emit("1 NOTE BK5 raw name: {}".format(person["raw_name"]))
        if person["sex"]:
            emit("1 SEX {}".format(person["sex"]))
        for date_offset, date in person["dates"]:
            emit("1 NOTE BK5 raw date at {}: {}".format(
                date_offset, gedcom_date(date)))
        refs = person["families"]
        if person["person_id"] is not None and refs:
            for family_id, role in family_roles(person["person_id"],
                                                 refs, marriages):
                emit("1 {} {}".format(role, family_tag(family_id)))
            for family_id in refs[2:]:
                if family_id:
                    emit("1 NOTE Additional BK5 raw family reference {}".format(
                        family_tag(family_id)))
        else:
            for family_id in refs:
                if family_id:
                    emit("1 NOTE BK5 raw family reference {} (role unresolved)".format(
                        family_tag(family_id)))
        emit("1 NOTE BK5 raw person record offset {}".format(person["offset"]))

    for person_id in sorted(placeholders):
        emit("0 @I{}@ INDI".format(person_id))
        emit("1 NOTE Unresolved BK5 marriage spouse reference {}".format(
            person_id))

    family_ids = set(marriages)
    for person in people.values():
        family_ids.update(value for value in person["families"] if value)
    for person in raw_unmerged:
        family_ids.update(value for value in person["families"] if value)
    for family_id in sorted(family_ids):
        emit("0 {} FAM".format(family_tag(family_id)))
        spouses = []
        children = []
        for person_id, person in sorted(people.items()):
            refs = person["families"]
            for reference, role in family_roles(person_id, refs, marriages):
                if reference != family_id:
                    continue
                if role == "FAMS":
                    spouses.append((person_id, person["sex"]))
                elif role == "FAMC":
                    children.append(person_id)
        for person in raw_unmerged:
            refs = person["families"]
            if person["person_id"] is not None:
                for reference, role in family_roles(person["person_id"],
                                                     refs, marriages):
                    if reference != family_id:
                        continue
                    if role == "FAMS":
                        spouses.append((person["person_id"], person["sex"]))
                    elif role == "FAMC":
                        children.append(person["person_id"])
        for person_id, sex in spouses:
            emit("1 {} @I{}@".format("HUSB" if sex == "M" else "WIFE"
                                      if sex == "F" else "NOTE", person_id))
        for person_id in children:
            emit("1 CHIL @I{}@".format(person_id))
        if family_id in marriages:
            emit("1 MARR")
            emit("2 DATE {}".format(
                gedcom_date(marriages[family_id]["date"])))
            if marriages[family_id]["inferred_family_id"]:
                emit("1 NOTE {}".format(marriages[family_id]["inference"]))
            known_spouses = {person_id for person_id, _ in spouses}
            for index, person_id in enumerate(marriages[family_id]["spouses"]):
                if person_id in known_spouses:
                    continue
                role = "HUSB" if index == 0 else "WIFE"
                emit("1 {} @I{}@".format(role, person_id))
        if not spouses or not children:
            emit("1 NOTE Incomplete indexed view; consult raw BK5 records.")

    emit("0 @NUNRESOLVED@ NOTE")
    emit("1 TEXT {} indexed entries were not decoded or were not named.".format(
        len(unresolved)))
    for line in unresolved:
        emit("1 CONT {}".format(line))
    emit("0 TRLR")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        print("bk5-indexed-to-gedcom: {}".format(error), file=sys.stderr)
        sys.exit(1)
