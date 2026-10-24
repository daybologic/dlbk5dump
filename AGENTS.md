# Repository Guidelines

## Project Structure & Module Organization

This repository contains a C++ reverse-engineering utility for BK5 family-history data. Application entry code is in `src/dlbk5dump.cpp`; person, marriage, lookup, ALTKEYS, options, mail, and Btrieve decoding live in their corresponding `src/` modules. Native GEDCOM export is in `src/gedcom.cpp`; extraction auditing is in `src/audit.cpp`. Both share in-process dump capture through `src/diagnostic.cpp` and CP437, Unicode whitespace/line handling, and diagnostic quoting through `src/text.cpp`. The `BK5/` directory holds the legacy database and program files used as runtime input, while `bk5.iso` is the source disk image. `tools/` contains the retained Python references and independent audit tools.

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
./dlbk5dump --dump-altkeys      # inspect non-empty 162-byte ALTKEYS slots
./dlbk5dump --dump-options      # compare BKOPTION.DT5 with BKOPTION.BAK
./dlbk5dump --dump-mail         # inspect unassigned BKMAIL text runs
./dlbk5dump --family 0x9a       # inspect one family in raw person data
./dlbk5dump --gedcom           # native C++ audit GEDCOM on stdout
./dlbk5dump --gedcom --clean   # native C++ GEDCOM without extraction notes
./dlbk5dump --audit indexed.ged # compare an extraction with the BK5 inputs
./dlbk5dump --gedcom | ./dlbk5dump --audit # stdin audit, entirely native
make -s gedcom > indexed.ged   # -s keeps compiler commands out of the GEDCOM
make gedcom-file
make gedcom-clean-file          # indexed-clean.ged, without extraction notes
make audit                    # native export and extraction audit
make check-gedcom              # byte-for-byte C++/Python parity, both modes
make check-altkeys             # native/Python ALTKEYS parity and edge cases
make check-options             # native/Python options parity and edge cases
make check-mail                # mail report/input parity and edge cases
make check-audit               # extraction report and exit-status parity
./dlbk5dump --ancestry indexed.ged --person 3 --terminals
make check-ancestry            # both ancestry modes, every exported person
./dlbk5dump --check-gedcom indexed.ged
make check-structure           # structural report parity; not export parity
```

The program checks for required `.DT5` files before reading them. `--dump-id-records` first reads the key-0 index, then scans physical 29-byte fixed-record slots to recover valid person numbers that are absent from that index; it does not infer numbers from names or slot order. Records that cannot be reconstructed are reported as failures, not treated as deleted. `--dump-records` and `--family` scan raw variable pages and preserve another view of records. Use the bundled BK5 data for parity and smoke tests and inspect representative output.

`--gedcom` is a standalone C++ export; it never invokes Python or subprocesses.
It captures the existing person/family/lookup dump methods in memory so all
binary decoding remains shared. The diagnostic record formats are therefore
an internal export contract: update the native adapter and Python reference
together if those formats change. `tools/bk5-indexed-to-gedcom.py` remains the
reference implementation for `make check-gedcom`; Python is needed only for
reference comparisons and the remaining independent validators. Both
exporters produce an intermediate extraction that preserves BK5 addresses, unresolved date slots,
and raw-only records under synthetic `@IR...@` identifiers. `--clean` omits
extraction notes but retains genealogical notes and unresolved information.
Keep ordering, whitespace, UTF-8 encoding, note wrapping, citations, and final
newlines identical in both implementations. File targets replace the previous
GEDCOM only after a successful export.

`--audit [GEDCOM]` reads a UTF-8 file or stdin and compares it with the shared
person dumps and mail reader, without subprocesses. It needs the same DT5
inventory as the person decoder. Keep `tools/bk5-audit.py` as the independent
reference; run `make check-audit` after changing the audit, diagnostic formats
or shared helpers. Preserve report bytes, sorted identifiers and failure
conditions: missing families, dangling references, mismatched raw-date/mail
note counts, unresolved lookups, and an ALTKEYS note count other than one.
Duplicate IDs, ordinary date counts and undecodable-record counts are
reported but do not alone fail. The exact-one-ALTKEYS assumption is corpus
specific; this extraction audit is not a substitute for GEDCOM validation.
Clean exports intentionally lack its required notes. Native file/encoding
errors return 1 with concise stderr and no partial report; invalid arguments
return 2. Both native input modes require UTF-8; Python's stdin decoding
depends on its runtime environment. Tests compare file/stdin input, every
failure condition, nonfatal counters, Unicode/line endings, input errors and
native execution with no executables on `PATH`, using temporary fixtures.

Run `./dlbk5dump --dump-altkeys` when auditing `ALTKEYS.DT5`; preserve
non-empty slots as unassigned data unless a person identifier is independently
proven. This flag needs only `BK5/ALTKEYS.DT5` and is independent of other
database files. The native GEDCOM export shares the ALTKEYS reader.
Keep `tools/bk5-altkeys.py` as the independent reference and run
`make check-altkeys` after changing the reader or shared text helpers. The
check covers all CP437 bytes, quoting, whitespace, slot counts and boundaries,
empty/missing/malformed input, and execution without Python on `PATH`.
Run `./dlbk5dump --dump-mail` to inspect unassigned address-book text; it
needs only `BK5/BKMAIL.DT5`. Keep `tools/bk5-mail.py` as the independent
reference and run `make check-mail` after changing mail handling. The reader
is shared with GEDCOM export. Preserve the strict printable byte range
0x20-0x7e, space trimming, minimum trimmed length of three, EOF flushing,
Python-style quoting, and report ordering. The legacy mail report labels
each run's end offset (delimiter or EOF), while GEDCOM notes label its start;
retain both raw boundaries before trimming. Never attach mail text to a
person by name similarity alone. Tests compare stdout and successful stderr,
verify input bytes are unchanged, and check file errors and invalid arguments.
Native file errors use concise stderr messages rather than Python tracebacks.
Run `./dlbk5dump --dump-options` to compare `BKOPTION.DT5` with
`BKOPTION.BAK`; this flag needs only those two files. Configuration
differences must not be interpreted as genealogy records. Keep
`tools/bk5-options.py` as the reference and run `make check-options` after
changing the comparison. Preserve CP437 decoding, CR/LF/CRLF boundaries,
trailing-whitespace trimming, leading/key whitespace, last-value-wins
duplicates, first-'=' splitting, Unicode key ordering, and the literal
`<missing>` sentinel. A successful comparison returns 0 even with differences;
missing/unreadable input returns 1 and invalid arguments return 2. Read both
files before emitting output so input errors cannot leave a partial report.
`--ancestry GEDCOM [--person ID] [--terminals]` is standalone C++ and
needs only that GEDCOM, not BK5 inputs. `src/ancestry.cpp` uses the shared
UTF-8 and universal-newline reader in `src/gedcom_reader.cpp`. Preserve the
reference's explicit FAMC traversal, global visited set, input parent/family
ordering, unresolved links, duplicate record replacement and terminal
semantics (an existing parentless family is not a terminal person). Person
IDs are strings, defaulting to `3`; synthetic IDs are supported. Keep
`tools/bk5-ancestry.py` independent; run `make check-ancestry` after changes.
It compares tree/terminal output for every exported individual and edge
fixtures, including operation without BK5 or other executables on PATH.
Input errors return 1; missing people and invalid arguments return 2.
Native help/error text differs; use full option names rather than argparse
abbreviations. Do not silently repair permissive reference parsing rules.

`--check-gedcom GEDCOM` in `src/gedcom_check.cpp` ports the structural
checker. Preserve diagnostic ordering, output limits and Python container
formatting. Findings return 0, matching the reference; input/read failures
return 1 and native usage errors return 2. Only this checker uses UTF-8
replacement decoding. `src/unicode.cpp` freezes Unicode 15.1 printable,
word and decimal properties to avoid locale-dependent report changes.
Retain `tools/gedcom-check.py` independently and run `make check-structure`;
`make check-gedcom` remains the exporter parity test. Structural tests cover
both exports, all diagnostic groups, report limits, Counter ties, Unicode
digits/length/escaping, malformed UTF-8, errors, and standalone execution.

Run `./dlbk5dump --check-gedcom indexed.ged`, `./tools/gedcom-roles.py
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

