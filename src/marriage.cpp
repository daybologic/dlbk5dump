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

#include "marriage.hpp"

#include "btrieve.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
const char *const MARRIAGE_FILE = "BK5/BKMARR.DT5";
const std::size_t PAGE_SIZE = 1024U;
const std::size_t DATA_START = 0xc00U;

/* BKMARR.DT5 is a compressed Btrieve file with one key: the four-byte
 * family number.  Its file control record gives a five-byte physical
 * record (the compressed-record pointer) and 167 records.  Key-0 leaf
 * pages (bytes 4..7 = 00 00 2a 00) hold twelve-byte entries: the family
 * number, a Btrieve address of the five-byte pointer, and a duplicate
 * link.  Each pointer expands to a 242-byte family record whose first
 * four bytes repeat the key.  Validated on all 167 records: every indexed
 * key matches the expanded record, and 148 families agree exactly with
 * the spouse and child links decoded independently from BKPERSON.DT5.
 *
 * Expanded record layout (numbers are big-endian, text space-padded):
 *     0   family number
 *     4   husband person number (0 = none)
 *     8   wife person number (0 = none)
 *    12   marriage date, 15 characters
 *    27   second date, 15 characters (blank in every observed record)
 *    42   marriage location number (BKLOCATE.DT5)
 *    46   second location number (zero in every observed record)
 *    50   three 20-character source-citation fields, one per source
 *         slot: 18 characters of citation detail then a 2-character
 *         quality code (0-3).  A code only appears with its source.
 *   110   three source numbers (BKSOURCE.DT5)
 *   122   status, NUL or space padded: BK5's CHANGES.DOC lists the fields
 *         "Marr. ceremony Y/N", "Divorced/Annulled/Separated" and "End
 *         year", stored as 1, 1 and 4 characters ("YS1991", " D")
 *   133   25 child slots of four bytes; slot 9 (offset 165) is zero in
 *         every record, and children continue after it in the five
 *         families with more than eight children
 *   233   eight-digit YYYYMMDD stamp
 *   241   one trailing flag byte ('*' in every observed record) */
const std::size_t FAMILY_RECORD_LENGTH = 242U;
const std::size_t FAMILY_POINTER_LENGTH = 5U;
const std::size_t FAMILY_HUSBAND = 4U;
const std::size_t FAMILY_WIFE = 8U;
const std::size_t FAMILY_DATES = 12U;
const std::size_t FAMILY_DATE_LENGTH = 15U;
const std::size_t FAMILY_LOCATIONS = 42U;
const std::size_t FAMILY_CITATIONS = 50U;
const std::size_t FAMILY_CITATION_LENGTH = 20U;
const std::size_t FAMILY_CITATION_DETAIL_LENGTH = 18U;
const std::size_t FAMILY_SOURCES = 110U;
const std::size_t FAMILY_SLOTS = 3U;
const std::size_t FAMILY_STATUS = 122U;
const std::size_t FAMILY_STATUS_LENGTH = 11U;
const std::size_t FAMILY_CHILDREN = 133U;
const std::size_t FAMILY_MAX_CHILDREN = 25U;
const std::size_t FAMILY_STAMP = 233U;
const std::size_t FAMILY_STAMP_LENGTH = 8U;
const std::size_t FAMILY_FLAG = 241U;

bool Printable(unsigned char ch)
{
	return ch == ' ' || (ch >= 0x20U && ch <= 0x7eU);
}

