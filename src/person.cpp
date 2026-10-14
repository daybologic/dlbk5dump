#include "person.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
const char *const PERSON_FILE = "BK5/BKPERSON.DT5";
const std::size_t PAGE_SIZE = 1024U;
const std::size_t CONTEXT_AFTER = 128U;

std::string Upper(const std::string& value)
{
	std::string result(value);
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
	return result;
}

bool Printable(unsigned char ch)
{
	return ch == ' ' || (ch >= 0x20U && ch <= 0x7eU);
}

bool MostlyPrintable(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length)
{
	if ( length == 0U ) return false;
	std::size_t printable = 0U;
	for ( std::size_t i = 0U; i < length; ++i ) {
		if ( Printable(data[offset + i]) ) printable++;
	}
	return printable * 4U >= length * 3U;
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

void PrintBinary(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length);

void PrintRelatedBlock(const std::vector<unsigned char>& data,
	std::size_t value, std::size_t fieldOffset)
{
	std::cout << "    RELATED_BLOCK field+0x" << std::hex << fieldOffset
		<< std::dec << " ";
	PrintBinary(data, value, 16U);
	const std::uint32_t header = BigEndian32(data, value);
	const std::uint32_t descriptor = LittleEndian32(data, value + 4U);
	const unsigned int tag = data[value + 8U];
	const std::uint32_t ordinal = LittleEndian32(data, value + 9U);
	const std::uint32_t pointer = (static_cast<std::uint32_t>(data[value + 13U]) << 16U)
		| (static_cast<std::uint32_t>(data[value + 14U]) << 8U)
		| static_cast<std::uint32_t>(data[value + 15U]);
	std::cout << " header=0x" << std::hex << header
		<< " descriptor=0x" << descriptor
		<< " tag=0x" << tag << " ordinal=0x" << ordinal
		<< " pointer24=0x" << pointer << std::dec;
	if ( pointer < data.size() ) {
		std::cout << " file_offset=" << pointer
			<< " page=" << pointer / PAGE_SIZE
			<< " page_offset=" << pointer % PAGE_SIZE;
	} else {
		std::cout << " file_offset=outside-BKPERSON.DT5";
	}
	std::cout << '\n';
}

void PrintCandidateFields(const std::vector<unsigned char>& data,
	std::size_t begin, std::size_t end)
{
	bool related = false;
	for ( std::size_t offset = begin; offset + 3U <= end; ++offset ) {
		/* Text fields observed in BKPERSON use 0x20 followed by a
		 * little-endian length. This remains a candidate decoder until
		 * the complete Btrieve record layout is known. */
		if ( data[offset] != 0x20U ) continue;
		const std::size_t length = static_cast<std::size_t>(data[offset + 1U])
			+ (static_cast<std::size_t>(data[offset + 2U]) << 8U);
		const std::size_t value = offset + 3U;
		if ( length == 0U || length > 128U || value + length > end ) continue;
		const bool relatedMarker = length == 16U
			&& data[value] == 0x00U && data[value + 1U] == 0x00U
			&& data[value + 2U] == 0x00U && data[value + 3U] == 0x07U
			&& data[value + 4U] == 0x00U && data[value + 5U] == 0x20U
			&& data[value + 6U] == 0x01U && data[value + 7U] == 0x00U
			&& data[value + 8U] == 0x2aU;
		if ( relatedMarker ) {
			related = true;
			PrintRelatedBlock(data, value, offset - begin);
			continue;
		}
		std::cout << "    field+0x" << std::hex << offset - begin
			<< std::dec << " length " << length << ": ";
		if ( related ) std::cout << "related ";
		if ( MostlyPrintable(data, value, length) ) {
			std::string text;
			for ( std::size_t i = 0U; i < length; ++i ) {
				const unsigned char ch = data[value + i];
				text += Printable(ch) ? static_cast<char>(ch) : '.';
			}
			std::cout << text << '\n';
			continue;
		}
		std::cout << "binary";
		if ( length == 8U ) {
			std::cout << " LE32=[0x" << std::hex
				<< LittleEndian32(data, value) << ", 0x"
				<< LittleEndian32(data, value + 4U)
				<< "] BE32=[0x" << BigEndian32(data, value)
				<< ", 0x" << BigEndian32(data, value + 4U)
				<< std::dec << "]";
		}
		std::cout << " bytes=";
		for ( std::size_t i = 0U; i < length; ++i ) {
			std::cout << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(data[value + i]);
		}
		std::cout << std::setfill(' ') << std::dec << '\n';
	}
}

void PrintBinary(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length)
{
	std::cout << "bytes=";
	for ( std::size_t i = 0U; i < length; ++i ) {
		std::cout << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned int>(data[offset + i]);
	}
	std::cout << std::setfill(' ') << std::dec;
}

void PrintField(const std::vector<unsigned char>& data,
	std::size_t marker, std::size_t page, std::size_t fileOffset)
{
	const std::size_t length = static_cast<std::size_t>(data[marker + 1U])
		+ (static_cast<std::size_t>(data[marker + 2U]) << 8U);
	const std::size_t value = marker + 3U;
	if ( length == 0U || length > 256U || value + length > data.size() ) return;
	std::cout << "FIELD page=" << page << " offset=" << fileOffset
		<< " length=" << length << " ";
	if ( MostlyPrintable(data, value, length) ) {
		std::cout << "text=\"";
		for ( std::size_t i = 0U; i < length; ++i ) {
			const unsigned char ch = data[value + i];
			std::cout << (Printable(ch) ? static_cast<char>(ch) : '.');
		}
		std::cout << "\"\n";
		return;
	}
	PrintBinary(data, value, length);
	if ( length == 8U ) {
		std::cout << " le32=[0x" << std::hex << LittleEndian32(data, value)
			<< ",0x" << LittleEndian32(data, value + 4U)
			<< "] be32=[0x" << BigEndian32(data, value)
			<< ",0x" << BigEndian32(data, value + 4U) << std::dec << "]";
	}
	std::cout << '\n';
}

bool MostlyUpperText(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length)
{
	std::size_t letters = 0U;
	std::size_t upper = 0U;
	for ( std::size_t i = 0U; i < length; ++i ) {
		const unsigned char ch = data[offset + i];
		if ( std::isalpha(ch) ) {
			letters++;
			if ( std::isupper(ch) ) upper++;
		}
	}
	return letters >= 4U && upper * 2U >= letters;
}

bool NameCandidate(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length)
{
	if ( length < 12U || length > 60U || !MostlyUpperText(data, offset, length) )
		return false;
	/* Date, occupation, and note values are normally introduced by a
	 * 0x20 field marker with a length greater than one. A name may use
	 * the one-byte marker, so reject the former without rejecting names. */
	if ( offset >= 3U && data[offset - 3U] == 0x20U
		&& data[offset - 2U] != 0x01U ) return false;
	std::size_t spaces = 0U;
	for ( std::size_t i = 0U; i < length; ++i )
		if ( data[offset + i] == ' ' ) spaces++;
	return spaces >= 2U;
}

void PrintText(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length)
{
	for ( std::size_t i = 0U; i < length; ++i )
		std::cout << static_cast<char>(data[offset + i]);
}

bool FixedIndexKey(const std::vector<unsigned char>& data,
	std::size_t offset, std::size_t length, std::size_t pageEnd)
{
	if ( offset + length + 12U > pageEnd ) return false;
	for ( std::size_t i = 0U; i < length; ++i ) {
		if ( !Printable(data[offset + i]) ) return false;
	}
	return true;
}

void PrintIndexReference(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	std::cout << " ref1=";
	PrintBinary(data, offset, 4U);
	std::cout << "(address=0x" << std::hex << BtrieveAddress(data, offset)
		<< std::dec << ")";
	std::cout << " ref2=";
	PrintBinary(data, offset + 4U, 4U);
	std::cout << "(address=0x" << std::hex
		<< BtrieveAddress(data, offset + 4U) << std::dec << ")";
	std::cout << " link=";
	PrintBinary(data, offset + 8U, 4U);
	std::cout << '\n';
}

void PrintTraceBytes(const std::vector<unsigned char>& data,
	std::size_t begin, std::size_t end)
{
	for ( std::size_t offset = begin; offset < end; offset += 16U ) {
		std::cout << "  " << std::hex << std::setw(8) << std::setfill('0')
			<< offset << "  ";
		const std::size_t lineEnd = std::min(offset + 16U, end);
		for ( std::size_t i = offset; i < lineEnd; ++i ) {
			std::cout << std::setw(2) << static_cast<unsigned int>(data[i])
				<< ' ';
		}
		for ( std::size_t i = lineEnd; i < offset + 16U; ++i )
			std::cout << "   ";
		std::cout << " |";
		for ( std::size_t i = offset; i < lineEnd; ++i )
			std::cout << (Printable(data[i]) ? static_cast<char>(data[i]) : '.');
		std::cout << "|\n";
	}
	std::cout << std::setfill(' ') << std::dec;
}
}

