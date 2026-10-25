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

# Compare the native BKOPTION report with the retained Python reference.
set -eu

repo=$(pwd)
options_tmp=$(mktemp -d /tmp/dlbk5dump-check-options.XXXXXX)
cleanup() {
	find "$options_tmp" -type f -delete
	find "$options_tmp" -depth -type d -empty -delete
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$options_tmp/BK5"

compare_output() {
	(cd "$options_tmp" && "$repo/tools/bk5-options.py") > "$options_tmp/python.txt"
	(cd "$options_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --dump-options) > "$options_tmp/cpp.txt"
	cmp "$options_tmp/python.txt" "$options_tmp/cpp.txt"
	printf '%s: byte-for-byte match\n' "$1"
}

expect_failure() {
	label=$1
	expected=$2
	shift 2
	if (cd "$options_tmp" && "$repo/dlbk5dump" --dump-options "$@") > "$options_tmp/output" 2> "$options_tmp/error"; then
		printf '%s unexpectedly succeeded\n' "$label" >&2
		exit 1
	else
		test "$?" -eq "$expected"
	fi
	test ! -s "$options_tmp/output"
	test -s "$options_tmp/error"
	printf '%s: rejected without comparison output\n' "$label"
}

cp "$repo/BK5/BKOPTION.DT5" "$options_tmp/BK5/BKOPTION.DT5"
cp "$repo/BK5/BKOPTION.BAK" "$options_tmp/BK5/BKOPTION.BAK"
compare_output 'Bundled options, with no other database files or executables'

{
	printf 'z=last\r\nsame=equal\ndup=first\rdup=last\nempty=\nignored line\n'
	printf ' lead key =  leading value  \t\r\nmulti=a=b=c\n=empty key\n'
	printf '#comment=still a key\ncase=lower\nCase=upper\nsentinel=<missing>\n'
	printf 'nbsp=\377start\377 \nvertical=one\vsecond=two\nform=one\fsecond=two\n'
	printf 'controls=one\034two\035three\036four\037five\n'
	printf '%s\n' "quote'key=both \" and ' with \\ slash"
	printf 'final=no newline'
} > "$options_tmp/BK5/BKOPTION.DT5"
{
	printf 'same=equal\nz=last\ndup=first\nempty=not empty\nignored line\n'
	printf ' lead key =  old value\nmulti=a=b\n=old empty key\n'
	printf '#comment=old\ncase=lower\nCase=upper\nonlybackup=present\n'
	printf 'nbsp=old\nvertical=one\nform=one\ncontrols=old\n'
	printf '%s\n' "quote'key=old"
	printf 'final=no newline'
} > "$options_tmp/BK5/BKOPTION.BAK"
compare_output 'Line endings, whitespace, duplicate/empty keys, equals and missing values'

# Exercise Unicode key sorting and repr for every possible CP437 byte.
# CR/LF and '=' bytes are intentional: they must keep their delimiter roles.
{
	byte=0
	while [ "$byte" -lt 256 ]; do
		byte_octal=$(printf '%03o' "$byte")
		printf k
		printf '%b' "\\0$byte_octal"
		printf '=x'
		printf '%b' "\\0$byte_octal"
		printf 'y\n'
		byte=$((byte + 1))
	done
} > "$options_tmp/BK5/BKOPTION.DT5"
{
	byte=0
	while [ "$byte" -lt 256 ]; do
		byte_octal=$(printf '%03o' "$byte")
		printf k
		printf '%b' "\\0$byte_octal"
		printf '=old\n'
		byte=$((byte + 1))
	done
} > "$options_tmp/BK5/BKOPTION.BAK"
compare_output 'All 256 CP437 bytes in keys and values, including Unicode sorting'

: > "$options_tmp/BK5/BKOPTION.DT5"
: > "$options_tmp/BK5/BKOPTION.BAK"
compare_output 'Empty option files'

mv "$options_tmp/BK5/BKOPTION.BAK" "$options_tmp/saved-backup"
expect_failure 'Missing backup' 1
mv "$options_tmp/saved-backup" "$options_tmp/BK5/BKOPTION.BAK"
mv "$options_tmp/BK5/BKOPTION.DT5" "$options_tmp/saved-active"
expect_failure 'Missing active options' 1
mv "$options_tmp/BK5/BKOPTION.BAK" "$options_tmp/saved-backup"
expect_failure 'Missing both option files' 1
expect_failure 'Invalid arguments' 2 --unknown