std::uint32_t LittleEndian32(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	return static_cast<std::uint32_t>(data[offset])
		| (static_cast<std::uint32_t>(data[offset + 1U]) << 8U)
		| (static_cast<std::uint32_t>(data[offset + 2U]) << 16U)
		| (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

std::uint32_t BigEndian32(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	return (static_cast<std::uint32_t>(data[offset]) << 24U)
		| (static_cast<std::uint32_t>(data[offset + 1U]) << 16U)
		| (static_cast<std::uint32_t>(data[offset + 2U]) << 8U)
		| static_cast<std::uint32_t>(data[offset + 3U]);
}

std::size_t DateLengthAt(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	if ( offset + 11U > data.size() ) return 0U;
	if ( !std::isdigit(data[offset]) || !std::isdigit(data[offset + 1U])
		|| data[offset + 2U] != ' ' ) return 0U;
	const bool fourLetterSeptember = data[offset + 3U] == 'S'
		&& data[offset + 4U] == 'E' && data[offset + 5U] == 'P'
		&& data[offset + 6U] == 'T';
	const std::size_t monthLength = fourLetterSeptember ? 4U : 3U;
	const std::size_t yearOffset = offset + 3U + monthLength + 1U;
	if ( yearOffset + 4U > data.size() ) return 0U;
	static const char *const MONTHS = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	std::string month;
	month += static_cast<char>(data[offset + 3U]);
	month += static_cast<char>(data[offset + 4U]);
	month += static_cast<char>(data[offset + 5U]);
	if ( std::string(MONTHS).find(month) == std::string::npos ) return 0U;
	if ( !fourLetterSeptember && data[offset + 6U] != ' ' ) return 0U;
	for ( std::size_t i = 0U; i < 4U; ++i ) {
		if ( !std::isdigit(data[yearOffset + i])
			&& data[yearOffset + i] != '?' ) return 0U;
	}
	return monthLength + 8U;
}

bool PlausibleSpouseReference(std::uint32_t value)
{
	return value <= 10000U;
}

bool StandardMarriageHeader(const std::vector<unsigned char>& data,
	std::size_t recordStart, std::size_t dateOffset)
{
	if ( dateOffset < 16U || recordStart + 16U > data.size() ) return false;
	const std::uint32_t spouse1 = BigEndian32(data, recordStart + 8U);
	const std::uint32_t spouse2 = BigEndian32(data, recordStart + 12U);
	return spouse1 != 0U && PlausibleSpouseReference(spouse1)
		&& PlausibleSpouseReference(spouse2);
}

bool AlternateFamilyHint(const std::vector<unsigned char>& data,
	std::size_t recordStart, std::size_t dateOffset)
{
	if ( dateOffset < 16U || recordStart + 8U > data.size() ) return false;
	/* Several older BKMARR headers retain a family-like byte in the same
	 * position, but their layout is not sufficiently stable to promote it to
	 * a family ID. Report it as a candidate for manual correlation only. */
	return data[recordStart + 2U] == 0x17U
		&& data[recordStart + 3U] == 0x00U
		&& data[recordStart + 4U] == 0x00U
		&& data[recordStart + 5U] == 0x00U
		&& data[recordStart + 6U] == 0x00U
		&& data[recordStart + 7U] != 0U;
}

void PrintBytes(const std::vector<unsigned char>& data,
	std::size_t begin, std::size_t end)
{
	for ( std::size_t offset = begin; offset < end; offset += 16U ) {
		const std::size_t lineEnd = std::min(offset + 16U, end);
		std::cout << "    " << std::hex << std::setw(8)
			<< std::setfill('0') << offset << "  ";
		for ( std::size_t i = offset; i < lineEnd; ++i )
			std::cout << std::setw(2) << static_cast<unsigned int>(data[i]) << ' ';
		std::cout << " |";
		for ( std::size_t i = offset; i < lineEnd; ++i )
			std::cout << (Printable(data[i]) ? static_cast<char>(data[i]) : '.');
		std::cout << "|\n";
	}
	std::cout << std::setfill(' ') << std::dec;
}

std::uint32_t BtrieveAddress(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	/* Old Btrieve stores a file address as two little-endian words, with
	 * the high word first. */
	const std::uint32_t high = static_cast<std::uint32_t>(data[offset])
		| (static_cast<std::uint32_t>(data[offset + 1U]) << 8U);
	const std::uint32_t low = static_cast<std::uint32_t>(data[offset + 2U])
		| (static_cast<std::uint32_t>(data[offset + 3U]) << 8U);
	return (high << 16U) | low;
}

bool ReadMarriageFile(std::vector<unsigned char>& data)
{
	std::ifstream marriageFile(MARRIAGE_FILE, std::ios::in | std::ios::binary);
	if ( !marriageFile.is_open() ) {
		std::cerr << "Error: cannot open " << MARRIAGE_FILE << '\n';
		return false;
	}
	data.assign((std::istreambuf_iterator<char>(marriageFile)),
		std::istreambuf_iterator<char>());
	return true;
}

struct TFamilyRecord {
	std::size_t address;
	bool indexed;
	std::vector<unsigned char> record;
};

struct TFamilyScan {
	std::map<std::uint32_t, TFamilyRecord> families;
	unsigned int indexEntries;
	unsigned int indexMismatches;
	unsigned int slots;
	unsigned int failures;
	unsigned int duplicates;
};

/* Find the family records through the key-0 index, then scan every
 * five-byte slot on the data pages that index points into.  The scan
 * recovers records whose index entries sit on non-leaf pages; no record is
 * accepted unless it expands to the full family length, and its family
 * number is taken from the expanded record, never from slot position. */
void ScanFamilies(const std::vector<unsigned char>& data, TFamilyScan& scan)
{
	scan.indexEntries = scan.indexMismatches = 0U;
	scan.slots = scan.failures = scan.duplicates = 0U;
	std::map<std::size_t, std::uint32_t> indexed;
	std::set<std::size_t> dataPages;
	const std::size_t pageCount = data.size() / PAGE_SIZE;
	for ( std::size_t page = 1U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		if ( data[pageBegin + 4U] != 0x00U || data[pageBegin + 5U] != 0x00U
			|| data[pageBegin + 6U] != 0x2aU
			|| data[pageBegin + 7U] != 0x00U ) continue;
		for ( std::size_t offset = pageBegin + 16U;
			offset + 12U <= pageBegin + PAGE_SIZE; offset += 12U ) {
			const std::uint32_t key = BigEndian32(data, offset);
			if ( key == 0U ) continue;
			const std::uint32_t duplicate = BigEndian32(data, offset + 8U);
			if ( duplicate != 0U && duplicate != 0xffffffffU ) continue;
			const std::size_t address = BtrieveAddress(data, offset + 4U);
			if ( address + FAMILY_POINTER_LENGTH > data.size() ) continue;
			indexed[address] = key;
			dataPages.insert(address / PAGE_SIZE);
			scan.indexEntries++;
		}
	}
	for ( std::set<std::size_t>::const_iterator page = dataPages.begin();
		page != dataPages.end(); ++page ) {
		const std::size_t pageBegin = *page * PAGE_SIZE;
		for ( std::size_t address = pageBegin + 6U;
			address + FAMILY_POINTER_LENGTH <= pageBegin + PAGE_SIZE;
			address += FAMILY_POINTER_LENGTH ) {
			if ( data[address + 2U] != 0U || data[address + 4U] == 0U ) continue;
			scan.slots++;
			std::vector<unsigned char> record;
			if ( !DecodeCompressedRecord(data, address, record)
				|| record.size() != FAMILY_RECORD_LENGTH ) {
				scan.failures++;
				std::cout << "FAMILY_SLOT_FAILURE address=0x" << std::hex
					<< address << std::dec << '\n';
				continue;
			}
			const std::uint32_t family = BigEndian32(record, 0U);
			const std::map<std::size_t, std::uint32_t>::const_iterator key =
				indexed.find(address);
			if ( key != indexed.end() && key->second != family ) {
				scan.indexMismatches++;
				std::cout << "FAMILY_INDEX_MISMATCH address=0x" << std::hex
					<< address << std::dec << " index_key=" << key->second
					<< " record_key=" << family << '\n';
			}
			if ( scan.families.count(family) != 0U ) {
				scan.duplicates++;
				std::cout << "FAMILY_DUPLICATE id=" << family << " address=0x"
					<< std::hex << address << std::dec << '\n';
				continue;
			}
			TFamilyRecord entry;
			entry.address = address;
			entry.indexed = key != indexed.end();
			entry.record = record;
			scan.families[family] = entry;
		}
	}
}

std::string FamilyText(const std::vector<unsigned char>& record,
	std::size_t offset, std::size_t length)
{
	std::string text;
	for ( std::size_t i = 0U; i < length && record[offset + i] != 0U; ++i )
		text += Printable(record[offset + i])
			? static_cast<char>(record[offset + i]) : '?';
	const std::size_t end = text.find_last_not_of(' ');
	return end == std::string::npos ? std::string() : text.substr(0U, end + 1U);
}

void PrintFamilyFields(const std::vector<unsigned char>& record)
{
	std::cout << " husband=" << BigEndian32(record, FAMILY_HUSBAND)
		<< " wife=" << BigEndian32(record, FAMILY_WIFE);
	for ( std::size_t slot = 0U; slot < 2U; ++slot )
		std::cout << " date_" << slot + 1U << "=\"" << FamilyText(record,
			FAMILY_DATES + slot * FAMILY_DATE_LENGTH, FAMILY_DATE_LENGTH)
			<< '"';
	std::cout << " location_refs=" << BigEndian32(record, FAMILY_LOCATIONS)
		<< ',' << BigEndian32(record, FAMILY_LOCATIONS + 4U);
	std::cout << " source_refs=";
	for ( std::size_t slot = 0U; slot < FAMILY_SLOTS; ++slot )
		std::cout << (slot ? "," : "")
			<< BigEndian32(record, FAMILY_SOURCES + slot * 4U);
	std::cout << " source_details=";
	for ( std::size_t slot = 0U; slot < FAMILY_SLOTS; ++slot )
		std::cout << (slot ? "," : "") << '"' << FamilyText(record,
			FAMILY_CITATIONS + slot * FAMILY_CITATION_LENGTH,
			FAMILY_CITATION_DETAIL_LENGTH) << '"';
	std::cout << " source_quality=";
	for ( std::size_t slot = 0U; slot < FAMILY_SLOTS; ++slot )
		std::cout << (slot ? "," : "") << '"' << FamilyText(record,
			FAMILY_CITATIONS + slot * FAMILY_CITATION_LENGTH
				+ FAMILY_CITATION_DETAIL_LENGTH,
			FAMILY_CITATION_LENGTH - FAMILY_CITATION_DETAIL_LENGTH) << '"';
	std::cout << " status=\"" << FamilyText(record, FAMILY_STATUS,
		FAMILY_STATUS_LENGTH) << "\" children=";
	bool first = true;
	for ( std::size_t child = 0U; child < FAMILY_MAX_CHILDREN; ++child ) {
		const std::uint32_t person = BigEndian32(record,
			FAMILY_CHILDREN + child * 4U);
		/* Slot 9 is empty in every record, including the five families
		 * with more than eight children, so skip empty slots rather than
		 * stopping at the first one. */
		if ( person == 0U ) continue;
		std::cout << (first ? "" : ",") << person;
		first = false;
	}
	std::cout << " stamp=" << FamilyText(record, FAMILY_STAMP,
		FAMILY_STAMP_LENGTH) << " flag=\""
		<< FamilyText(record, FAMILY_FLAG, 1U) << '"';
}
}

using namespace std;
//---------------------------------------------------------------------------
TReadMarriages *TReadMarriages::Load()
{
	std::ifstream marriageFile(MARRIAGE_FILE, std::ios::in | std::ios::binary);
	return marriageFile.is_open() ? this : NULL;
}

int TReadMarriages::Dump() const
{
	std::ifstream marriageFile(MARRIAGE_FILE, std::ios::in | std::ios::binary);
	if ( !marriageFile.is_open() ) {
		std::cerr << "Error: cannot open " << MARRIAGE_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(marriageFile)),
		std::istreambuf_iterator<char>());
	std::cout << "BKMARR.DT5 size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " data_start=0x"
		<< std::hex << DATA_START << std::dec << '\n';
	unsigned int count = 0U;
	for ( std::size_t offset = DATA_START; offset + 11U <= data.size(); ++offset ) {
		const std::size_t dateLength = DateLengthAt(data, offset);
		if ( dateLength == 0U ) continue;
		const bool hasPrefix = offset >= 16U;
		const std::size_t recordStart = hasPrefix ? offset - 16U : offset;
		const std::size_t begin = offset >= 16U ? offset - 16U : 0U;
		const std::size_t end = std::min(data.size(), offset + dateLength + 37U);
		std::cout << "MARRIAGE_RECORD number=" << (++count)
			<< " record_offset=" << recordStart
			<< " date_offset=" << offset << " page=" << offset / PAGE_SIZE
			<< " page_offset=" << offset % PAGE_SIZE << " date=\"";
		for ( std::size_t i = 0U; i < dateLength; ++i )
			std::cout << static_cast<char>(data[offset + i]);
		std::cout << "\"\n";
		if ( hasPrefix && StandardMarriageHeader(data, recordStart, offset) ) {
			std::cout << "    header=";
			for ( std::size_t i = 0U; i < 8U; ++i )
				std::cout << std::hex << std::setw(2) << std::setfill('0')
					<< static_cast<unsigned int>(data[recordStart + i]);
			std::cout << std::dec << std::setfill(' ')
				<< " spouse_ref_1=0x" << std::hex
				<< BigEndian32(data, recordStart + 8U)
				<< " spouse_ref_2=0x" << BigEndian32(data, recordStart + 12U)
				<< std::dec;
			if ( data[recordStart + 1U] == 0x2aU
				&& data[recordStart + 2U] == 0x17U )
				std::cout << " family_id=0x" << std::hex
					<< static_cast<unsigned int>(data[recordStart + 7U])
					<< std::dec;
			else if ( AlternateFamilyHint(data, recordStart, offset) )
				std::cout << " family_hint=0x" << std::hex
					<< static_cast<unsigned int>(data[recordStart + 7U])
					<< std::dec;
			std::cout << '\n';
		} else if ( offset >= 3U && data[offset - 3U] == 0x2aU
			&& data[offset - 2U] == 0x00U
			&& data[offset - 1U] != 0U ) {
			/* Some old records use a compact key immediately before the
			 * date.  The final byte is a person/index candidate, not a
			 * proven spouse pair; keep it explicitly labelled. */
			std::cout << "    layout=compact-date-key compact_ref=0x"
				<< std::hex << static_cast<unsigned int>(data[offset - 1U])
				<< std::dec << '\n';
		} else if ( hasPrefix ) {
			std::cout << "    layout=unclassified-date-context\n";
		}
		PrintBytes(data, begin, end);
		if ( offset >= 4U ) {
			std::cout << "    preceding_le32=0x" << std::hex
				<< LittleEndian32(data, offset - 4U) << std::dec << '\n';
		}
	}
	std::cout << "MARRIAGE_RECORDS total=" << count << '\n';
	return 0;
}