int TReadPersons::Dump() const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	const std::size_t pageCount = data.size() / PAGE_SIZE;
	std::cout << "BKPERSON.DT5 size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount << '\n';
	for ( std::size_t page = 0U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		for ( std::size_t offset = pageBegin; offset + 3U <= pageEnd; ++offset ) {
			if ( data[offset] == 0x20U )
				PrintField(data, offset, page, offset);
		}
		for ( std::size_t offset = pageBegin; offset < pageEnd; ) {
			while ( offset < pageEnd && !Printable(data[offset]) ) offset++;
			const std::size_t begin = offset;
			while ( offset < pageEnd && Printable(data[offset]) ) offset++;
			const std::size_t length = offset - begin;
			if ( length >= 8U && MostlyUpperText(data, begin, length) ) {
				std::cout << "TEXT page=" << page << " offset=" << begin
					<< " length=" << length << " text=\"";
				for ( std::size_t i = 0U; i < length; ++i )
					std::cout << static_cast<char>(data[begin + i]);
				std::cout << "\"\n";
			}
		}
	}
	return 0;
}

int TReadPersons::DumpPage(unsigned int page) const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	const std::size_t pageCount = data.size() / PAGE_SIZE;
	if ( page >= pageCount ) {
		std::cerr << "Error: page " << page << " is outside the file ("
			<< pageCount << " pages)\n";
		return 2;
	}
	const std::size_t pageBegin = static_cast<std::size_t>(page) * PAGE_SIZE;
	const std::size_t pageEnd = pageBegin + PAGE_SIZE;
	std::cout << "PAGE " << page << " file_offset=" << pageBegin
		<< " size=" << PAGE_SIZE << "\n";
	std::cout << "  header=";
	PrintBinary(data, pageBegin, 16U);
	std::cout << '\n';

	std::vector<std::pair<std::size_t, std::size_t> > candidates;
	for ( std::size_t offset = pageBegin; offset < pageEnd; ) {
		while ( offset < pageEnd && !Printable(data[offset]) ) offset++;
		const std::size_t begin = offset;
		while ( offset < pageEnd && Printable(data[offset]) ) offset++;
		const std::size_t length = offset - begin;
		if ( NameCandidate(data, begin, length) )
			candidates.push_back(std::make_pair(begin, length));
	}
	if ( candidates.empty() ) {
		std::cout << "  no candidate person records\n";
		return 0;
	}
	for ( std::size_t i = 0U; i < candidates.size(); ++i ) {
		const std::size_t nameBegin = candidates[i].first;
		const std::size_t nameEnd = nameBegin + candidates[i].second;
		const std::size_t recordEnd = i + 1U < candidates.size()
			? candidates[i + 1U].first : pageEnd;
		std::cout << "  NAME_ANCHOR name_offset=" << nameBegin
			<< " span=" << nameBegin << ".." << recordEnd << '\n';
		std::cout << "    name_text=\"";
		PrintText(data, nameBegin, candidates[i].second);
		std::cout << "\"\n";
		PrintCandidateFields(data, nameEnd, recordEnd);
	}
	return 0;
}

