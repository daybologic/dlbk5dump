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

void PrintCandidateFields(const std::vector<unsigned char>& data,
	std::size_t begin, std::size_t end)
{
	for ( std::size_t offset = begin; offset + 3U <= end; ++offset ) {
		/* Text fields observed in BKPERSON use 0x20 followed by a
		 * little-endian length. This remains a candidate decoder until
		 * the complete Btrieve record layout is known. */
		if ( data[offset] != 0x20U ) continue;
		const std::size_t length = static_cast<std::size_t>(data[offset + 1U])
			+ (static_cast<std::size_t>(data[offset + 2U]) << 8U);
		const std::size_t value = offset + 3U;
		if ( length == 0U || length > 128U || value + length > end ) continue;
		std::cout << "    field+0x" << std::hex << offset - begin
			<< std::dec << " length " << length << ": ";
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