int TReadMarriages::DumpFamilies() const
{
	std::vector<unsigned char> data;
	if ( !ReadMarriageFile(data) ) return 1;
	TFamilyScan scan;
	ScanFamilies(data, scan);
	for ( std::map<std::uint32_t, TFamilyRecord>::const_iterator family =
		scan.families.begin(); family != scan.families.end(); ++family ) {
		std::cout << "FAMILY_RECORD id=" << family->first << " address=0x"
			<< std::hex << family->second.address << std::dec
			<< " indexed=" << (family->second.indexed ? "yes" : "no");
		PrintFamilyFields(family->second.record);
		std::cout << '\n';
	}
	std::cout << "FAMILY_RECORDS decoded=" << scan.families.size()
		<< " index_entries=" << scan.indexEntries
		<< " index_mismatches=" << scan.indexMismatches
		<< " slots=" << scan.slots << " failures=" << scan.failures
		<< " duplicates=" << scan.duplicates << '\n';
	return 0;
}

int TReadMarriages::TraceFamily(std::uint32_t family) const
{
	std::vector<unsigned char> data;
	if ( !ReadMarriageFile(data) ) return 1;
	TFamilyScan scan;
	ScanFamilies(data, scan);
	const std::map<std::uint32_t, TFamilyRecord>::const_iterator entry =
		scan.families.find(family);
	if ( entry == scan.families.end() ) {
		std::cout << "FAMILY_TRACE id=" << family << " status=not-found\n";
		return 1;
	}
	std::cout << "FAMILY_TRACE id=" << family << " address=0x" << std::hex
		<< entry->second.address << std::dec << " pointer=";
	for ( std::size_t i = 0U; i < FAMILY_POINTER_LENGTH; ++i )
		std::cout << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned int>(data[entry->second.address + i]);
	std::cout << std::dec << std::setfill(' ') << " expanded_length="
		<< entry->second.record.size();
	PrintFamilyFields(entry->second.record);
	std::cout << '\n';
	/* Offsets in this dump are relative to the expanded record. */
	PrintBytes(entry->second.record, 0U, entry->second.record.size());
	return 0;
}
//---------------------------------------------------------------------------
