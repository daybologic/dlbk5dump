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

# Compare extraction reports and exit statuses with the independent reference.
set -eu
repo=$(pwd)
audit_tmp=$(mktemp -d /tmp/dlbk5dump-check-audit.XXXXXX)
cleanup() {
	find "$audit_tmp" -type l -delete
	find "$audit_tmp" -type f -delete
	find "$audit_tmp" -depth -type d -empty -delete
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$audit_tmp/BK5"
for input in REPORTS BKLOCATE BKPERSON ALTKEYS BKSOURCE BKOPTION BKMARR BKMAIL; do
	ln -s "$repo/BK5/$input.DT5" "$audit_tmp/BK5/$input.DT5"
done
ln -s "$repo/dlbk5dump" "$audit_tmp/dlbk5dump"
./dlbk5dump --gedcom > "$audit_tmp/base.ged"
./dlbk5dump --gedcom --clean > "$audit_tmp/clean.ged"

compare_audit() {
	label=$1
	expected_status=$2
	mode=${3:-file}
	cp "$audit_tmp/input.ged" "$audit_tmp/input.before"
	python_status=0
	cpp_status=0
	if [ "$mode" = stdin ]; then
		(cd "$audit_tmp" && "$repo/tools/bk5-audit.py") < "$audit_tmp/input.ged" > "$audit_tmp/python.txt" 2> "$audit_tmp/python.err" || python_status=$?
		(cd "$audit_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --audit) < "$audit_tmp/input.ged" > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
	else
		(cd "$audit_tmp" && "$repo/tools/bk5-audit.py" input.ged) > "$audit_tmp/python.txt" 2> "$audit_tmp/python.err" || python_status=$?
		(cd "$audit_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --audit input.ged) > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
	fi
	cmp "$audit_tmp/input.before" "$audit_tmp/input.ged"
	cmp "$audit_tmp/python.txt" "$audit_tmp/cpp.txt"
	if [ "$python_status" -ne "$expected_status" ] || [ "$cpp_status" -ne "$expected_status" ]; then
		printf '%s: expected status %s, Python=%s C++=%s\n' "$label" "$expected_status" "$python_status" "$cpp_status" >&2
		exit 1
	fi
	# Reported audit failures have no stderr; file/encoding errors use different
	# diagnostics, but both implementations must fail without a partial report.
	if [ -s "$audit_tmp/python.txt" ]; then
		cmp "$audit_tmp/python.err" "$audit_tmp/cpp.err"
		test ! -s "$audit_tmp/cpp.err"
	else
		test -s "$audit_tmp/python.err"
		test -s "$audit_tmp/cpp.err"
	fi
	printf '%s: identical report and status %s; input unchanged\n' "$label" "$cpp_status"
}

cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
compare_audit 'Bundled audit GEDCOM, file input without executables on PATH' 0
cp "$audit_tmp/cpp.txt" "$audit_tmp/expected.txt"
compare_audit 'Bundled audit GEDCOM, stdin without executables on PATH' 0 stdin

# Pin each failing condition, plus nonfatal counts, independently.
sed '/^0 @F1@ FAM$/d; /^0 @F2@ FAM$/d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Missing families and sorted missing/dangling identifiers' 1
cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
printf '1 CHIL @Iz@\n1 FAMC @Fzz@\n1 CHIL @Ia@\n1 CHIL @Iz@\n' >> "$audit_tmp/input.ged"
compare_audit 'Dangling references sorted and deduplicated' 1
sed '/^1 NOTE BK5 raw date at /d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Missing raw-date notes' 1
sed '/^[12] CONT BKMAIL offset /d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Missing mail notes' 1
sed '/^[12] CONT ALTKEYS slot /d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Missing ALTKEYS note' 1
cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
printf '2 CONT ALTKEYS slot extra\n' >> "$audit_tmp/input.ged"
compare_audit 'Extra ALTKEYS note retains exact-one requirement' 1
cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
printf '1 NOTE [not decoded]\n' >> "$audit_tmp/input.ged"
compare_audit 'Unresolved lookup note' 1

cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
printf '0 @I3@ INDI\n0 @F1@ FAM\n' >> "$audit_tmp/input.ged"
compare_audit 'Duplicate individual/family counts remain nonfatal' 0
test "$(sed -n 's/.*duplicate_individuals=\([0-9]*\) duplicate_families=\([0-9]*\)/\1 \2/p' "$audit_tmp/cpp.txt")" = '1 1'
sed '/^2 DATE /d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Ordinary date count remains nonfatal' 0
cp "$audit_tmp/clean.ged" "$audit_tmp/input.ged"
compare_audit 'Clean export lacks required extraction notes' 1
: > "$audit_tmp/input.ged"
compare_audit 'Empty GEDCOM' 1

awk '{ printf "%s\r\n", $0 }' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'CRLF line endings' 0
cmp "$audit_tmp/expected.txt" "$audit_tmp/cpp.txt"
tr '\n' '\r' < "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'CR-only line endings' 0
cmp "$audit_tmp/expected.txt" "$audit_tmp/cpp.txt"
# Python splitlines also accepts NEL, Unicode separators and control separators.
awk '{ printf "%s%c", $0, 28 }' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Control line separators' 0
cmp "$audit_tmp/expected.txt" "$audit_tmp/cpp.txt"
cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
printf '0 @I\303\251@ INDI\302\2051 CHIL @I\303\251@\342\200\2500 @I\360\237\230\200@ INDI\342\200\2511 CHIL @I\360\237\230\200@\n' >> "$audit_tmp/input.ged"
# The regular expression allows NBSP inside IDs; Python split() still splits it.
printf '0 @Ix\302\240y@ INDI\n1 CHIL @Ix\302\240y@\n' >> "$audit_tmp/input.ged"
compare_audit 'UTF-8 identifiers, Unicode lines and whitespace' 1

for bytes in '\377' '\302' '\300\257' '\355\240\200' '\364\220\200\200' '\342A\200'; do
	cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
	printf '%b' "$bytes" >> "$audit_tmp/input.ged"
	compare_audit "Invalid UTF-8 $bytes" 1
done

# Native stdin deliberately uses the same strict UTF-8 policy as file input.
cpp_status=0
PATH=/nonexistent ./dlbk5dump --audit < "$audit_tmp/input.ged" > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
test "$cpp_status" -eq 1
test ! -s "$audit_tmp/cpp.txt"
test -s "$audit_tmp/cpp.err"
printf 'Invalid UTF-8 on native stdin: status 1 and no partial report\n'

# Change only temporary fixture entries; never write through database symlinks.
cp "$audit_tmp/base.ged" "$audit_tmp/input.ged"
cp "$repo/BK5/BKMAIL.DT5" "$audit_tmp/mail.before"
rm "$audit_tmp/BK5/BKMAIL.DT5"
printf '\000\377' > "$audit_tmp/BK5/BKMAIL.DT5"
compare_audit 'Binary-only mail fixture changes coverage count' 1
rm "$audit_tmp/BK5/BKMAIL.DT5"
compare_audit 'Missing mail input' 1
ln -s "$repo/BK5/BKMAIL.DT5" "$audit_tmp/BK5/BKMAIL.DT5"
cmp "$audit_tmp/mail.before" "$repo/BK5/BKMAIL.DT5"
rm "$audit_tmp/BK5/BKPERSON.DT5"
compare_audit 'Missing person input' 1
ln -s "$repo/BK5/BKPERSON.DT5" "$audit_tmp/BK5/BKPERSON.DT5"

# One synthetic key-0 entry points to an uncompressed/empty fixed record.
# Decoder failure counts alone must not turn a complete extraction audit red.
rm "$audit_tmp/BK5/BKPERSON.DT5"
dd if=/dev/zero of="$audit_tmp/BK5/BKPERSON.DT5" bs=1024 count=2 2>/dev/null
printf '\000\000\052\000' | dd of="$audit_tmp/BK5/BKPERSON.DT5" bs=1 seek=1028 conv=notrunc 2>/dev/null
printf '\000\000\000\001' | dd of="$audit_tmp/BK5/BKPERSON.DT5" bs=1 seek=1040 conv=notrunc 2>/dev/null
sed '/^1 NOTE BK5 raw date at /d' "$audit_tmp/base.ged" > "$audit_tmp/input.ged"
compare_audit 'Undecodable-record count remains nonfatal' 0
test "$(sed -n 's/.* undecodable=\([0-9]*\) .*/\1/p' "$audit_tmp/cpp.txt")" = 1
rm "$audit_tmp/BK5/BKPERSON.DT5"
ln -s "$repo/BK5/BKPERSON.DT5" "$audit_tmp/BK5/BKPERSON.DT5"

# File failures differ in diagnostic wording, but not status or report output.
for path in nonexistent BK5; do
	python_status=0
	cpp_status=0
	(cd "$audit_tmp" && "$repo/tools/bk5-audit.py" "$path") > "$audit_tmp/python.txt" 2> "$audit_tmp/python.err" || python_status=$?
	(cd "$audit_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --audit "$path") > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
	test "$python_status" -eq 1
	test "$cpp_status" -eq 1
	cmp "$audit_tmp/python.txt" "$audit_tmp/cpp.txt"
	test ! -s "$audit_tmp/cpp.txt"
	test -s "$audit_tmp/python.err"
	test -s "$audit_tmp/cpp.err"
done
printf 'Missing GEDCOM and directory input: matching failure status and empty stdout\n'

for argument in --unknown extra; do
	cpp_status=0
	./dlbk5dump --audit input.ged "$argument" > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
	test "$cpp_status" -eq 2
	test ! -s "$audit_tmp/cpp.txt"
	test -s "$audit_tmp/cpp.err"
done
cpp_status=0
./dlbk5dump --audit --unknown > "$audit_tmp/cpp.txt" 2> "$audit_tmp/cpp.err" || cpp_status=$?
test "$cpp_status" -eq 2
test ! -s "$audit_tmp/cpp.txt"
test -s "$audit_tmp/cpp.err"
printf 'Invalid native arguments: status 2 and empty stdout\n'

PATH=/nonexistent ./dlbk5dump --gedcom | PATH=/nonexistent ./dlbk5dump --audit > "$audit_tmp/pipeline.txt"
cmp "$audit_tmp/expected.txt" "$audit_tmp/pipeline.txt"
printf 'Native export-to-audit pipeline works with no executables on PATH\n'
