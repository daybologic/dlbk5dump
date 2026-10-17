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
FAMILY_RE = re.compile(
    r'^FAMILY_RECORD id=(\d+) address=0x([0-9a-f]+) indexed=(yes|no) '
    r'husband=(\d+) wife=(\d+) date_1="(.*?)" date_2="(.*?)" '
    r'location_refs=(\d+),(\d+) source_refs=(\d+),(\d+),(\d+) '
    r'source_details="(.*?)","(.*?)","(.*?)" '
    r'source_quality="(.*?)","(.*?)","(.*?)" status="(.*?)" '
    r'children=([\d,]*) stamp=(\S*) flag="(.*?)"$'
)
MONTHS = ("JAN", "FEB", "MAR", "APR", "MAY", "JUN",
          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC")
GEDCOM_DATE_RE = re.compile(
    r'^(?:\d{1,2} )?(?:(?:' + "|".join(MONTHS) + r') )?\d{3,4}$')


def gedcom_date(value):
    """Normalize the month in a BK5 date without changing source files.

    Dates GEDCOM cannot express exactly, such as BK5's uncertain years
    ("195?"), are kept verbatim as a GEDCOM date phrase in parentheses.
    """
    fields = value.strip().split()
    if len(fields) >= 2 and fields[-2][:3].upper() in MONTHS:
        fields[-2] = fields[-2][:3].upper()
    date = " ".join(fields)
    if date and not GEDCOM_DATE_RE.match(date):
        return "({})".format(date)
    return date


def gedcom_name(value):
    """Mark the final name component as the GEDCOM surname."""
    name = value.strip()
    if name.startswith("THIS INFORMATION"):
        return "//"
    if "/" in name:
        return name
    fields = name.split()
    if len(fields) < 2:
        return name
    if "?" in fields[-1] or fields[-1].startswith("("):
        return "{} //".format(" ".join(fields[:-1]))
    return "{} /{}/".format(" ".join(fields[:-1]), fields[-1])


def name_requires_note(value):
    """Return whether the original name needs preserving as an audit note."""
    name = value.strip()
    return (name.startswith("THIS INFORMATION")
            or (len(name.split()) >= 2
                and ("?" in name.split()[-1]
                     or name.split()[-1].startswith("("))))


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


QUOTED = r'"((?:[^"\\]|\\.)*)"'
SOURCE_RE = re.compile(
    r'^SOURCE_RECORD id=(\d+) address=0x[0-9a-f]+ '
    r'line_1=' + QUOTED + r' line_2=' + QUOTED + r' line_3=' + QUOTED + r'$')
LOCATION_RE = re.compile(
    r'^LOCATION_RECORD id=(\d+) address=0x[0-9a-f]+ name=' + QUOTED + r'$')


def unquote(value):
    """Undo the backslash escaping used by the dlbk5dump text dumps."""
    return re.sub(r'\\(.)', r'\1', value)


def parse_sources(text):
    """Read BKSOURCE.DT5 records decoded by --dump-sources.

    Each source holds up to three 78-column lines; blank lines are dropped.
    """
    sources = {}
    for line in text.splitlines():
        match = SOURCE_RE.match(line)
        if match:
            lines = [unquote(value) for value in match.groups()[1:]]
            sources[int(match.group(1))] = [value for value in lines if value]
    return sources


def parse_locations(text):
    """Read BKLOCATE.DT5 records decoded by --dump-locations."""
    locations = {}
    for line in text.splitlines():
        match = LOCATION_RE.match(line)
        if match:
            locations[int(match.group(1))] = unquote(match.group(2))
    return locations


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
    if raw_name.startswith("*I") and len(raw_name) > 3:
        candidate = raw_name[2:-1].strip()
        if candidate and candidate[0].isalpha() and " " in candidate:
            return candidate
    if raw_name.startswith("*") or len(raw_name) <= 11:
        return raw_name
    candidate = raw_name[10:-1].strip()
    if candidate and candidate[0].isalpha() and " " in candidate:
        return candidate
    return raw_name


def parse_families(text):
    """Read the BKMARR.DT5 family records decoded by --dump-families."""
    families = {}
    for line in text.splitlines():
        match = FAMILY_RE.match(line)
        if not match:
            continue
        values = match.groups()
        families[int(values[0])] = {
            "address": values[1],
            "husband": int(values[3]),
            "wife": int(values[4]),
            "dates": [values[5], values[6]],
            "location_refs": [int(values[7]), int(values[8])],
            "source_refs": [int(value) for value in values[9:12]],
            "source_details": list(values[12:15]),
            "source_quality": list(values[15:18]),
            "status": values[18],
            "children": [int(value) for value in values[19].split(',')
                         if value],
            "stamp": values[20],
        }
    return families


def family_tag(family_id):
    return "@F{:x}@".format(family_id)


def family_links(families):
    """Map each person number to its (family, FAMC|FAMS) memberships."""
    links = {}
    for family_id, family in sorted(families.items()):
        for person_id in (family["husband"], family["wife"]):
            if person_id:
                links.setdefault(person_id, []).append((family_id, "FAMS"))
        for person_id in family["children"]:
            links.setdefault(person_id, []).append((family_id, "FAMC"))
    return links


def emit_family_links(person_id, refs, links):
    """Emit FAMC/FAMS from BKMARR, noting person references it lacks."""
    order = {value: index for index, value in enumerate(refs) if value}
    memberships = sorted(links.get(person_id, []),
                         key=lambda item: (item[1] != "FAMC",
                                           order.get(item[0], len(order)),
                                           item[0]))
    for family_id, role in memberships:
        emit("1 {} {}".format(role, family_tag(family_id)))
    confirmed = {family_id for family_id, _ in memberships}
    unconfirmed = [value for value in refs if value and value not in confirmed]
    if unconfirmed:
        emit_note("BK5 person family references not listed in BKMARR.DT5: "
                  "{}".format(", ".join(family_tag(value)
                                        for value in unconfirmed)))


DATE_SLOTS = ("0x111", "0x120", "0x12f", "0x13e", "0x14d")
DATE_TAGS = ("BIRT", "BAPM", "DEAT", "BURI", None)


def emit(line=""):
    print(line)


def note_chunks(text, first_prefix_length, rest_prefix_length):
    """Split note text so each complete GEDCOM line stays within 255 chars.

    BK5 pads note fields with spaces, so trailing padding is dropped.  GEDCOM
    5.5.1 requires CONC splits to fall between two non-space characters,
    because readers may trim leading or trailing line whitespace; a split
    point is therefore moved back until both neighbours are non-space.
    """
    text = str(text).rstrip()
    chunks = []
    prefix_length = first_prefix_length
    while True:
        width = 255 - prefix_length
        if width < 2:
            raise ValueError("GEDCOM note prefix is too long")
        if len(text) <= width:
            chunks.append(text)
            return chunks
        split = width
        while split > 1 and (text[split - 1] == " " or text[split] == " "):
            split -= 1
        if split == 1:
            split = width
        chunks.append(text[:split])
        text = text[split:]
        prefix_length = rest_prefix_length


BK5_NOTE_LINE_WIDTH = 78
BK5_NOTE_LINES = 7


def format_bk5_note(note):
    """Restore the line layout of a fixed-width BK5 person note.

    Every observed BK5 note field is 554 bytes: seven 78-column text lines
    followed by an eight-digit YYYYMMDD stamp.  Text is padded with spaces
    to each line boundary rather than wrapped mid-word, so the lines become
    GEDCOM continuation lines.  The stamp is kept, labelled but uninterpreted.
    Notes that do not match this layout are returned unchanged.
    """
    text_length = BK5_NOTE_LINE_WIDTH * BK5_NOTE_LINES
    stamp = note[text_length:]
    if len(note) != text_length + 8 or not re.fullmatch(r"\d{8}", stamp):
        return note
    lines = [note[offset:offset + BK5_NOTE_LINE_WIDTH].rstrip()
             for offset in range(0, text_length, BK5_NOTE_LINE_WIDTH)]
    while lines and not lines[-1]:
        lines.pop()
    lines.append("[BK5 note stamp {}]".format(stamp))
    return "\n".join(lines)


def emit_note(text, level=1):
    """Emit one NOTE and CONC records without exceeding GEDCOM line limits."""
    lines = str(text).splitlines() or [""]
    note_prefix = "{} NOTE ".format(level)
    cont_prefix = "{} CONT ".format(level + 1)
    conc_prefix = "{} CONC ".format(level + 1)
    for index, line in enumerate(lines):
        prefix = note_prefix if index == 0 else cont_prefix
        chunks = note_chunks(line, len(prefix), len(conc_prefix))
        emit(prefix + chunks[0])
        for chunk in chunks[1:]:
            emit(conc_prefix + chunk)


def emit_note_record(xref, lines):
    """Emit an independent NOTE record with valid text and continuations."""
    lines = [str(line) for line in lines] or [""]
    note_prefix = "0 @{}@ NOTE ".format(xref)
    for index, line in enumerate(lines):
        prefix = note_prefix if index == 0 else "1 CONT "
        chunks = note_chunks(line, len(prefix), len("1 CONC "))
        emit(prefix + chunks[0])
        for chunk in chunks[1:]:
            emit("1 CONC " + chunk)


def main():
    people, unresolved = parse_people(run_dump("--dump-id-records"))
    locations = parse_locations(run_dump("--dump-locations"))
    source_lines = parse_sources(run_dump("--dump-sources"))
    sources = {source_id: " / ".join(lines)
               for source_id, lines in source_lines.items()}
    altkeys = parse_altkeys("BK5/ALTKEYS.DT5")
    mail_text = parse_printable_runs("BK5/BKMAIL.DT5")
    raw_people = parse_raw_people(run_dump("--dump-records"))
    families = parse_families(run_dump("--dump-families"))
    links = family_links(families)
    raw_by_id = {person["person_id"]: person for person in raw_people
                 if person["person_id"] is not None}
    raw_unmerged = [person for person in raw_people
                    if person["person_id"] is None
                    or person["person_id"] not in people]
    raw_unmerged_ids = {person["person_id"] for person in raw_unmerged}
    placeholders = sorted(person_id for person_id in links
                          if person_id not in people
                          and person_id not in raw_unmerged_ids)

    emit("0 HEAD")
    emit("1 SOUR dlbk5dump")
    emit("1 GEDC")
    emit("2 VERS 5.5.1")
    emit("2 FORM LINEAGE-LINKED")
    header_notes = [
        "This is an intermediate export of decoded BK5 fixed records.",
        "Raw-only and unresolved records require a later reconciliation pass.",
        "Families, spouses, children and marriage details come from the",
        "{} BKMARR.DT5 family records; person-record family references".format(
            len(families)),
        "that BKMARR.DT5 does not confirm are kept as notes.",
        "{} non-empty ALTKEYS.DT5 slots are preserved without a person link.".format(
            len(altkeys)),
    ]
    for slot, text in altkeys:
        header_notes.append("ALTKEYS slot {}: {}".format(slot, text))
    header_notes.append("{} unassigned BKMAIL.DT5 text runs are preserved.".format(
        len(mail_text)))
    for offset, text in mail_text:
        header_notes.append("BKMAIL offset 0x{:x}: {}".format(offset, text))
    header_notes.extend([
        "BK5 date slots 1-4 are exported as BIRT, BAPM, DEAT, and BURI;",
        "slot 5 remains an audit note because it is the other-date field.",
    ])
    emit_note("\n".join(header_notes))
    emit("1 CHAR UTF-8")

    for source_id, lines in sorted(source_lines.items()):
        emit("0 @S{}@ SOUR".format(source_id))
        lines = lines or [""]
        emit("1 TITL BK5 source {}: {}".format(source_id, lines[0]).rstrip())
        for line in lines[1:]:
            emit("2 CONT {}".format(line))

    for person_id in sorted(people):
        person = people[person_id]
        emit("0 @I{}@ INDI".format(person_id))
        emit("1 NAME {}".format(gedcom_name(person["name"])))
        if name_requires_note(person["name"]):
            emit_note("BK5 original name: {}".format(person["name"]))
        if person["sex"]:
            emit("1 SEX {}".format(person["sex"]))
        if person["occupation"]:
            emit("1 OCCU {}".format(person["occupation"]))
        for note in person["notes"]:
            emit_note("BK5: {}".format(format_bk5_note(note)))
        for slot, location_id in enumerate(person["location_refs"], 1):
            if not location_id:
                continue
            description = locations.get(location_id, "[not decoded]")
            emit_note("BK5 location slot {} ({}): {}".format(
                slot, location_id, description))
        for slot, source_id in enumerate(person["source_refs"], 1):
            if not source_id:
                continue
            description = sources.get(source_id, "[not decoded]")
            if source_id in sources:
                emit("1 SOUR @S{}@".format(source_id))
            emit_note("BK5 source slot {} ({}): {}".format(
                slot, source_id, description))
        for slot, offset in enumerate(DATE_SLOTS):
            date = person["dates"].get(offset)
            tag = DATE_TAGS[slot]
            if tag is None:
                if date:
                    emit_note("BK5 date slot {} ({}): {}".format(
                        slot + 1, offset, gedcom_date(date)))
                continue
            location_id = (person["location_refs"][slot]
                           if slot < len(person["location_refs"])
                           else 0)
            if not date and not location_id:
                continue
            emit("1 {}".format(tag))
            if date:
                emit("2 DATE {}".format(gedcom_date(date)))
            if location_id:
                emit("2 PLAC {}".format(
                    locations.get(location_id, "[not decoded]")))
        for offset, date in sorted(person["dates"].items()):
            if offset not in DATE_SLOTS:
                emit_note("BK5 date at {}: {}".format(
                    offset, gedcom_date(date)))
        refs = person["families"]
        emit_family_links(person_id, refs, links)
        emit_note("BK5 fixed record address 0x{}; expanded length {}".format(
            person["address"], person["length"]))
        if person_id in raw_by_id:
            raw = raw_by_id[person_id]
            emit_note("BK5 raw record offset {}".format(raw["offset"]))
            emit_note("BK5 raw name: {}".format(raw["raw_name"]))
            for date_offset, date in raw["dates"]:
                emit_note("BK5 raw date at {}: {}".format(
                    date_offset, gedcom_date(date)))
            extra_families = [family_id for family_id in raw["families"]
                              if family_id and family_id not in refs]
            if extra_families:
                emit_note("BK5 raw family references not present in fixed record: {}".format(
                    ", ".join(family_tag(value)
                              for value in extra_families)))

    for person in raw_unmerged:
        emit("0 @I{}@ INDI".format(person["xref"]))
        emit("1 NAME {}".format(gedcom_name(raw_display_name(person))))
        emit_note("BK5 raw name: {}".format(person["raw_name"]))
        if person["sex"]:
            emit("1 SEX {}".format(person["sex"]))
        for date_offset, date in person["dates"]:
            emit_note("BK5 raw date at {}: {}".format(
                date_offset, gedcom_date(date)))
        refs = person["families"]
        if person["person_id"] is not None:
            emit_family_links(person["person_id"], refs, links)
        else:
            for family_id in refs:
                if family_id:
                    emit_note("BK5 raw family reference {} (role unresolved)".format(
                        family_tag(family_id)))
        emit_note("BK5 raw person record offset {}".format(person["offset"]))

    for person_id in placeholders:
        emit("0 @I{}@ INDI".format(person_id))
        emit("1 NAME //")
        emit_family_links(person_id, [], links)
        emit_note("BK5 person {} is listed in BKMARR.DT5 but its person "
                  "record has no decoded name.".format(person_id))

    for family_id, family in sorted(families.items()):
        emit("0 {} FAM".format(family_tag(family_id)))
        if family["husband"]:
            emit("1 HUSB @I{}@".format(family["husband"]))
        if family["wife"]:
            emit("1 WIFE @I{}@".format(family["wife"]))
        for person_id in family["children"]:
            emit("1 CHIL @I{}@".format(person_id))
        date, second_date = family["dates"]
        location_id, second_location_id = family["location_refs"]
        if date or location_id:
            emit("1 MARR")
            if date:
                emit("2 DATE {}".format(gedcom_date(date)))
            if location_id:
                emit("2 PLAC {}".format(
                    locations.get(location_id, "[not decoded]")))
        if location_id:
            emit_note("BK5 family location slot 1 ({}): {}".format(
                location_id, locations.get(location_id, "[not decoded]")))
        if second_date or second_location_id:
            emit_note("BK5 family second date/location slot: {} {}".format(
                gedcom_date(second_date), second_location_id))
        for slot, source_id in enumerate(family["source_refs"]):
            detail = family["source_details"][slot]
            quality = family["source_quality"][slot]
            if not source_id:
                if detail or quality:
                    emit_note("BK5 family citation slot {} without source: "
                              "detail={!r} quality={!r}".format(
                                  slot + 1, detail, quality))
                continue
            if source_id in sources:
                emit("1 SOUR @S{}@".format(source_id))
                if detail:
                    emit("2 PAGE {}".format(detail))
                if quality in ("0", "1", "2", "3"):
                    emit("2 QUAY {}".format(quality))
            emit_note("BK5 family source slot {} ({}): {}".format(
                slot + 1, source_id,
                sources.get(source_id, "[not decoded]")))
        if family["status"]:
            emit_note("BK5 family status field (meaning unproven): {}".format(
                family["status"]))
        emit_note("BK5 family record address 0x{}; stamp {}".format(
            family["address"], family["stamp"]))

    emit_note_record("NUNRESOLVED", [
        "{} indexed entries were not decoded or were not named.".format(
            len(unresolved)),
        *unresolved,
    ])
    emit("0 TRLR")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        print("bk5-indexed-to-gedcom: {}".format(error), file=sys.stderr)
        sys.exit(1)
