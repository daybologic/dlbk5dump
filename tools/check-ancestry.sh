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

# Compare ancestry output and statuses without needing BK5 or subprocesses.
set -eu
repo=$(pwd)
ancestry_tmp=$(mktemp -d /tmp/dlbk5dump-check-ancestry.XXXXXX)
trap 'find "$ancestry_tmp" -type f -delete; find "$ancestry_tmp" -depth -type d -empty -delete' EXIT
trap 'exit 1' HUP INT TERM

compare() {
	input=$1
	expected=$2
	shift 2
	py_status=0
	cpp_status=0
	(cd "$ancestry_tmp" && "$repo/tools/bk5-ancestry.py" "$input" "$@") > "$ancestry_tmp/python.txt" 2> "$ancestry_tmp/python.err" || py_status=$?
	(cd "$ancestry_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --ancestry "$input" "$@") > "$ancestry_tmp/cpp.txt" 2> "$ancestry_tmp/cpp.err" || cpp_status=$?
	cmp "$ancestry_tmp/python.txt" "$ancestry_tmp/cpp.txt"
	if [ "$py_status" -ne "$expected" ] || [ "$cpp_status" -ne "$expected" ]; then
		printf 'Ancestry status mismatch: expected %s, Python %s, C++ %s\n' "$expected" "$py_status" "$cpp_status" >&2
		exit 1
	fi
	if [ "$expected" -eq 0 ]; then
		cmp "$ancestry_tmp/python.err" "$ancestry_tmp/cpp.err"
		test ! -s "$ancestry_tmp/cpp.err"
	else
		test ! -s "$ancestry_tmp/cpp.txt"
		test -s "$ancestry_tmp/python.err"
		test -s "$ancestry_tmp/cpp.err"
	fi
}

"$repo/dlbk5dump" --gedcom > "$ancestry_tmp/input.ged"
cp "$ancestry_tmp/input.ged" "$ancestry_tmp/before"
sed -n 's/^0 @I\([^ ]*\)@ INDI$/\1/p' "$ancestry_tmp/input.ged" > "$ancestry_tmp/ids"
while IFS= read -r id; do
	compare "$ancestry_tmp/input.ged" 0 --person "$id"
	compare "$ancestry_tmp/input.ged" 0 --person="$id" --terminals
done < "$ancestry_tmp/ids"
cmp "$ancestry_tmp/before" "$ancestry_tmp/input.ged"
printf 'Every exported individual: tree and terminal output byte parity; no BK5 or executables on PATH\n'
"$repo/dlbk5dump" --gedcom --clean > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 0
compare "$ancestry_tmp/input.ged" 0 --terminals
printf 'Clean export: tree and terminal parity\n'

cat > "$ancestry_tmp/input.ged" <<'GEDCOM'
0 HEAD
0 @I3@ INDI
1 NAME Child /Example/
1 BIRT
2 DATE 2000
1 BAPM
2 DATE ABT 2001
1 FAMC @F1@
1 FAMC @F2@
1 FAMC @Fmissing@
0 @I1@ INDI
1 NAME Père /Example/
1 FAMC @Fcycle@
0 @I2@ INDI
1 NAME Leaf /Example/
1 DEAT
2 DATE (unknown)
0 @Iempty@ INDI
1 NAME No recorded parents
1 FAMC @Fempty@
0 @Ireset@ INDI
1 NAME First
0 @Ireset@ INDI
1 NAME Replacement
0 NOTE
1 NAME Reference keeps the last person active
0 @F1@ FAM
1 WIFE @I1@
1 HUSB @I2@
1 HUSB @Iunresolved@
1 MARR
2 DATE 1980
1 CHIL @I3@
0 @F2@ FAM
1 HUSB @I2@
0 @Fcycle@ FAM
1 HUSB @I3@
0 @Fempty@ FAM
0 TRLR
GEDCOM
for id in 3 1 2 empty reset; do
	compare "$ancestry_tmp/input.ged" 0 --person "$id"
	compare "$ancestry_tmp/input.ged" 0 --person "$id" --terminals
done
compare "$ancestry_tmp/input.ged" 0 --person=missing --person=2 --terminals
cp "$ancestry_tmp/input.ged" "$ancestry_tmp/lf"
awk '{printf "%s\r\n", $0}' "$ancestry_tmp/lf" > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 0
tr '\n' '\r' < "$ancestry_tmp/lf" > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 0 --terminals
# Keep an unterminated final line and Unicode separators inside a name.
printf '0 @I3@ INDI\n1 NAME Léa\342\200\250continues' > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 0
compare "$ancestry_tmp/input.ged" 0 --terminals
printf 'Cycles, shared ancestors, unresolved links, empty families, duplicates, Unicode and newline parity\n'

compare "$ancestry_tmp/input.ged" 2 --person nonexistent
compare "$ancestry_tmp/input.ged" 2 --unknown
compare "$ancestry_tmp/input.ged" 2 --person
compare "$ancestry_tmp/input.ged" 2 extra
compare "$ancestry_tmp/missing.ged" 1
compare "$ancestry_tmp" 1
printf '\377' > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 1
: > "$ancestry_tmp/input.ged"
compare "$ancestry_tmp/input.ged" 2
printf 'Input failures, absent people and invalid arguments: matching status and empty stdout\n'
