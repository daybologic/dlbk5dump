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

#include "btrieve.hpp"

#include <cstdint>

namespace {
const std::size_t PAGE_SIZE = 1024U;

std::uint16_t LittleEndian16(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	return static_cast<std::uint16_t>(data[offset])
		| (static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

std::uint32_t BigEndian32(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	return (static_cast<std::uint32_t>(data[offset]) << 24U)
		| (static_cast<std::uint32_t>(data[offset + 1U]) << 16U)
		| (static_cast<std::uint32_t>(data[offset + 2U]) << 8U)
		| static_cast<std::uint32_t>(data[offset + 3U]);
}

/* File control record (page 0) fields used by the scanner. */
const std::size_t FCR_PHYSICAL_LENGTH = 0x18U;
const std::size_t FCR_RECORD_COUNT = 0x1cU;
const std::size_t DATA_PAGE_FLAGS = 4U;
const std::size_t DATA_PAGE_SLOTS = 6U;
const std::uint16_t DATA_PAGE_FLAG = 0x8000U;
const std::size_t POINTER_LENGTH = 5U;
}

void ScanCompressedRecords(const std::vector<unsigned char>& data,
	std::size_t recordLength, TBtrieveScan& scan)
{
	scan.headerRecords = scan.slots = scan.failures = scan.duplicates = 0U;
	scan.records.clear();
	if ( data.size() < PAGE_SIZE ) return;
	const std::size_t physical = LittleEndian16(data, FCR_PHYSICAL_LENGTH);
	scan.headerRecords = static_cast<unsigned int>(
		LittleEndian16(data, FCR_RECORD_COUNT)
		| (static_cast<std::uint32_t>(
			LittleEndian16(data, FCR_RECORD_COUNT + 2U)) << 16U));
	if ( physical < POINTER_LENGTH ) return;
	for ( std::size_t page = 1U; page < data.size() / PAGE_SIZE; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		if ( !(LittleEndian16(data, pageBegin + DATA_PAGE_FLAGS)
			& DATA_PAGE_FLAG) ) continue;
		for ( std::size_t address = pageBegin + DATA_PAGE_SLOTS;
			address + physical <= pageBegin + PAGE_SIZE;
			address += physical ) {
			if ( data[address + 4U] == 0U ) continue;
			scan.slots++;
			std::vector<unsigned char> record;
			if ( !DecodeCompressedRecord(data, address, record)
				|| record.size() != recordLength ) {
				scan.failures++;
				continue;
			}
			const std::uint32_t key = BigEndian32(record, 0U);
			if ( scan.records.count(key) != 0U ) {
				scan.duplicates++;
				continue;
			}
			TBtrieveRecord entry;
			entry.address = address;
			entry.record = record;
			scan.records[key] = entry;
		}
	}
}

bool DecodeCompressedRecord(const std::vector<unsigned char>& data,
	std::size_t fixedAddress, std::vector<unsigned char>& expanded)
{
	/* DUMPDATA temporarily restores a carried byte in the fragment buffer
	 * when a run count straddles fragments.  Keep that mutation local. */
	std::vector<unsigned char> work(data);
	std::uint16_t page = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(work[fixedAddress]) << 8U)
		| work[fixedAddress + 1U]);
	unsigned int fragment = work[fixedAddress + 3U];
	if ( work[fixedAddress + 4U] == 0U ) return false;
	int count = 0;
	bool copyState = true;
	bool countSpans = false;
	unsigned char saved = 0U;
	for ( unsigned int chain = 0U; chain < 256U; ++chain ) {
		if ( page >= work.size() / PAGE_SIZE || fragment > 254U ) return false;
		const std::size_t pageBegin = static_cast<std::size_t>(page) * PAGE_SIZE;
		const std::size_t tableIndex = ((PAGE_SIZE - 1U) / 2U) - fragment;
		const std::size_t tableOffset = pageBegin + tableIndex * 2U;
		const std::uint16_t currentRaw = LittleEndian16(work, tableOffset);
		std::size_t previousIndex = tableIndex;
		while ( previousIndex > 0U
			&& LittleEndian16(work, pageBegin + (previousIndex - 1U) * 2U)
				== 0xffffU ) previousIndex--;
		if ( previousIndex == 0U ) return false;
		const std::uint16_t current = currentRaw & 0x7fffU;
		const std::uint16_t previous = LittleEndian16(work,
			pageBegin + (previousIndex - 1U) * 2U) & 0x7fffU;
		if ( previous < current ) return false;
		std::size_t valueOffset = pageBegin + current;
		std::size_t valueLength = previous - current;
		std::uint16_t nextPage = 0xffffU;
		unsigned int nextFragment = 0xffU;
		if ( currentRaw & 0x8000U ) {
			if ( valueLength < 4U ) return false;
			nextPage = static_cast<std::uint16_t>(
				(static_cast<std::uint16_t>(work[valueOffset]) << 8U)
				| work[valueOffset + 1U]);
			nextFragment = work[valueOffset + 3U];
			valueOffset += 4U;
			valueLength -= 4U;
		}
		if ( countSpans ) {
			if ( valueOffset == pageBegin ) return false;
			valueOffset--;
			valueLength++;
			/* The source algorithm restores the byte preceding the fragment
			 * so a two-byte repeat count can cross a page boundary. */
			work[valueOffset] = saved;
			countSpans = false;
		}
		std::size_t i = 0U;
		while ( i < valueLength ) {
			if ( count < 1 ) {
				/* A fragment holding only a continuation pointer and one
				 * data byte carries just the low byte of the next count;
				 * the high byte starts the following fragment.  BKMARR
				 * family 128 and four BKPERSON records use this layout. */
				if ( i + 1U == valueLength ) {
					saved = work[valueOffset + i++];
					countSpans = true;
					break;
				}
				count = static_cast<int>(LittleEndian16(work, valueOffset + i));
				i += 2U;
			}
			if ( i >= valueLength ) break;
			if ( copyState ) {
				while ( count-- > 0 ) {
					expanded.push_back(work[valueOffset + i++]);
					if ( expanded.size() > 4096U ) return false;
					if ( i == valueLength && count > 0 ) break;
				}
				if ( count < 1 ) copyState = false;
			} else {
				const unsigned char repeated = work[valueOffset + i];
				while ( count-- > 0 ) {
					expanded.push_back(repeated);
					if ( expanded.size() > 4096U ) return false;
				}
				i++;
				copyState = true;
			}
			if ( i == valueLength - 1U ) {
				saved = work[valueOffset + i++];
				countSpans = true;
				break;
			}
		}
		if ( nextPage == 0xffffU ) return true;
		page = nextPage;
		fragment = nextFragment;
	}
	return false;
}
