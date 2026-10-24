# Python-to-C++ Conversion Plan

## Completed

| Python reference | Native command |
|---|---|
| `tools/bk5-indexed-to-gedcom.py` | `./dlbk5dump --gedcom [--clean]` |
| `tools/bk5-altkeys.py` | `./dlbk5dump --dump-altkeys` |
| `tools/bk5-options.py` | `./dlbk5dump --dump-options` |
| `tools/bk5-mail.py` | `./dlbk5dump --dump-mail` |
| `tools/bk5-audit.py` | `./dlbk5dump --audit [GEDCOM]` (stdin when omitted) |
| `tools/bk5-ancestry.py` | `./dlbk5dump --ancestry GEDCOM [--person ID] [--terminals]` |
| `tools/gedcom-check.py` | `./dlbk5dump --check-gedcom GEDCOM` |

Mail inspection and GEDCOM export share a reader that preserves both raw
boundaries: the reference mail report labels each run's end offset, while
GEDCOM notes label its start. `make check-mail` proves report parity and
unchanged input bytes; `make check-gedcom` also covers synthetic mail inputs
and long notes in both audit and clean modes. Mail entries remain unassigned.

`make audit` now exports and audits entirely in C++. `make check-audit`
compares report bytes and exit statuses with the retained Python reference,
including failure cases and nonfatal counters. The audit shares the person
dump capture, Unicode text helpers and mail reader with the exporter. It
preserves the exact-one-ALTKEYS-note requirement; duplicate IDs and decoder
failures remain report-only. Native errors are concise and invalid arguments
return 2; UTF-8 is required on both file input and stdin (Python's stdin
decoding depends on its runtime environment).

Ancestry now shares a UTF-8/universal-newline file reader with subsequent
validators. `make check-ancestry` proves both-mode parity for every exported
person and fixtures covering shared ancestors, cycles, unresolved links and
terminal semantics. Native argument/help errors use concise diagnostics and
require complete option names rather than argparse abbreviations.

The structural checker preserves report bytes, ordering, output limits and
report-only exit policy. `make check-structure` compares both exports and
all diagnostic groups, including malformed/Unicode input. Native Unicode
15.1 tables preserve Python decimal, word and repr semantics without locale
or runtime Python dependencies; the checker retains UTF-8 replacement mode.

## Remaining Work, in Recommended Order

1. **GEDCOM validation.** Reuse the GEDCOM reader for these remaining tools,
   creating and committing a separate feature branch for each conversion:

   | Python tool | Checks to preserve |
   |---|---|
   | `tools/gedcom-roles.py` | Suspicious child/spouse assignments, birth/marriage chronology, and one-spouse families |
   | `tools/gedcom-places.py` | Counts of events with places and place-only events |

## Conversion Requirements

- Keep each Python tool as an independent reference and compare native output
  byte for byte on the bundled data and relevant temporary fixtures.
- Preserve established output ordering, whitespace, escaping, encoding, CLI
  behaviour, and error handling; document any intentional differences.
- Use shared readers and text helpers rather than duplicating existing
  native parsing logic. Native commands must not invoke Python or subprocesses.
- Preserve genealogy uncertainties and audit notes. Do not infer family links
  from names, dates, slot order, or unvalidated index pointers.
- Retain the complete `COPYING` notice in every new source/build file,
  immediately below any shebang.
- Update `README`, `AGENTS.md`, build targets, and this plan as work completes.
- Run the applicable parity tests, required build/audit smoke checks, and
  `git diff --check`. Preserve a baseline GEDCOM and verify that unrelated
  conversions leave the export byte-for-byte unchanged.
