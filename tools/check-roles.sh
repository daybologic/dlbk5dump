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
roles_tmp=$(mktemp -d /tmp/dlbk5dump-check-roles.XXXXXX)
trap 'find "$roles_tmp" -type f -delete; find "$roles_tmp" -depth -type d -empty -delete' EXIT
trap 'exit 1' HUP INT TERM
compare() {
	expected=$1
	py_status=0
	cpp_status=0
	(cd "$roles_tmp" && "$repo/tools/gedcom-roles.py" input.ged) > "$roles_tmp/python.txt" 2> "$roles_tmp/python.err" || py_status=$?
	(cd "$roles_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --gedcom-roles input.ged) > "$roles_tmp/cpp.txt" 2> "$roles_tmp/cpp.err" || cpp_status=$?
	cmp "$roles_tmp/python.txt" "$roles_tmp/cpp.txt"
	test "$py_status" -eq "$expected"
	test "$cpp_status" -eq "$expected"
	if [ "$expected" -eq 0 ]; then
		cmp "$roles_tmp/python.err" "$roles_tmp/cpp.err"
		test ! -s "$roles_tmp/cpp.err"
	else
		test -s "$roles_tmp/python.err"
		test -s "$roles_tmp/cpp.err"
	fi
}
"$repo/dlbk5dump" --gedcom > "$roles_tmp/input.ged"
cp "$roles_tmp/input.ged" "$roles_tmp/before"
compare 0
cmp "$roles_tmp/before" "$roles_tmp/input.ged"
"$repo/dlbk5dump" --gedcom --clean > "$roles_tmp/input.ged"
compare 0
: > "$roles_tmp/input.ged"
compare 0
printf 'Audit/clean exports and empty input: byte parity, standalone native execution\n'

cat > "$roles_tmp/input.ged" <<'GEDCOM'
0 HEAD
0 @I1@ INDI
1 NAME Parent /Test/
1 BIRT
2 DATE 1930
0 @I2@ INDI
1 NAME Unknown birth
0 @I3@ INDI
1 NAME Replaced
1 BIRT
2 DATE 1900
0 @I3@ INDI
1 NAME Léa /Test/
1 BIRT
2 DATE BEF 1935
0 @I4@ INDI
1 NAME Baptism fallback
1 BIRT
2 PLAC Unknown
1 BAPM
2 DATE ABT 1945
0 @I5@ INDI
1 NAME Zero-year fallback
1 BIRT
2 DATE 0000
1 BAPM
2 DATE 0000
0 @F1@ FAM
1 HUSB @I1@
1 WIFE @I2@
1 CHIL @I3@
1 CHIL @I4@
1 MARR
2 DATE 1960
0 @F2@ FAM
1 HUSB @I1@
1 CHIL @I3@
1 MARR
2 DATE 1950
0 @F3@ FAM
1 HUSB @I1@
1 WIFE @I5@
1 CHIL @I3@
0 @F4@ FAM
1 HUSB @I1@
1 MARR
2 DATE 1940
3 SOUR @ignored@
2 DATE 2000
1 CHIL @I3@
0 @Fprefix@ SOUR
1 HUSB @I1@
1 HUSB @I1@
1 CHIL @I3@
0 HEAD
1 HUSB @MissingIgnored@
0 TRLR
GEDCOM
compare 0
cp "$roles_tmp/input.ged" "$roles_tmp/lf"
awk '{printf "%s\r\n", $0}' "$roles_tmp/lf" > "$roles_tmp/input.ged"
compare 0
tr '\n' '\r' < "$roles_tmp/lf" > "$roles_tmp/input.ged"
compare 0
printf 'Multiple flags, unknown/zero years, baptism fallback, duplicates, prefix rules and newlines: parity\n'

# Exact thresholds are strict: 15 years before marriage and a 12-year spouse
# gap do not warn; 16 and 11 years do. First four decimal digits win.
{
	printf '0 @Ip@ INDI\n1 BIRT\n2 DATE 1900\n'
	for spec in 'a 1911 1927' 'b 1912 1927' 'c 1885 1900' 'd 1884 1900'; do
		set -- $spec
		printf '0 @I%s@ INDI\n1 BIRT\n2 DATE %s\n0 @F%s@ FAM\n1 HUSB @Ip@\n1 CHIL @I%s@\n1 MARR\n2 DATE %s\n' "$1" "$2" "$1" "$1" "$3"
	done
	printf '0 @Iu@ \316\261\n1 NAME Unicode\n1 BAPM\n2 DATE \331\241\331\251\331\240\331\245\n'
	printf '0 @Fu@ FAM\n1 HUSB @Ip@\n1 CHIL @Iu@\n1 MARR\n2 DATE \331\241\331\251\331\244\331\240\n'
} > "$roles_tmp/input.ged"
compare 0
printf 'Boundary inequalities, Unicode record tags and decimal years: parity\n'

# Preserve partial output when a later unresolved pointer aborts the reference.
printf '0 @Fbroken@ FAM\n1 CHIL @Missing@\n' >> "$roles_tmp/input.ged"
compare 1
test -s "$roles_tmp/cpp.txt"
printf 'Unresolved pointer after warnings: matching partial stdout and status 1\n'
printf '0 @Fbad@ FAM\n1 HUSB\n' > "$roles_tmp/input.ged"
compare 1
test ! -s "$roles_tmp/cpp.txt"
printf '\377' > "$roles_tmp/input.ged"
compare 1
rm "$roles_tmp/input.ged"
compare 1
test ! -s "$roles_tmp/cpp.txt"
status=0
"$repo/dlbk5dump" --gedcom-roles > "$roles_tmp/cpp.txt" 2> "$roles_tmp/cpp.err" || status=$?
test "$status" -eq 2
test ! -s "$roles_tmp/cpp.txt"
printf 'Malformed pointers, UTF-8 and missing files: failure parity; native usage returns 2\n'
