/*
Family History Database Recovery Project (dlbk5dump)
Copyright (c) 2012-2026, Rev. Duncan Ross Palmer (M6KVM, 2E0EOL),
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the Daybo Logic nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef INC_BTRIEVE_H
#define INC_BTRIEVE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

/* Expand the compressed Btrieve record whose five-byte fixed pointer starts
 * at fixedAddress.  Returns false if the fragment chain cannot be followed. */
bool DecodeCompressedRecord(const std::vector<unsigned char>& data,
	std::size_t fixedAddress, std::vector<unsigned char>& expanded);

struct TBtrieveRecord {
	std::size_t address;
	std::vector<unsigned char> record;
};

struct TBtrieveScan {
	unsigned int headerRecords;
	unsigned int slots;
	unsigned int failures;
	unsigned int duplicates;
	std::map<std::uint32_t, TBtrieveRecord> records;
};

/* Expand every record in a compressed BK5 Btrieve file whose records start
 * with a big-endian record number.  Data pages are those whose 16-bit word
 * at offset 4 has its top bit set; their fixed slots, of the physical record
 * length in the file control record, start at offset 6.  A record is kept
 * only if it expands to exactly recordLength bytes, and it is keyed by the
 * number inside it, never by slot position. */
void ScanCompressedRecords(const std::vector<unsigned char>& data,
	std::size_t recordLength, TBtrieveScan& scan);

#endif //!INC_BTRIEVE_H
