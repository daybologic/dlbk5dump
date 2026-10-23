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

# Compare raw input handling and mail reports with the Python reference.
set -eu

repo=$(pwd)
mail_tmp=$(mktemp -d /tmp/dlbk5dump-check-mail.XXXXXX)
cleanup() {
	find "$mail_tmp" -type f -delete
	find "$mail_tmp" -depth -type d -empty -delete
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$mail_tmp/BK5"

compare_output() {
	cp "$mail_tmp/BK5/BKMAIL.DT5" "$mail_tmp/input.before"
	(cd "$mail_tmp" && "$repo/tools/bk5-mail.py") > "$mail_tmp/python.txt" 2> "$mail_tmp/python.err"
	cmp "$mail_tmp/input.before" "$mail_tmp/BK5/BKMAIL.DT5"
	(cd "$mail_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --dump-mail) > "$mail_tmp/cpp.txt" 2> "$mail_tmp/cpp.err"
	cmp "$mail_tmp/input.before" "$mail_tmp/BK5/BKMAIL.DT5"
	cmp "$mail_tmp/python.txt" "$mail_tmp/cpp.txt"
	cmp "$mail_tmp/python.err" "$mail_tmp/cpp.err"
	printf '%s: identical output; input bytes unchanged\n' "$1"
}

cp "$repo/BK5/BKMAIL.DT5" "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Bundled BKMAIL, with no other database files or executables'

{
	printf '\000a\000ab\000abc\000  ab   \000   abc  \000     \000'
	printf '%s\000' "'single'" '"double"' "both ' and \"" 'back\slash' 'a b'
	printf 'split\ttabs\r\nlines\177del\200high\377bytes\000'
	printf '%s' '  final run  '
} > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Minimum trimmed length, raw offsets, quoting, separators and EOF'

# Pin the legacy end-offset convention independently of either implementation.
printf '\000  abc  \000xy\000def' > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Padded run and EOF end offsets'
printf "BKMAIL_TEXT offset=0x8 text='abc'\nBKMAIL_TEXT offset=0xf text='def'\nBKMAIL_TEXT_RUNS count=2\n" > "$mail_tmp/expected.txt"
cmp "$mail_tmp/expected.txt" "$mail_tmp/cpp.txt"

# Surround every possible byte with text: printable bytes must join a run,
# while every control, DEL and high byte must split it at the exact offset.
{
	byte=0
	while [ "$byte" -lt 256 ]; do
		byte_octal=$(printf '%03o' "$byte")
		printf ' ABC'
		printf '%b' "\\0$byte_octal"
		printf 'XYZ \000'
		byte=$((byte + 1))
	done
} > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'All 256 byte values and printable-range boundaries'

{
	printf '%20000s' '' | tr ' ' X
	printf '\000tail'
} > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Long unbroken run and final unterminated run'

: > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Empty file'
printf '\000\001\037\177\200\377' > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Binary-only file'
printf '       ' > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Whitespace-only file'
printf ab > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Short unterminated run'
printf abc > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'Three-byte unterminated run'
printf '  abc  \000' > "$mail_tmp/BK5/BKMAIL.DT5"
compare_output 'NUL-terminated run with padding'

expect_input_failure() {
	if (cd "$mail_tmp" && "$repo/tools/bk5-mail.py") > "$mail_tmp/python.txt" 2> "$mail_tmp/python.err"; then
		printf 'Python reference unexpectedly accepted %s\n' "$1" >&2
		exit 1
	else
		test "$?" -eq 1
	fi
	if (cd "$mail_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --dump-mail) > "$mail_tmp/cpp.txt" 2> "$mail_tmp/cpp.err"; then
		printf 'Native command unexpectedly accepted %s\n' "$1" >&2
		exit 1
	else
		test "$?" -eq 1
	fi
	cmp "$mail_tmp/python.txt" "$mail_tmp/cpp.txt"
	test ! -s "$mail_tmp/cpp.txt"
	test -s "$mail_tmp/python.err"
	test -s "$mail_tmp/cpp.err"
	printf '%s: matching failure status and empty stdout\n' "$1"
}

mv "$mail_tmp/BK5/BKMAIL.DT5" "$mail_tmp/saved"
expect_input_failure 'Missing BKMAIL'
mkdir "$mail_tmp/BK5/BKMAIL.DT5"
expect_input_failure 'Directory instead of BKMAIL file'
if (cd "$mail_tmp" && "$repo/dlbk5dump" --dump-mail --unknown) > "$mail_tmp/output" 2> "$mail_tmp/error"; then
	printf 'Unknown mail argument unexpectedly succeeded\n' >&2
	exit 1
else
	test "$?" -eq 2
fi
test ! -s "$mail_tmp/output"
test -s "$mail_tmp/error"
printf 'Invalid arguments: status 2 without mail output\n'