int TReadPersons::DumpIndexPage(unsigned int page) const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	const std::size_t pageCount = data.size() / PAGE_SIZE;
	if ( page >= pageCount ) {
		std::cerr << "Error: page " << page << " is outside the file ("
			<< pageCount << " pages)\n";
		return 2;
	}
	const std::size_t pageBegin = static_cast<std::size_t>(page) * PAGE_SIZE;
	const std::size_t pageEnd = pageBegin + PAGE_SIZE;
	std::cout << "INDEX_PAGE " << page << " file_offset=" << pageBegin
		<< " size=" << PAGE_SIZE << "\n";
	std::cout << "  header=";
	PrintBinary(data, pageBegin, 16U);
	std::cout << '\n';
	bool found = false;
	for ( std::size_t offset = pageBegin + 16U;
		offset + 52U <= pageEnd; ++offset ) {
		std::size_t keyLength = 0U;
		if ( FixedIndexKey(data, offset, 40U, pageEnd) ) keyLength = 40U;
		else if ( FixedIndexKey(data, offset, 10U, pageEnd) ) keyLength = 10U;
		if ( keyLength == 0U ) continue;
		/* Index entries are fixed-width keys followed by two references
		 * and a four-byte child/link value. Avoid reporting the middle of
		 * a key as another candidate. */
		if ( offset > pageBegin + 16U &&
			(data[offset - 1U] == ' ' || Printable(data[offset - 1U])) )
			continue;
		std::cout << "  ENTRY offset=" << offset << " key=\"";
		PrintText(data, offset, keyLength);
		std::cout << "\"";
		PrintIndexReference(data, offset + keyLength);
		found = true;
		offset += keyLength + 11U;
	}
	if ( !found ) std::cout << "  no fixed-width index entries detected\n";
	return 0;
}

