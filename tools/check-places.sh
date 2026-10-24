#!/bin/sh
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

set -eu
repo=$(pwd)
places_tmp=$(mktemp -d /tmp/dlbk5dump-check-places.XXXXXX)
trap 'find "$places_tmp" -type f -delete; find "$places_tmp" -depth -type d -empty -delete' EXIT
trap 'exit 1' HUP INT TERM
compare() {
	expected=$1
	py_status=0
	cpp_status=0
	(cd "$places_tmp" && "$repo/tools/gedcom-places.py" input.ged) > "$places_tmp/python.txt" 2> "$places_tmp/python.err" || py_status=$?
	(cd "$places_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --gedcom-places input.ged) > "$places_tmp/cpp.txt" 2> "$places_tmp/cpp.err" || cpp_status=$?
	cmp "$places_tmp/python.txt" "$places_tmp/cpp.txt"
	test "$py_status" -eq "$expected"
	test "$cpp_status" -eq "$expected"
	if [ "$expected" -eq 0 ]; then
		cmp "$places_tmp/python.err" "$places_tmp/cpp.err"
		test ! -s "$places_tmp/cpp.err"
	else
		test ! -s "$places_tmp/cpp.txt"
		test -s "$places_tmp/python.err"
		test -s "$places_tmp/cpp.err"
	fi
}
"$repo/dlbk5dump" --gedcom > "$places_tmp/input.ged"
cp "$places_tmp/input.ged" "$places_tmp/before"
compare 0
cmp "$places_tmp/before" "$places_tmp/input.ged"
"$repo/dlbk5dump" --gedcom --clean > "$places_tmp/input.ged"
compare 0
: > "$places_tmp/input.ged"
compare 0
printf 'Audit/clean exports and empty input: byte parity, standalone native execution\n'

cat > "$places_tmp/input.ged" <<'GEDCOM'
0 @I3@ INDI
1 BIRT
2 DATE 1982
2 PLAC Example
2 PLAC Duplicate counts once
1 BAPM
2 PLAC Place only
1 DEAT
2 DATE 1994
3 SOUR @S1@
2 PLAC After level 3 is ignored
1 BURI
2 PLAC Before level 3 counts
3 NOTE nested
2 DATE After level 3 is ignored
1 MARR
2 PLAC Not one of the four person events
1 BIRT
2 PLACEMENT Prefix is intentionally counted
2 DATELESS Prefix is intentionally counted
1 BIRT
2 PLAC Empty value is counted
1 NOTE ends scan
2 DATE ignored
1 BAPM
2 plac lowercase ignored
1 DEAT_TRAILING
2 PLAC trailing event space is ignored
1 BURI
2 NOTE Léa
2 PLAC_EMPTY
0 @F1@ FAM
1 BIRT
2 PLAC Reference scans all records
0 TRLR
GEDCOM
sed 's/^1 DEAT_TRAILING$/1 DEAT /;s/^2 PLAC_EMPTY$/2 PLAC /' "$places_tmp/input.ged" > "$places_tmp/adjusted"
mv "$places_tmp/adjusted" "$places_tmp/input.ged"
compare 0
printf 'events with PLAC 7 place-only 5\n' > "$places_tmp/expected.txt"
cmp "$places_tmp/expected.txt" "$places_tmp/cpp.txt"
cp "$places_tmp/input.ged" "$places_tmp/lf"
awk '{printf "%s\r\n", $0}' "$places_tmp/lf" > "$places_tmp/input.ged"
compare 0
tr '\n' '\r' < "$places_tmp/lf" > "$places_tmp/input.ged"
compare 0
printf 'Exact event tags, four-character prefixes, duplicate places, nesting boundaries and newlines: parity\n'
printf '1 BIRT\n2 PLAC Léa\342\200\250Example' > "$places_tmp/input.ged"
compare 0
printf '\377' > "$places_tmp/input.ged"
compare 1
rm "$places_tmp/input.ged"
compare 1
status=0
"$repo/dlbk5dump" --gedcom-places > "$places_tmp/cpp.txt" 2> "$places_tmp/cpp.err" || status=$?
test "$status" -eq 2
test ! -s "$places_tmp/cpp.txt"
printf 'Unterminated Unicode line, invalid UTF-8 and file failures: parity; native usage returns 2\n'
