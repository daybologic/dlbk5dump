# Python-to-C++ Conversion Plan

## Completed

| Python reference | Native command |
|---|---|
| `tools/bk5-indexed-to-gedcom.py` | `./dlbk5dump --gedcom [--clean]` |
| `tools/bk5-altkeys.py` | `./dlbk5dump --dump-altkeys` |
| `tools/bk5-options.py` | `./dlbk5dump --dump-options` |

## Remaining Work, in Recommended Order

1. **Mail inspection — `tools/bk5-mail.py`.** Extract the scanning logic
   already used by the native GEDCOM exporter into a shared reader. Add
   `--dump-mail`, preserving offsets, quoted text, and summary counts. The
   current database yields 43 text runs. Keep entries unassigned unless
   ownership is independently proven.

2. **Extraction audit — `tools/bk5-audit.py`.** Port decoded-record and date
   coverage checks, raw-date preservation, mail/ALTKEYS note coverage,
   unresolved lookups, duplicate identifiers, missing families, and dangling
   references. Preserve failure conditions and exit status. Switch
   `make audit` to the native implementation once validated, removing Python
   from the ordinary audit workflow.

3. **Ancestry — `tools/bk5-ancestry.py`.** Port ancestor-tree traversal and
   terminal-ancestor listing, preserving person selection, family identifiers,
   unresolved records, and cycle handling. Follow explicit parent-family links
   only. Establish a shared native GEDCOM reader for subsequent validators.

4. **GEDCOM validation.** Reuse the GEDCOM reader for these remaining tools:

   | Python tool | Checks to preserve |
   |---|---|
   | `tools/gedcom-check.py` | Syntax, nesting, line lengths, submitter, identifiers, pointers, reciprocal links, names, dates, family roles, and chronology |
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
