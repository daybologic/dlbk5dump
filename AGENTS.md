# Repository Guidelines

## Project Structure & Module Organization

This repository contains a small C++ reverse-engineering utility for BK5 family-history data. Application entry code is in `src/dlbk5dump.cpp`; marriage-record parsing is split between `src/marriage.cpp` and `src/marriage.hpp`. The `BK5/` directory holds the legacy database and program files used as runtime input, while `bk5.iso` is the source disk image. `README` contains the project’s brief runtime requirement. There is currently no separate test, asset, or generated-output directory.

## Build, Test, and Development Commands

Build from the repository root with the checked-in GNU Makefile:

```sh
make
```

Run it from the repository root so its relative `BK5/...` paths resolve correctly. Useful extraction commands are:

```sh
./dlbk5dump --dump-id-index     # enumerate the key-0 person index
./dlbk5dump --dump-id-records   # decode indexed and unindexed fixed records
./dlbk5dump --dump-marriages    # inspect BKMARR.DT5 marriage records
./dlbk5dump --family 0x9a       # inspect one family in raw person data
make gedcom > indexed.ged
make gedcom-file
make audit
```

The program checks for required `.DT5` files before reading them. `--dump-id-records` first reads the key-0 index, then scans physical 29-byte fixed-record slots to recover valid person numbers that are absent from that index; it does not infer numbers from names or slot order. Records that cannot be reconstructed are reported as failures, not treated as deleted. `--dump-records` and `--family` scan raw variable pages and preserve another view of records. No automated test suite is currently provided; use the bundled BK5 data for smoke tests and inspect representative output.

`bk5-indexed-to-gedcom.py` is an intermediate export only. It preserves BK5 source addresses and unresolved date slots, and includes raw-only records under synthetic `@IR...@` identifiers. Their family roles and encoded names still require reconciliation before the GEDCOM is complete.

Run `./tools/bk5-altkeys.py` when auditing `ALTKEYS.DT5`; preserve non-empty
slots as unassigned data unless a person identifier is independently proven.
Run `./tools/bk5-mail.py` to inspect the unassigned address-book text in
`BKMAIL.DT5`; do not attach it to a person by name similarity alone.
Run `./tools/bk5-options.py` to compare `BKOPTION.DT5` with its backup;
configuration differences must not be interpreted as genealogy records.
Run `./tools/gedcom-check.py indexed.ged`, `./tools/gedcom-roles.py
indexed.ged` and `./tools/gedcom-places.py indexed.ged` as independent
checks of GEDCOM structure, family-role plausibility and place events.

Marriage IDs may be recovered only when both spouse records independently
share exactly one family reference. Ambiguous marriage scans must remain
explicitly unlinked and preserved as audit notes; do not assign a family by
date, name similarity, or record order.

It also preserves all five decoded location references from
`BK5/BKLOCATE.DT5` and source references from `BK5/BKSOURCE.DT5` as
slot-numbered notes. The validated date-slot order is birth, baptism, death,
burial, and other; the first four slots receive matching `PLAC` values when
present. Keep the raw slot notes for auditability.

Raw variable-page dates must also be retained as offset-labelled notes when
they cannot be mapped safely to a GEDCOM event; do not discard them merely
because the corresponding fixed record lacks the date.

Emit decoded source descriptions as standalone GEDCOM `SOUR` records, but
emit person-level `SOUR` pointers for decoded source slots, retain the
original BK5 source-slot notes, and do not invent event citations.

Treat `--dump-relations` output as unvalidated B-tree pointer candidates, not
as family relationships, unless an independent record-aware decoder proves
the linkage.

## Coding Style & Naming Conventions

Match the existing C++ style: tabs for indentation, braces on the same line, and explicit standard-library headers. Keep declarations in headers and implementations in corresponding `.cpp` files. Use descriptive PascalCase for existing types and methods (for example, `TReadMarriages::Load`) and preserve the existing uppercase field naming in legacy data structures. Avoid changing record offsets or binary layouts without documenting the format assumptions.

## Testing Guidelines

Before submitting changes, run `make`, `make audit`, `git diff --check`, and at least `./dlbk5dump --dump-id-records` and `./dlbk5dump --dump-marriages` from the root. Confirm missing-input behavior by running in a directory without `BK5/` only when needed; do not alter or delete the supplied legacy data. If parser behavior changes, include representative output and explain which BK5 record layout was validated.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects, consistent with the existing history (for example, `Recover files from Hg at ...`). Keep commits focused. Pull requests should describe the parser or data-format change, identify any assumptions about BK5 files, include the commands used for validation, and attach relevant sample output when behavior changes. Do not commit generated binaries or local BK5 data unless explicitly required.

Commits made by Codex must include this trailer:
`Co-Authored-By: Codex <noreply@openai.com>`.

## Data and Repository Hygiene

Treat `BK5/` and `bk5.iso` as input fixtures, not disposable build artifacts. Work from the repository root, avoid modifying legacy files during experiments, and keep compiled executables out of commits.
