#include "lookup.hpp"

#include "btrieve.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {
const char *const SOURCE_FILE = "BK5/BKSOURCE.DT5";
const char *const LOCATION_FILE = "BK5/BKLOCATE.DT5";

/* BKSOURCE.DT5 and BKLOCATE.DT5 are compressed Btrieve files of numbered
 * descriptions, referenced by number from person and family records.
 * Validated on every record: all 294 sources and all 90 locations expand
 * to the lengths below, numbered without gaps or duplicates.
 *
 * Source record (241 bytes): 0 big-endian source number; 4 three 78-column
 * text lines (BK5 documents "3 full lines" of source text); 238 "  *".
 * Location record (47 bytes): 0 big-endian location number; 4 a
 * 42-character name; 46 '*'.  Text is space padded and contains no
 * control bytes. */
const std::size_t SOURCE_RECORD_LENGTH = 241U;
const std::size_t SOURCE_TEXT = 4U;
const std::size_t SOURCE_LINE_LENGTH = 78U;
const std::size_t SOURCE_LINES = 3U;
const std::size_t LOCATION_RECORD_LENGTH = 47U;
const std::size_t LOCATION_TEXT = 4U;
const std::size_t LOCATION_TEXT_LENGTH = 42U;

bool ReadLookupFile(const char *path, std::vector<unsigned char>& data)
{
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if ( !file.is_open() ) {
		std::cerr << "Error: cannot open " << path << '\n';
		return false;
	}
	data.assign((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	return true;
}

/* Quote a space-padded field, escaping backslashes and double quotes so the
 * dump can be parsed unambiguously; trailing padding is dropped. */
std::string QuotedText(const std::vector<unsigned char>& record,
	std::size_t offset, std::size_t length)
{
	std::string text(record.begin() + offset, record.begin() + offset + length);
	const std::size_t end = text.find_last_not_of(' ');
	text = end == std::string::npos ? std::string() : text.substr(0U, end + 1U);
	std::string quoted("\"");
	for ( std::string::const_iterator ch = text.begin(); ch != text.end(); ++ch ) {
		if ( *ch == '\\' || *ch == '"' ) quoted += '\\';
		quoted += *ch;
	}
	return quoted + '"';
}

void PrintSummary(const char *label, std::size_t decoded,
	const TBtrieveScan& scan)
{
	std::cout << label << " decoded=" << decoded
		<< " header_records=" << scan.headerRecords
		<< " slots=" << scan.slots << " failures=" << scan.failures
		<< " duplicates=" << scan.duplicates << '\n';
}
}

using namespace std;
//---------------------------------------------------------------------------
int TReadLookups::DumpSources() const
{
	std::vector<unsigned char> data;
	if ( !ReadLookupFile(SOURCE_FILE, data) ) return 1;
	TBtrieveScan scan;
	ScanCompressedRecords(data, SOURCE_RECORD_LENGTH, scan);
	for ( std::map<std::uint32_t, TBtrieveRecord>::const_iterator source =
		scan.records.begin(); source != scan.records.end(); ++source ) {
		std::cout << "SOURCE_RECORD id=" << source->first << " address=0x"
			<< std::hex << source->second.address << std::dec;
		for ( std::size_t line = 0U; line < SOURCE_LINES; ++line )
			std::cout << " line_" << line + 1U << '=' << QuotedText(
				source->second.record,
				SOURCE_TEXT + line * SOURCE_LINE_LENGTH, SOURCE_LINE_LENGTH);
		std::cout << '\n';
	}
	PrintSummary("SOURCE_RECORDS", scan.records.size(), scan);
	return 0;
}

int TReadLookups::DumpLocations() const
{
	std::vector<unsigned char> data;
	if ( !ReadLookupFile(LOCATION_FILE, data) ) return 1;
	TBtrieveScan scan;
	ScanCompressedRecords(data, LOCATION_RECORD_LENGTH, scan);
	for ( std::map<std::uint32_t, TBtrieveRecord>::const_iterator location =
		scan.records.begin(); location != scan.records.end(); ++location ) {
		std::cout << "LOCATION_RECORD id=" << location->first << " address=0x"
			<< std::hex << location->second.address << std::dec
			<< " name=" << QuotedText(location->second.record, LOCATION_TEXT,
				LOCATION_TEXT_LENGTH) << '\n';
	}
	PrintSummary("LOCATION_RECORDS", scan.records.size(), scan);
	return 0;
}
//---------------------------------------------------------------------------
