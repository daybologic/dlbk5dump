#include "marriage.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
const char *const MARRIAGE_FILE = "BK5/BKMARR.DT5";
const std::size_t PAGE_SIZE = 1024U;
const std::size_t DATA_START = 0xc00U;

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
//---------------------------------------------------------------------------
