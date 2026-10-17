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

bool DateAt(const std::vector<unsigned char>& data, std::size_t offset)
{
	if ( offset + 11U > data.size() ) return false;
	if ( !std::isdigit(data[offset]) || !std::isdigit(data[offset + 1U])
		|| data[offset + 2U] != ' ' ) return false;
	static const char *const MONTHS = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	std::string month;
	month += static_cast<char>(data[offset + 3U]);
	month += static_cast<char>(data[offset + 4U]);
	month += static_cast<char>(data[offset + 5U]);
	if ( std::string(MONTHS).find(month) == std::string::npos ) return false;
	return data[offset + 6U] == ' '
		&& (std::isdigit(data[offset + 7U]) || data[offset + 7U] == '?')
		&& (std::isdigit(data[offset + 8U]) || data[offset + 8U] == '?')
		&& (std::isdigit(data[offset + 9U]) || data[offset + 9U] == '?')
		&& (std::isdigit(data[offset + 10U]) || data[offset + 10U] == '?');
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
		if ( !DateAt(data, offset) ) continue;
		const bool hasPrefix = offset >= 16U;
		const std::size_t recordStart = hasPrefix ? offset - 16U : offset;
		const std::size_t begin = offset >= 16U ? offset - 16U : 0U;
		const std::size_t end = std::min(data.size(), offset + 48U);
		std::cout << "MARRIAGE_RECORD number=" << (++count)
			<< " record_offset=" << recordStart
			<< " date_offset=" << offset << " page=" << offset / PAGE_SIZE
			<< " page_offset=" << offset % PAGE_SIZE << " date=\"";
		for ( std::size_t i = 0U; i < 11U; ++i )
			std::cout << static_cast<char>(data[offset + i]);
		std::cout << "\"\n";
		if ( hasPrefix ) {
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
			std::cout << '\n';
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