int TReadPersons::DumpIndex() const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	const std::size_t pageCount = data.size() / PAGE_SIZE;
	std::cout << "INDEX_SCAN file_size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount << '\n';
	for ( std::size_t page = 0U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		for ( std::size_t offset = pageBegin + 16U;
			offset + 52U <= pageEnd; ++offset ) {
			std::size_t keyLength = 0U;
			if ( FixedIndexKey(data, offset, 40U, pageEnd) ) keyLength = 40U;
			else if ( FixedIndexKey(data, offset, 10U, pageEnd) ) keyLength = 10U;
			if ( keyLength == 0U ) continue;
			if ( offset > pageBegin + 16U &&
				(data[offset - 1U] == ' ' || Printable(data[offset - 1U])) )
				continue;
			std::cout << "ENTRY page=" << page << " offset=" << offset
				<< " key=\"";
			PrintText(data, offset, keyLength);
			std::cout << "\"";
			PrintIndexReference(data, offset + keyLength);
			offset += keyLength + 11U;
		}
	}
	return 0;
}

int TReadPersons::TraceAddress(std::uint32_t address) const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	if ( address >= data.size() ) {
		std::cerr << "Error: address 0x" << std::hex << address << std::dec
			<< " is outside the file (" << data.size() << " bytes)\n";
		return 2;
	}
	const std::size_t page = address / PAGE_SIZE;
	const std::size_t pageOffset = address % PAGE_SIZE;
	const std::size_t begin = address >= 16U ? address - 16U : 0U;
	const std::size_t end = std::min(data.size(),
		static_cast<std::size_t>(address) + 64U);
	std::cout << "ADDRESS address=0x" << std::hex << address << std::dec
		<< " file_offset=" << address << " page=" << page
		<< " page_offset=" << pageOffset << '\n';
	PrintTraceBytes(data, begin, end);
	std::cout << "REFERENCES\n";
	for ( std::size_t offset = begin; offset + 4U <= end; ++offset ) {
		/* In this file the high word is stored first and is little-endian;
		 * reject arbitrary byte sequences before decoding them as pointers. */
		if ( data[offset + 1U] != 0x00U || data[offset] > 0x02U ) continue;
		const std::uint32_t candidate = BtrieveAddress(data, offset);
		if ( candidate == 0U || candidate == 0xffffffffU || candidate >= data.size() )
			continue;
		std::cout << "  offset=" << offset << " address=0x" << std::hex
			<< candidate << std::dec << " page=" << candidate / PAGE_SIZE
			<< " page_offset=" << candidate % PAGE_SIZE << '\n';
	}
	return 0;
}

int TReadPersons::Find(const std::string& query) const
{
	if ( query.empty() ) {
		std::cerr << "Error: person search text is empty\n";
		return 2;
	}

	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	std::string upperData;
	upperData.reserve(data.size());
	for ( unsigned char ch : data )
		upperData += static_cast<char>(std::toupper(ch));
	const std::string upperQuery = Upper(query);
	std::size_t match = upperData.find(upperQuery);
	unsigned int count = 0U;
	while ( match != std::string::npos ) {
		const std::size_t begin = match + upperQuery.size();
		const std::size_t end = std::min(data.size(),
			begin + CONTEXT_AFTER);
		std::cout << "Match " << (++count) << " at file offset " << match
			<< " (0x" << std::hex << match << std::dec << ")\n";
		std::cout << "    name: " << query << '\n';
		PrintCandidateFields(data, begin, end);
		match = upperData.find(upperQuery, match + upperQuery.size());
	}
	if ( count == 0U ) {
		std::cout << "No person match for: " << query << '\n';
		return 1;
	}
	return 0;
}