Copy the complete text of `COPYING` verbatim into a comment at the top of
each source/build file. Keep any shebang on line 1 and place the notice
immediately below it; retain all notice text and line breaks.

## Testing Guidelines

Before submitting changes, run `make`, `make check-gedcom`, `make audit`, `git diff --check`, and at least `./dlbk5dump --dump-id-records` and `./dlbk5dump --dump-marriages` from the root. `make check-gedcom` compares both export modes against Python with `cmp`, checks native export without executables on `PATH`, and tests CP437 text, malformed/missing inputs, and invalid arguments using temporary fixtures. For export changes, also regenerate `indexed.ged` and run the three independent GEDCOM checks listed above. Preserve a copy of the previous GEDCOM before changing export behavior; require byte equality when porting behavior unchanged. Do not alter or delete the supplied legacy data. If parser behavior changes, include representative output and explain which BK5 record layout was validated.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects, consistent with the existing history (for example, `Recover files from Hg at ...`). Keep commits focused. Pull requests should describe the parser or data-format change, identify any assumptions about BK5 files, include the commands used for validation, and attach relevant sample output when behavior changes. Do not commit generated binaries or local BK5 data unless explicitly required.

Commits made by Codex must include this trailer:
`Co-Authored-By: Codex <noreply@openai.com>`.

## Data and Repository Hygiene

Treat `BK5/` and `bk5.iso` as input fixtures, not disposable build artifacts. Work from the repository root, avoid modifying legacy files during experiments, and keep compiled executables out of commits.
