#!/bin/sh
# The retained Python exporter is the independent formatting reference.
# Run from the repository root; use only temporary outputs and input copies.
set -eu

repo=$(pwd)
parity_dir=$(mktemp -d /tmp/dlbk5dump-check-gedcom.XXXXXX)
cleanup() {
	# TemporaryDirectory-style cleanup without a recursive deletion command.
	find "$parity_dir" -type l -delete
	find "$parity_dir" -type f -delete
	find "$parity_dir" -depth -type d -empty -delete
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

compare_exports() {
	label=$1
	shift
	"$repo/tools/bk5-indexed-to-gedcom.py" "$@" > "$parity_dir/python.ged"
	# Translate the reference's optional --clean to the native CLI.
	"$repo/dlbk5dump" --gedcom "$@" > "$parity_dir/cpp.ged"
	cmp "$parity_dir/python.ged" "$parity_dir/cpp.ged"
	printf '%s: byte-for-byte match\n' "$label"
}

compare_exports 'Audit export'
compare_exports 'Clean export' --clean

# The native export must not need Python, a shell, or another executable.
PATH=/nonexistent "$repo/dlbk5dump" --gedcom > "$parity_dir/standalone.ged"
"$repo/tools/bk5-indexed-to-gedcom.py" > "$parity_dir/python.ged"
cmp "$parity_dir/python.ged" "$parity_dir/standalone.ged"
printf 'Native export with no executables on PATH: byte-for-byte match\n'

mkdir "$parity_dir/fixture" "$parity_dir/fixture/BK5" "$parity_dir/empty"
ln -s "$repo/dlbk5dump" "$parity_dir/fixture/dlbk5dump"
for name in BKPERSON BKMARR BKLOCATE BKSOURCE BKMAIL BKOPTION REPORTS; do
	ln -s "$repo/BK5/$name.DT5" "$parity_dir/fixture/BK5/$name.DT5"
done

# Exercise CP437 -> UTF-8, non-breaking whitespace, line breaks, escaping,
# and trailing spaces even when the bundled ALTKEYS slot contains only ASCII.
(
	cd "$parity_dir/fixture"
	printf '\377\202\234 quoted "text"\r\nand backslash \\ '
	i=0
	while [ "$i" -lt 100 ]; do printf '\202'; i=$((i + 1)); done
	printf '\377<'
	printf '%19s' ''
) > "$parity_dir/fixture/BK5/ALTKEYS.DT5"
# Pad this synthetic slot to the required 162 bytes.
slot_bytes=$(wc -c < "$parity_dir/fixture/BK5/ALTKEYS.DT5")
slot_padding=$((162 - slot_bytes))
test "$slot_padding" -ge 0
if [ "$slot_padding" -gt 0 ]; then
	printf '%*s' "$slot_padding" '' >> "$parity_dir/fixture/BK5/ALTKEYS.DT5"
fi
(
	cd "$parity_dir/fixture"
	compare_exports 'CP437 fixture audit export'
	compare_exports 'CP437 fixture clean export' --clean
)

# Decode errors and missing fixtures must fail with no partial GEDCOM.
printf x >> "$parity_dir/fixture/BK5/ALTKEYS.DT5"
if (cd "$parity_dir/fixture" && "$repo/dlbk5dump" --gedcom) > "$parity_dir/failure.ged" 2> "$parity_dir/error"; then
	printf 'Malformed ALTKEYS unexpectedly succeeded\n' >&2
	exit 1
fi
test ! -s "$parity_dir/failure.ged"
printf 'Previous complete GEDCOM\n' > "$parity_dir/previous.ged"
for filename in indexed.ged indexed-clean.ged; do
	cp "$parity_dir/previous.ged" "$parity_dir/fixture/$filename"
done
for target in gedcom-file gedcom-clean-file; do
	if (cd "$parity_dir/fixture" && make -s -f "$repo/Makefile" SOURCES= "$target") > "$parity_dir/failure.ged" 2> "$parity_dir/error"; then
		printf 'File target %s unexpectedly succeeded on malformed inputs\n' "$target" >&2
		exit 1
	fi
done
cmp "$parity_dir/previous.ged" "$parity_dir/fixture/indexed.ged"
cmp "$parity_dir/previous.ged" "$parity_dir/fixture/indexed-clean.ged"
printf 'File targets preserve the previous GEDCOM on export failure\n'
if (cd "$parity_dir/empty" && "$repo/dlbk5dump" --gedcom) > "$parity_dir/failure.ged" 2> "$parity_dir/error"; then
	printf 'Missing inputs unexpectedly succeeded\n' >&2
	exit 1
fi
test ! -s "$parity_dir/failure.ged"
if "$repo/dlbk5dump" --gedcom --unknown > "$parity_dir/failure.ged" 2> "$parity_dir/error"; then
	printf 'Unknown GEDCOM option unexpectedly succeeded\n' >&2
	exit 1
else
	test "$?" -eq 2
fi
test ! -s "$parity_dir/failure.ged"
printf 'Malformed/missing inputs and invalid options: rejected without GEDCOM output\n'
