# Repository Guidelines

## Project Structure & Module Organization

This repository contains a C++ reverse-engineering utility for BK5 family-history data. Application entry code is in `src/dlbk5dump.cpp`; person, marriage, lookup, and Btrieve decoding live in their corresponding `src/` modules. Native GEDCOM export is in `src/gedcom.cpp` and `src/gedcom.hpp`. The `BK5/` directory holds the legacy database and program files used as runtime input, while `bk5.iso` is the source disk image. `tools/` contains the retained Python reference exporter and independent audit tools.

## Build, Test, and Development Commands

Build from the repository root with the checked-in GNU Makefile:

```sh
make
```

Run it from the repository root so its relative `BK5/...` paths resolve correctly. Useful extraction commands are:

```sh
./dlbk5dump --dump-id-index     # enumerate the key-0 person index
./dlbk5dump --dump-id-records   # decode indexed and unindexed fixed records
./dlbk5dump --dump-marriages    # legacy date-string scan of BKMARR.DT5
./dlbk5dump --dump-families     # decode all BKMARR.DT5 family records
./dlbk5dump --trace-family 31   # one family record, fields and raw bytes
./dlbk5dump --dump-sources      # decode all BKSOURCE.DT5 source records
./dlbk5dump --dump-locations    # decode all BKLOCATE.DT5 location records
./dlbk5dump --family 0x9a       # inspect one family in raw person data
./dlbk5dump --gedcom           # native C++ audit GEDCOM on stdout
./dlbk5dump --gedcom --clean   # native C++ GEDCOM without extraction notes
make -s gedcom > indexed.ged   # -s keeps compiler commands out of the GEDCOM
make gedcom-file
make gedcom-clean-file          # indexed-clean.ged, without extraction notes
make audit
make check-gedcom              # byte-for-byte C++/Python parity, both modes
```

The program checks for required `.DT5` files before reading them. `--dump-id-records` first reads the key-0 index, then scans physical 29-byte fixed-record slots to recover valid person numbers that are absent from that index; it does not infer numbers from names or slot order. Records that cannot be reconstructed are reported as failures, not treated as deleted. `--dump-records` and `--family` scan raw variable pages and preserve another view of records. Use the bundled BK5 data for parity and smoke tests and inspect representative output.

`--gedcom` is a standalone C++ export; it never invokes Python or subprocesses.
It captures the existing person/family/lookup dump methods in memory so all
binary decoding remains shared. The diagnostic record formats are therefore
an internal export contract: update the native adapter and Python reference
together if those formats change. `tools/bk5-indexed-to-gedcom.py` remains the
reference implementation for `make check-gedcom`; Python is needed only for
this comparison and the independent audits. Both exporters produce an
intermediate extraction that preserves BK5 addresses, unresolved date slots,
and raw-only records under synthetic `@IR...@` identifiers. `--clean` omits
extraction notes but retains genealogical notes and unresolved information.
Keep ordering, whitespace, UTF-8 encoding, note wrapping, citations, and final
newlines identical in both implementations. File targets replace the previous
GEDCOM only after a successful export.

Run `./tools/bk5-altkeys.py` when auditing `ALTKEYS.DT5`; preserve non-empty
slots as unassigned data unless a person identifier is independently proven.
Run `./tools/bk5-mail.py` to inspect the unassigned address-book text in
`BKMAIL.DT5`; do not attach it to a person by name similarity alone.
Run `./tools/bk5-options.py` to compare `BKOPTION.DT5` with its backup;
configuration differences must not be interpreted as genealogy records.
Run `./tools/gedcom-check.py indexed.ged`, `./tools/gedcom-roles.py
indexed.ged` and `./tools/gedcom-places.py indexed.ged` as independent
checks of GEDCOM structure, family-role plausibility and place events.

`BKMARR.DT5` is the BK5 family table: a compressed Btrieve file keyed by
family number, holding 167 fixed 242-byte family records (husband, wife,
marriage date and location, three source citations with detail and quality,
a status field, and the children). The layout is documented in
`src/marriage.cpp`; every indexed key matches its expanded record, and 148
families agreed exactly with the links decoded independently from
`BKPERSON.DT5` before the family table was used. Family membership in the
GEDCOM export comes from these records. Person-record family references that
the family table does not confirm must be kept as audit notes, not dropped
or used to create links. The status field holds BK5's documented
"Marr. ceremony Y/N", "Divorced/Annulled/Separated" and "End year" fields
(`BK5/CHANGES.DOC`); end codes other than D, A and S are kept as notes.
Never assign a family by date, name similarity, or record order.

It also preserves all five decoded location references from
`BK5/BKLOCATE.DT5` and source references from `BK5/BKSOURCE.DT5` as
slot-numbered notes. The validated date-slot order is birth, baptism, death,
burial, and other; the first four slots receive matching `PLAC` values when
present. Keep the raw slot notes for auditability.

Raw variable-page dates must also be retained as offset-labelled notes when
they cannot be mapped safely to a GEDCOM event; do not discard them merely
because the corresponding fixed record lacks the date.

Emit decoded source descriptions as standalone GEDCOM `SOUR` records and
retain the original BK5 source-slot notes. BK5 keeps one citation per
date/location slot (`BK5/CHANGES.DOC`: "3 full lines for each
date/location"), so person source slot n cites event slot n (birth,
baptism, death, burial, other); the source texts agree (all baptism
wording is in slot 2, death wording in slot 3). Place each person citation,
with its `PAGE` and `QUAY`, under its exported event; keep it on the person
when that event is not exported or is slot 5. Never create an event only to
hold a citation.

Treat `--dump-relations` output as unvalidated B-tree pointer candidates, not
as family relationships, unless an independent record-aware decoder proves
the linkage.

## Coding Style & Naming Conventions

Match the existing C++ style: tabs for indentation, braces on the same line, and explicit standard-library headers. Keep declarations in headers and implementations in corresponding `.cpp` files. Use descriptive PascalCase for existing types and methods (for example, `TReadMarriages::Load`) and preserve the existing uppercase field naming in legacy data structures. Avoid changing record offsets or binary layouts without documenting the format assumptions.

## Testing Guidelines

Before submitting changes, run `make`, `make check-gedcom`, `make audit`, `git diff --check`, and at least `./dlbk5dump --dump-id-records` and `./dlbk5dump --dump-marriages` from the root. `make check-gedcom` compares both export modes against Python with `cmp`, checks native export without executables on `PATH`, and tests CP437 text, malformed/missing inputs, and invalid arguments using temporary fixtures. For export changes, also regenerate `indexed.ged` and run the three independent GEDCOM checks listed above. Preserve a copy of the previous GEDCOM before changing export behavior; require byte equality when porting behavior unchanged. Do not alter or delete the supplied legacy data. If parser behavior changes, include representative output and explain which BK5 record layout was validated.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects, consistent with the existing history (for example, `Recover files from Hg at ...`). Keep commits focused. Pull requests should describe the parser or data-format change, identify any assumptions about BK5 files, include the commands used for validation, and attach relevant sample output when behavior changes. Do not commit generated binaries or local BK5 data unless explicitly required.

Commits made by Codex must include this trailer:
`Co-Authored-By: Codex <noreply@openai.com>`.

## Data and Repository Hygiene

Treat `BK5/` and `bk5.iso` as input fixtures, not disposable build artifacts. Work from the repository root, avoid modifying legacy files during experiments, and keep compiled executables out of commits.
