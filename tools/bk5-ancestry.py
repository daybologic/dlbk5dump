#!/usr/bin/env python3
# Family History Database Recovery Project (dlbk5dump)
# Copyright (c) 2012-2026, Rev. Duncan Ross Palmer (M6KVM, 2E0EOL),
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#     * Neither the name of the Daybo Logic nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

"""Audit explicit linked ancestors for one individual in a GEDCOM export."""

import argparse
import re


def read_gedcom(path):
    people = {}
    families = {}
    current = None
    for raw_line in open(path, encoding="utf-8"):
        line = raw_line.rstrip("\n")
        match = re.match(r"0 (@I[^ ]+@) INDI$", line)
        if match:
            current = match.group(1)
            people[current] = {"name": "", "events": [], "famc": [],
                               "notes": []}
            continue
        match = re.match(r"0 (@F[^ ]+@) FAM$", line)
        if match:
            current = match.group(1)
            families[current] = {"parents": [], "children": [], "date": ""}
            continue
        if current in people:
            if line.startswith("1 NAME "):
                people[current]["name"] = line[7:]
            elif re.match(r"1 (?:BIRT|BAPM|DEAT|BURI)$", line):
                people[current]["events"].append([line[2:], ""])
            elif line.startswith("2 DATE ") and people[current]["events"]:
                people[current]["events"][-1][1] = line[7:]
            elif line.startswith("1 FAMC "):
                people[current]["famc"].append(line[7:])
            elif line.startswith("1 NOTE "):
                people[current]["notes"].append(line[7:])
        elif current in families:
            match = re.match(r"1 (?:HUSB|WIFE) (@I[^ ]+@)$", line)
            if match:
                families[current]["parents"].append(match.group(1))
            else:
                match = re.match(r"1 CHIL (@I[^ ]+@)$", line)
                if match:
                    families[current]["children"].append(match.group(1))
                elif line.startswith("2 DATE "):
                    families[current]["date"] = line[7:]
    return people, families


def print_tree(person_id, people, families, depth, seen):
    person = people.get(person_id)
    if person is None:
        print("  " * depth + person_id + " [unresolved]")
        return
    event_text = "".join(" {} {}".format(tag.lower(), date)
                         for tag, date in person["events"] if date)
    print("  " * depth + "{} {}{}".format(person_id, person["name"], event_text))
    if person_id in seen:
        print("  " * (depth + 1) + "[cycle suppressed]")
        return
    seen.add(person_id)
    for family_id in person["famc"]:
        family = families.get(family_id)
        if family is None:
            print("  " * (depth + 1) + family_id + " [unresolved family]")
            continue
        marriage = (" marriage " + family["date"]) if family["date"] else ""
        print("  " * (depth + 1) + family_id + marriage)
        for parent_id in family["parents"]:
            print_tree(parent_id, people, families, depth + 2, seen)


def terminal_ancestors(person_id, people, families, seen=None):
    """Return linked ancestors whose records have no explicit FAMC."""
    if seen is None:
        seen = set()
    if person_id in seen:
        return []
    seen.add(person_id)
    person = people.get(person_id)
    if person is None:
        return []
    result = []
    followed = False
    for family_id in person["famc"]:
        family = families.get(family_id)
        if family is None:
            continue
        followed = True
        for parent_id in family["parents"]:
            result.extend(terminal_ancestors(parent_id, people, families, seen))
    if not followed:
        result.append(person_id)
    return result


def print_terminals(person_id, people, families):
    terminals = []
    for terminal_id in terminal_ancestors(person_id, people, families):
        if terminal_id not in terminals:
            terminals.append(terminal_id)
    print("TERMINAL_ANCESTORS count={}".format(len(terminals)))
    for terminal_id in terminals:
        person = people[terminal_id]
        events = ", ".join("{} {}".format(tag, date)
                            for tag, date in person["events"] if date)
        suffix = " events=" + events if events else ""
        print("{} name={}{}".format(terminal_id, person["name"], suffix))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("gedcom")
    parser.add_argument("--person", default="3", help="numeric BK5 person ID")
    parser.add_argument("--terminals", action="store_true",
                        help="list terminal ancestors with no linked parents")
    args = parser.parse_args()
    people, families = read_gedcom(args.gedcom)
    person_id = "@I{}@".format(args.person)
    if person_id not in people:
        parser.error("person {} is not present in {}".format(args.person,
                                                              args.gedcom))
    if args.terminals:
        print_terminals(person_id, people, families)
    else:
        print_tree(person_id, people, families, 0, set())


if __name__ == "__main__":
    main()
