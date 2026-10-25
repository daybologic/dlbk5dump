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

# Compare native ALTKEYS decoding with the retained Python reference.
set -eu

repo=$(pwd)
altkeys_tmp=$(mktemp -d /tmp/dlbk5dump-check-altkeys.XXXXXX)
cleanup() {
	find "$altkeys_tmp" -type f -delete
	find "$altkeys_tmp" -depth -type d -empty -delete
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$altkeys_tmp/BK5"

compare_output() {
	(cd "$altkeys_tmp" && "$repo/tools/bk5-altkeys.py") > "$altkeys_tmp/python.txt"
	(cd "$altkeys_tmp" && PATH=/nonexistent "$repo/dlbk5dump" --dump-altkeys) > "$altkeys_tmp/cpp.txt"
	cmp "$altkeys_tmp/python.txt" "$altkeys_tmp/cpp.txt"
	printf '%s: byte-for-byte match\n' "$1"
}

cp "$repo/BK5/ALTKEYS.DT5" "$altkeys_tmp/BK5/ALTKEYS.DT5"
compare_output 'Bundled ALTKEYS, with no other database files or executables'

# Produce one complete slot, allowing arbitrary text and padding.
write_slot() {
	printf "$@" > "$altkeys_tmp/slot"
	slot_bytes=$(wc -c < "$altkeys_tmp/slot")
	test "$slot_bytes" -le 162
	cat "$altkeys_tmp/slot"
	slot_padding=$((162 - slot_bytes))
	if [ "$slot_padding" -gt 0 ]; then
		printf '%*s' "$slot_padding" ''
	fi
}

# Each byte is surrounded by printable text so controls and non-breaking
# spaces inside a field are preserved and quoted, rather than trimmed away.
{
	byte=0
	while [ "$byte" -lt 256 ]; do
		byte_octal=$(printf '%03o' "$byte")
		printf x
		printf '%b' "\\0$byte_octal"
		printf 'y<%158s' ''
		byte=$((byte + 1))
	done
	write_slot '%s' ''
	write_slot '%s' '<ignored after delimiter'
	write_slot '%s' "O'Brien"
	write_slot '%s' 'double "quotes"'
	write_slot '%s' "both ' and \" quotes with \\ backslash"
	write_slot '\t\377boundary\377\r\n<'
	write_slot '%162s' X
} > "$altkeys_tmp/BK5/ALTKEYS.DT5"
compare_output 'All 256 CP437 bytes, quotes, controls, padding and slot boundaries'

# A partial final slot must fail before any records are written.
printf x >> "$altkeys_tmp/BK5/ALTKEYS.DT5"
if (cd "$altkeys_tmp" && "$repo/dlbk5dump" --dump-altkeys) > "$altkeys_tmp/output" 2> "$altkeys_tmp/error"; then
	printf 'Malformed ALTKEYS unexpectedly succeeded\n' >&2
	exit 1
else
	test "$?" -eq 1
fi
test ! -s "$altkeys_tmp/output"
test -s "$altkeys_tmp/error"

: > "$altkeys_tmp/BK5/ALTKEYS.DT5"
compare_output 'Empty ALTKEYS file'
mv "$altkeys_tmp/BK5/ALTKEYS.DT5" "$altkeys_tmp/saved"
if (cd "$altkeys_tmp" && "$repo/dlbk5dump" --dump-altkeys) > "$altkeys_tmp/output" 2> "$altkeys_tmp/error"; then
	printf 'Missing ALTKEYS unexpectedly succeeded\n' >&2
	exit 1
else
	test "$?" -eq 1
fi
test ! -s "$altkeys_tmp/output"
test -s "$altkeys_tmp/error"
if (cd "$altkeys_tmp" && "$repo/dlbk5dump" --dump-altkeys --unknown) > "$altkeys_tmp/output" 2> "$altkeys_tmp/error"; then
	printf 'Unknown ALTKEYS option unexpectedly succeeded\n' >&2
	exit 1
else
	test "$?" -eq 2
fi
test ! -s "$altkeys_tmp/output"
printf 'Malformed/missing input and invalid arguments: rejected without slot output\n'
