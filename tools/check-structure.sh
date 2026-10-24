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
structure_tmp=$(mktemp -d /tmp/dlbk5dump-check-structure.XXXXXX)
trap 'find "$structure_tmp" -type f -delete; find "$structure_tmp" -depth -type d -empty -delete' EXIT
trap 'exit 1' HUP INT TERM

compare() {
	expected=$1
	py_status=0
	cpp_status=0
	cp "$structure_tmp/input.ged" "$structure_tmp/before"
	(cd "$structure_tmp" && "$repo/tools/gedcom-check.py" input.ged) > "$structure_tmp/python.txt" 2> "$structure_tmp/python.err" || py_status=$?
	(cd "$structure_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --check-gedcom input.ged) > "$structure_tmp/cpp.txt" 2> "$structure_tmp/cpp.err" || cpp_status=$?
	cmp "$structure_tmp/python.txt" "$structure_tmp/cpp.txt"
	cmp "$structure_tmp/before" "$structure_tmp/input.ged"
	test "$py_status" -eq "$expected"
	test "$cpp_status" -eq "$expected"
	if [ "$expected" -eq 0 ]; then
		cmp "$structure_tmp/python.err" "$structure_tmp/cpp.err"
		test ! -s "$structure_tmp/cpp.err"
	else
		test ! -s "$structure_tmp/cpp.txt"
		test -s "$structure_tmp/python.err"
		test -s "$structure_tmp/cpp.err"
	fi
}
"$repo/dlbk5dump" --gedcom > "$structure_tmp/input.ged"
compare 0
"$repo/dlbk5dump" --gedcom --clean > "$structure_tmp/input.ged"
compare 0
: > "$structure_tmp/input.ged"
compare 0
printf 'Bundled exports and empty input: byte parity without BK5 or executables on PATH\n'

cat > "$structure_tmp/input.ged" <<'GEDCOM'
0 HEAD
1 SUBM @U1@
0 @U1@ SUBM
1 NAME Submitter
0 @I1@ INDI
1 NAME Replaced
0 @I1@ INDI
1 NAME O'Neil "quoted" /Test/
1 SEX F
1 BIRT
2 DATE 2000
1 FAMS @F1@
1 FAMC @F1@
1 FAMC @Fmissing@
0 @I2@ INDI
1 NAME No surname?
1 SEX M
1 BAPM
2 DATE ABT 1900
0 INDI
1 SEX U
0 @F1@ FAM
1 HUSB @I1@
1 HUSB @I2@
1 WIFE @I2@
1 CHIL @I1@
1 CHIL @I2@
1 CHIL @Imissing@
1 MARR
2 DATE SEPT 1901
3 SOUR @Smissing@
nonsense
5 NOTE jump
0 @Fempty@ FAM
0 @Fsmall@ FAM
1 CHIL @I1@
0 @Iodd@ INDI
1 NAME Léa /Test/
1 NAME ? //
1 BIRT
2 DATE (uncertain)
0 TRLR
GEDCOM
compare 0
cp "$structure_tmp/input.ged" "$structure_tmp/lf"
awk '{printf "%s\r\n", $0}' "$structure_tmp/lf" > "$structure_tmp/input.ged"
compare 0
tr '\n' '\r' < "$structure_tmp/lf" > "$structure_tmp/input.ged"
compare 0
printf 'Nonfatal findings: syntax, levels, duplicates, pointers, reciprocity, names, roles, dates and cycles\n'

# Exercise output limits, stable Counter tie ordering, chronological gaps,
# Unicode codepoint line lengths, quotes, controls and nonprintable escapes.
{
	printf '0 HEAD\n'
	n=1
	while [ "$n" -le 40 ]; do
		printf 'invalid %s\n0 @I%s@ INDI\n1 NAME ?Person%s\n1 BIRT\n2 DATE 1900\n1 SEX F\n' "$n" "$n" "$n"
		printf '0 @F%s@ FAM\n1 HUSB @I%s@\n1 CHIL @I%s@\n1 NOTE @Missing%s@\n' "$n" "$n" "$n" "$n"
		n=$((n + 1))
	done
	printf '0 @Iunicode@ INDI\n1 NAME '
	n=0
	while [ "$n" -lt 256 ]; do printf '\303\251'; n=$((n + 1)); done
	printf '\n1 BIRT\n2 DATE \331\241\331\240 JAN \331\241\331\251\331\240\331\240\n'
	printf '1 NAME A\302\240B\342\200\213C\356\200\200D\360\237\230\200\n'
	printf '1 NAME a\001b\177c\302\205d\342\200\250e\n'
	printf '\331\241 NOTE Unicode decimal level\n'
	printf '1 \342\200\213TAG control-format-tag\n'
	printf '0 @Fnew@ FAM\n1 HUSB @Iunicode@\n1 CHIL @I1@\n'
} > "$structure_tmp/input.ged"
compare 0
printf 'Report limits, Counter ties, Unicode digits/length/escaping and chronology: parity\n'

# The checker alone intentionally replaces invalid UTF-8, as its reference does.
{
	printf '0 HEAD\n'
	printf '1 NAME \377\302 \300\257 \355\240\200 \364\220\200\200 \342A\200\n'
	printf '0 @I3@ INDI\n1 NAME Bad\377Name\n'
} > "$structure_tmp/input.ged"
compare 0
printf 'UTF-8 replacement policy: byte parity\n'
printf '1 NAME before any record\n' > "$structure_tmp/input.ged"
compare 1
rm "$structure_tmp/input.ged"
py_status=0
cpp_status=0
"$repo/tools/gedcom-check.py" "$structure_tmp/input.ged" > "$structure_tmp/python.txt" 2> "$structure_tmp/python.err" || py_status=$?
PATH=/nonexistent "$repo/dlbk5dump" --check-gedcom "$structure_tmp/input.ged" > "$structure_tmp/cpp.txt" 2> "$structure_tmp/cpp.err" || cpp_status=$?
test "$py_status" -eq 1
test "$cpp_status" -eq 1
test ! -s "$structure_tmp/python.txt"
test ! -s "$structure_tmp/cpp.txt"
test -s "$structure_tmp/python.err"
test -s "$structure_tmp/cpp.err"
cpp_status=0
"$repo/dlbk5dump" --check-gedcom > "$structure_tmp/cpp.txt" 2> "$structure_tmp/cpp.err" || cpp_status=$?
test "$cpp_status" -eq 2
test ! -s "$structure_tmp/cpp.txt"
printf 'Unreadable input and data before first record: matching failures; native usage returns 2\n'
