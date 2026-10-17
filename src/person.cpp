#include "person.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
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
	/* Data records often carry a five-character surname/index prefix
	 * immediately before the displayed name, so a genuine name can contain
	 * only one literal space (for example WALLI + ELIZABETH WALLIS). */
	return spaces >= 1U;
}

unsigned int PrintFamilyMembers(const std::vector<unsigned char>& data,
	std::size_t nameOffset, std::size_t nameLength, std::size_t recordEnd,
	std::uint32_t family)
{
	unsigned int matches = 0U;
	for ( std::size_t offset = nameOffset + nameLength;
		offset + 3U <= recordEnd; ++offset ) {
		if ( data[offset] != 0x20U ) continue;
		const std::size_t length = static_cast<std::size_t>(data[offset + 1U])
			+ (static_cast<std::size_t>(data[offset + 2U]) << 8U);
		const std::size_t value = offset + 3U;
		if ( length == 0U || length > 32U || value + length > recordEnd
			|| (data[value] != 'M' && data[value] != 'F') ) continue;
		const bool compactFamily = length == 1U && value + 7U <= recordEnd
			&& data[value + 1U] == 0x07U
			&& data[value + 2U] == 0x00U
			&& data[value + 3U] == 0x00U
			&& data[value + 4U] == 0x01U
			&& data[value + 5U] == 0x00U;
		for ( std::size_t familyOffset = 1U;
			familyOffset + 4U <= length; familyOffset += 4U ) {
			if ( BigEndian32(data, value + familyOffset) != family ) continue;
			std::cout << "FAMILY_MEMBER family=0x" << std::hex << family
				<< std::dec << " relation=" << static_cast<char>(data[value])
				<< " record_offset=" << nameOffset << " name=\"";
			for ( std::size_t i = 0U; i < nameLength; ++i ) {
				const unsigned char ch = data[nameOffset + i];
				std::cout << (Printable(ch) ? static_cast<char>(ch) : '.');
			}
			std::cout << "\" field_offset=" << offset << '\n';
			matches++;
		}
		/* Some older records store a one-byte sex field followed by a
		 * compact family reference rather than a length-5 field. */
		if ( compactFamily
			&& family <= 0xffU
			&& data[value + 6U] == family ) {
			std::cout << "FAMILY_MEMBER family=0x" << std::hex << family
				<< std::dec << " relation=" << static_cast<char>(data[value])
				<< " record_offset=" << nameOffset << " name=\"";
			for ( std::size_t i = 0U; i < nameLength; ++i ) {
				const unsigned char ch = data[nameOffset + i];
				std::cout << (Printable(ch) ? static_cast<char>(ch) : '.');
			}
			std::cout << "\" field_offset=" << offset << '\n';
			matches++;
		}
	}
	return matches;
}

std::uint32_t FindLeadingNumeric(const std::vector<unsigned char>& data,
	std::size_t nameEnd, std::size_t recordEnd)
{
	/* The first numeric field is repeated once or twice in some records
	 * (length 8 or 12).  Do not scan farther into the record: a four-byte
	 * year field would otherwise look like an identifier. */
	for ( std::size_t offset = nameEnd; offset + 3U <= recordEnd
		&& offset < nameEnd + 24U; ++offset ) {
		if ( data[offset] != 0x20U ) continue;
		const std::size_t length = static_cast<std::size_t>(data[offset + 1U])
			+ (static_cast<std::size_t>(data[offset + 2U]) << 8U);
		if ( (length != 4U && length != 8U && length != 12U)
			|| offset + 3U + length > recordEnd ) return 0U;
		const std::uint32_t value = BigEndian32(data, offset + 3U);
		/* A note's update date (for example ASCII "19930716") can have
		 * the same marker shape, but is not a plausible BK numeric id. */
		return value <= 0x00ffffffU ? value : 0U;
	}
	return 0U;
}

void PrintRecordFamilies(const std::vector<unsigned char>& data,
	std::size_t nameEnd, std::size_t recordEnd)
{
	std::vector<std::uint32_t> families;
	char sex = '?';
	for ( std::size_t offset = nameEnd; offset + 3U <= recordEnd; ++offset ) {
		if ( data[offset] != 0x20U ) continue;
		const std::size_t length = static_cast<std::size_t>(data[offset + 1U])
			+ (static_cast<std::size_t>(data[offset + 2U]) << 8U);
		const std::size_t value = offset + 3U;
		if ( length == 0U || value + length > recordEnd ) continue;
		if ( length == 1U && (data[value] == 'M' || data[value] == 'F') ) {
			sex = static_cast<char>(data[value]);
			if ( value + 7U <= recordEnd && data[value + 1U] == 0x07U
				&& data[value + 2U] == 0x00U && data[value + 3U] == 0x00U
				&& data[value + 4U] == 0x01U && data[value + 5U] == 0x00U )
				families.push_back(data[value + 6U]);
		}
		if ( (length == 5U || length == 9U)
			&& (data[value] == 'M' || data[value] == 'F') ) {
			sex = static_cast<char>(data[value]);
			for ( std::size_t familyOffset = 1U;
				familyOffset + 4U <= length; familyOffset += 4U )
				families.push_back(BigEndian32(data, value + familyOffset));
		}
	}
	std::cout << " sex=" << sex << " families=";
	for ( std::size_t i = 0U; i < families.size(); ++i ) {
		if ( i != 0U ) std::cout << ',';
		std::cout << "0x" << std::hex << families[i] << std::dec;
	}
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
				if ( i + 2U > valueLength ) return false;
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

bool FirstDecodedName(const std::vector<unsigned char>& record,
	std::string& name, std::size_t& nameEnd)
{
	/* In the expanded fixed record, bytes 4..53 contain a ten-byte sort
	 * key followed by the displayed name and space padding. */
	if ( record.size() >= 54U ) {
		std::size_t length = 50U;
		while ( length > 10U && record[4U + length - 1U] == ' ' ) length--;
		bool printable = true;
		for ( std::size_t i = 10U; i < length; ++i )
			if ( !Printable(record[4U + i]) ) printable = false;
		if ( length > 10U && printable
			&& MostlyUpperText(record, 14U, length - 10U) ) {
			name.clear();
			for ( std::size_t i = 10U; i < length; ++i )
				name += static_cast<char>(record[4U + i]);
			nameEnd = 54U;
			return true;
		}
	}
	const std::size_t limit = std::min(record.size(), static_cast<std::size_t>(512U));
	for ( std::size_t offset = 4U; offset < limit; ) {
		while ( offset < limit && !Printable(record[offset]) ) offset++;
		const std::size_t begin = offset;
		while ( offset < limit && Printable(record[offset]) ) offset++;
		std::size_t length = offset - begin;
		while ( length > 0U && record[begin + length - 1U] == ' ' ) length--;
		/* The name field is followed by padding, so it is one long printable
		 * run rather than the short run found by the raw-page scanner. */
		if ( length >= 12U && MostlyUpperText(record, begin, length) ) {
			name.clear();
			for ( std::size_t i = 0U; i < length; ++i )
				name += static_cast<char>(record[begin + i]);
			nameEnd = begin + length;
			return true;
		}
	}
	return false;
}

void PrintFixedRecordFamilies(const std::vector<unsigned char>& record)
{
	char sex = '?';
	std::vector<std::uint32_t> families;
	for ( std::size_t offset = 54U; offset + 9U <= record.size(); ++offset ) {
		if ( record[offset] != 'M' && record[offset] != 'F' ) continue;
		sex = static_cast<char>(record[offset]);
		const std::uint32_t first = BigEndian32(record, offset + 1U);
		const std::uint32_t second = BigEndian32(record, offset + 5U);
		if ( first != 0U && first <= 0x1ffU ) families.push_back(first);
		else if ( second != 0U && second <= 0x1ffU ) families.push_back(0U);
		if ( second != 0U && second <= 0x1ffU && second != first )
			families.push_back(second);
		if ( !families.empty() ) break;
	}
	std::cout << " sex=" << sex << " families=";
	for ( std::size_t i = 0U; i < families.size(); ++i ) {
		if ( i != 0U ) std::cout << ',';
		std::cout << "0x" << std::hex << families[i] << std::dec;
	}
}

bool FixedDateAt(const std::vector<unsigned char>& record, std::size_t offset)
{
	if ( offset + 11U > record.size() ) return false;
	const std::size_t monthOffset = std::isdigit(record[offset])
		? offset + 3U : offset + 2U;
	if ( std::isdigit(record[offset]) ) {
		if ( !std::isdigit(record[offset + 1U])
			|| record[offset + 2U] != ' ' ) return false;
	} else if ( record[offset] == ' ' && std::isdigit(record[offset + 1U])
		&& record[offset + 2U] == ' ' ) {
		/* BK5 accepts one-digit days and left-pads them in the 15-byte field. */
	} else return false;
	static const char *const months =
		"JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	std::string month;
	month += static_cast<char>(record[monthOffset]);
	month += static_cast<char>(record[monthOffset + 1U]);
	month += static_cast<char>(record[monthOffset + 2U]);
	if ( std::string(months).find(month) == std::string::npos ) return false;
	return record[monthOffset + 3U] == ' '
		&& std::isdigit(record[monthOffset + 4U])
		&& std::isdigit(record[monthOffset + 5U])
		&& std::isdigit(record[monthOffset + 6U])
		&& std::isdigit(record[monthOffset + 7U]);
}

void PrintFixedRecordDates(const std::vector<unsigned char>& record)
{
	std::cout << " dates=";
	bool first = true;
	for ( std::size_t offset = 0x100U; offset < 0x180U; ++offset ) {
		if ( !FixedDateAt(record, offset) ) continue;
		if ( !first ) std::cout << ',';
		first = false;
		std::cout << "0x" << std::hex << offset << std::dec << ':';
		for ( std::size_t j = 0U; j < 11U; ++j )
			std::cout << static_cast<char>(record[offset + j]);
		offset += 10U;
	}
}

void PrintFixedRecordOccupation(const std::vector<unsigned char>& record)
{
	const std::size_t begin = 0x15cU;
	const std::size_t end = std::min(record.size(), static_cast<std::size_t>(0x175U));
	std::size_t first = begin;
	while ( first < end && record[first] == ' ' ) first++;
	std::size_t last = end;
	while ( last > first && record[last - 1U] == ' ' ) last--;
	std::cout << " occupation=\"";
	for ( std::size_t offset = first; offset < last; ++offset )
		std::cout << (Printable(record[offset])
			? static_cast<char>(record[offset]) : '.');
	std::cout << "\"";
}

void PrintFixedRecordSourceRefs(const std::vector<unsigned char>& record)
{
	/* BK5 stores five source numbers after the five location numbers.  Keep
	 * the references separate from the dates until the per-field ordering has
	 * been completely established. */
	std::cout << " source_refs=";
	for ( std::size_t offset = 0x84U; offset < 0x98U; offset += 4U ) {
		const std::uint32_t source = LittleEndian32(record, offset);
		/* Values outside the source table's observed number range belong to
		 * adjacent legacy metadata in some converted records, not source IDs. */
		if ( source > 1000U )
			std::cout << 0U;
		else
			std::cout << source;
		if ( offset + 4U < 0x98U ) std::cout << ',';
	}
}

void PrintFixedRecordLocationRefs(const std::vector<unsigned char>& record)
{
	std::cout << " location_refs=";
	for ( std::size_t offset = 0x70U; offset < 0x84U; offset += 4U ) {
		const std::uint32_t location = LittleEndian32(record, offset);
		std::cout << (location <= 1000U ? location : 0U);
		if ( offset + 4U < 0x84U ) std::cout << ',';
	}
}

void PrintFixedRecordNotes(const std::vector<unsigned char>& record)
{
	std::cout << " notes=\"";
	bool firstNote = true;
	for ( std::size_t offset = 0x190U; offset + 8U < record.size(); ) {
		while ( offset < record.size() && !Printable(record[offset]) ) offset++;
		const std::size_t begin = offset;
		while ( offset < record.size() && Printable(record[offset]) ) offset++;
		std::size_t first = begin;
		while ( first < offset && record[first] == ' ' ) first++;
		std::size_t length = offset - first;
		while ( length > 0U && record[first + length - 1U] == ' ' ) length--;
		if ( length < 8U ) continue;
		std::size_t letters = 0U;
		for ( std::size_t i = 0U; i < length; ++i )
			if ( std::isalpha(record[first + i]) ) letters++;
		if ( letters < 4U ) continue;
		if ( !firstNote ) std::cout << '|';
		firstNote = false;
		while ( length > 0U && record[first + length - 1U] == '*' ) length--;
		while ( length > 0U && record[first + length - 1U] == ' ' ) length--;
		for ( std::size_t i = 0U; i < length; ++i )
			std::cout << (Printable(record[first + i])
				? static_cast<char>(record[first + i]) : '.');
	}
	std::cout << "\"";
}

void PrintRawRecordDates(const std::vector<unsigned char>& data,
	std::size_t begin, std::size_t end)
{
	std::cout << " dates=";
	bool first = true;
	for ( std::size_t offset = begin; offset + 11U <= end; ++offset ) {
		if ( !FixedDateAt(data, offset) ) continue;
		if ( !first ) std::cout << ',';
		first = false;
		std::cout << "0x" << std::hex << offset << std::dec << ':';
		for ( std::size_t i = 0U; i < 11U; ++i )
			std::cout << static_cast<char>(data[offset + i]);
		offset += 10U;
	}
}

std::uint32_t RawPersonId(const std::vector<unsigned char>& data,
	std::size_t nameOffset)
{
	if ( nameOffset < 4U || data[nameOffset - 4U] != 0x00U
		|| data[nameOffset - 3U] != 0x00U ) return 0U;
	const std::uint32_t id = BigEndian32(data, nameOffset - 4U);
	return id > 0U && id <= 10000U ? id : 0U;
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

int TReadPersons::DumpRecords() const
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
	std::cout << "RECORD_SCAN file_size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount << '\n';
	unsigned int records = 0U;
	/* Pages 131 onward contain the variable person records in this file.
	 * Keep the lower bound explicit: index pages also contain name keys. */
	for ( std::size_t page = 131U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		std::vector<std::pair<std::size_t, std::size_t> > candidates;
		for ( std::size_t offset = pageBegin; offset < pageEnd; ) {
			while ( offset < pageEnd && !Printable(data[offset]) ) offset++;
			const std::size_t begin = offset;
			while ( offset < pageEnd && Printable(data[offset]) ) offset++;
			const std::size_t length = offset - begin;
			if ( NameCandidate(data, begin, length) )
				candidates.push_back(std::make_pair(begin, length));
		}
		for ( std::size_t i = 0U; i < candidates.size(); ++i ) {
			const std::size_t nameBegin = candidates[i].first;
			const std::size_t nameEnd = nameBegin + candidates[i].second;
			const std::size_t recordEnd = i + 1U < candidates.size()
				? candidates[i + 1U].first : pageEnd;
			const std::uint32_t leadingNumeric = FindLeadingNumeric(data,
				nameEnd, recordEnd);
			if ( leadingNumeric == 0U ) continue;
			const std::uint32_t rawPersonId = RawPersonId(data, nameBegin);
			std::cout << "PERSON_RECORD offset=" << nameBegin;
			if ( rawPersonId != 0U )
				std::cout << " person_id=" << rawPersonId;
			std::cout
				<< " leading_numeric=0x" << std::hex << leadingNumeric
				<< std::dec << " raw_name=\"";
			PrintText(data, nameBegin, candidates[i].second);
			std::cout << "\"";
			PrintRecordFamilies(data, nameEnd, recordEnd);
			PrintRawRecordDates(data, nameEnd, recordEnd);
			std::cout << '\n';
			records++;
		}
	}
	std::cout << "PERSON_RECORDS total=" << records << '\n';
	return 0;
}

int TReadPersons::DumpIdIndex() const
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
	unsigned int entries = 0U;
	std::cout << "ID_INDEX_SCAN file_size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount << '\n';
	for ( std::size_t page = 1U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		/* Key 0 is the four-byte person-number key. Its leaf pages have
		 * the 00 00 2a 00 marker at page offset 4. */
		if ( data[pageBegin + 4U] != 0x00U
			|| data[pageBegin + 5U] != 0x00U
			|| data[pageBegin + 6U] != 0x2aU
			|| data[pageBegin + 7U] != 0x00U ) continue;
		for ( std::size_t offset = pageBegin + 16U;
			offset + 12U <= pageEnd; offset += 12U ) {
			if ( BigEndian32(data, offset) == 0U ) continue;
			const std::uint32_t duplicate = BigEndian32(data, offset + 8U);
			if ( duplicate != 0U && duplicate != 0xffffffffU ) continue;
			const std::uint32_t id = BigEndian32(data, offset);
			const std::uint32_t address = BtrieveAddress(data, offset + 4U);
			if ( address >= data.size() ) continue;
			std::cout << "PERSON_ID_ENTRY page=" << page
				<< " offset=" << offset << " id=" << id
				<< " address=0x" << std::hex << address << std::dec
				<< " address_page=" << address / PAGE_SIZE
				<< " address_offset=" << address % PAGE_SIZE << '\n';
			entries++;
		}
	}
	std::cout << "PERSON_ID_ENTRIES total=" << entries << '\n';
	return 0;
}

int TReadPersons::DumpIdRecords() const
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
	unsigned int indexed = 0U;
	unsigned int decoded = 0U;
	unsigned int named = 0U;
	unsigned int unindexed = 0U;
	unsigned int fixedSlots = 0U;
	unsigned int fixedDecodeFailures = 0U;
	unsigned int fixedUnnamed = 0U;
	std::set<std::uint32_t> indexedIds;
	std::cout << "ID_RECORD_SCAN file_size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount << '\n';
	for ( std::size_t page = 1U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		if ( data[pageBegin + 4U] != 0x00U
			|| data[pageBegin + 5U] != 0x00U
			|| data[pageBegin + 6U] != 0x2aU
			|| data[pageBegin + 7U] != 0x00U ) continue;
		for ( std::size_t offset = pageBegin + 16U;
			offset + 12U <= pageEnd; offset += 12U ) {
			const std::uint32_t id = BigEndian32(data, offset);
			if ( id == 0U ) continue;
			const std::uint32_t duplicate = BigEndian32(data, offset + 8U);
			if ( duplicate != 0U && duplicate != 0xffffffffU ) continue;
			const std::uint32_t address = BtrieveAddress(data, offset + 4U);
			indexedIds.insert(id);
			indexed++;
			std::cout << "PERSON_ID_RECORD id=" << id << " address=0x"
				<< std::hex << address << std::dec;
			if ( address + 29U > data.size() ) {
				std::cout << " status=address-out-of-range\n";
				continue;
			}
			if ( data[address + 4U] == 0U ) {
				std::cout << " status=deleted-or-uncompressed\n";
				continue;
			}
			std::vector<unsigned char> record;
			if ( !DecodeCompressedRecord(data, address, record) ) {
				std::cout << " status=compressed-decode-failed\n";
				continue;
			}
			decoded++;
			std::string name;
			std::size_t nameEnd = 0U;
			if ( FirstDecodedName(record, name, nameEnd) ) {
				named++;
				std::cout << " status=decoded decoded_length=" << record.size()
					<< " name=\"" << name << "\"";
				PrintFixedRecordFamilies(record);
				PrintFixedRecordOccupation(record);
				PrintFixedRecordNotes(record);
				PrintFixedRecordLocationRefs(record);
				PrintFixedRecordSourceRefs(record);
				PrintFixedRecordDates(record);
				std::cout << '\n';
			} else {
				std::cout << " status=decoded decoded_length=" << record.size()
					<< " name=\"\"\n";
			}
		}
	}
	/* Some fixed records are still addressable from the name and marriage
	 * indexes even when their key-0 person-number entry is absent. Scan the
	 * physical fixed-record slots to recover those records without assigning
	 * a number from position or name. The expanded record's first four bytes
	 * provide the person number. */
	std::cout << "UNINDEXED_FIXED_SCAN\n";
	for ( std::size_t page = 6U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		for ( std::size_t address = pageBegin + 6U;
			address + 29U <= pageEnd; address += 29U ) {
			if ( data[address + 2U] != 0U || data[address + 4U] == 0U ) continue;
			fixedSlots++;
			std::vector<unsigned char> record;
			if ( !DecodeCompressedRecord(data, address, record)
				|| record.size() < 54U ) {
				fixedDecodeFailures++;
				continue;
			}
			const std::uint32_t id = BigEndian32(record, 0U);
			/* The file contains variable pages interspersed with fixed pages;
			 * reject plausible-looking padding before it becomes a person id. */
			if ( id == 0U || id > 10000U || indexedIds.count(id) != 0U ) continue;
			std::string name;
			std::size_t nameEnd = 0U;
			if ( !FirstDecodedName(record, name, nameEnd) ) {
				fixedUnnamed++;
				continue;
			}
			std::cout << "PERSON_ID_RECORD id=" << id << " address=0x"
				<< std::hex << address << std::dec
				<< " status=decoded decoded_length=" << record.size()
				<< " name=\"" << name << "\"";
			PrintFixedRecordFamilies(record);
			PrintFixedRecordOccupation(record);
			PrintFixedRecordNotes(record);
			PrintFixedRecordLocationRefs(record);
			PrintFixedRecordSourceRefs(record);
			PrintFixedRecordDates(record);
			std::cout << " source=unindexed-fixed-scan\n";
			unindexed++;
		}
	}
	std::cout << "ID_RECORDS indexed=" << indexed << " decoded=" << decoded
		<< " named=" << named << " unindexed=" << unindexed
		<< " fixed_slots=" << fixedSlots
		<< " fixed_decode_failures=" << fixedDecodeFailures
		<< " fixed_unnamed=" << fixedUnnamed << '\n';
	return 0;
}

int TReadPersons::TracePersonId(std::uint32_t id) const
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
	std::size_t fixedAddress = data.size();
	for ( std::size_t page = 1U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		if ( data[pageBegin + 4U] != 0x00U
			|| data[pageBegin + 5U] != 0x00U
			|| data[pageBegin + 6U] != 0x2aU
			|| data[pageBegin + 7U] != 0x00U ) continue;
		for ( std::size_t offset = pageBegin + 16U;
			offset + 12U <= pageEnd; offset += 12U ) {
			if ( BigEndian32(data, offset) != id ) continue;
			const std::uint32_t duplicate = BigEndian32(data, offset + 8U);
			if ( duplicate != 0U && duplicate != 0xffffffffU ) continue;
			fixedAddress = BtrieveAddress(data, offset + 4U);
			break;
		}
		if ( fixedAddress < data.size() ) break;
	}
	if ( fixedAddress >= data.size() ) {
		std::cerr << "No key-0 index entry for person id " << id << '\n';
		return 1;
	}
	if ( fixedAddress + 29U > data.size() ) return 2;
	const std::uint32_t variablePage =
		(static_cast<std::uint32_t>(data[fixedAddress]) << 8U)
		| static_cast<std::uint32_t>(data[fixedAddress + 1U]);
	const unsigned int fragment = data[fixedAddress + 3U];
	const unsigned int compression = data[fixedAddress + 4U];
	std::cout << "PERSON_ID_TRACE id=" << id << " fixed_address=0x"
		<< std::hex << fixedAddress << std::dec
		<< " variable_page=" << variablePage
		<< " fragment=" << fragment
		<< " compression=0x" << std::hex << compression << std::dec << '\n';
	std::cout << "  fixed=";
	PrintBinary(data, fixedAddress, 29U);
	std::cout << '\n';
	if ( variablePage >= pageCount || fragment > 254U ) return 0;
	const std::size_t variableBegin = variablePage * PAGE_SIZE;
	const std::size_t tableIndex = ((PAGE_SIZE - 1U) / 2U) - fragment;
	const std::size_t tableOffset = variableBegin + tableIndex * 2U;
	if ( tableOffset + 2U > variableBegin + PAGE_SIZE ) return 2;
	const std::uint16_t start = LittleEndian16(data, tableOffset) & 0x7fffU;
	std::size_t previousIndex = tableIndex;
	while ( previousIndex > 0U
		&& LittleEndian16(data, variableBegin + (previousIndex - 1U) * 2U) == 0xffffU )
		previousIndex--;
	if ( previousIndex == 0U ) return 0;
	const std::uint16_t end = LittleEndian16(data,
		variableBegin + (previousIndex - 1U) * 2U) & 0x7fffU;
	if ( end < start || variableBegin + end > data.size() ) return 2;
	std::cout << "  variable_fragment file_offset=" << variableBegin + start
		<< " length=" << end - start << " table_offset=" << tableOffset << '\n';
	PrintTraceBytes(data, variableBegin + start,
		std::min(variableBegin + static_cast<std::size_t>(end),
			variableBegin + start + 128U));
	std::vector<unsigned char> expanded;
	if ( compression != 0U && DecodeCompressedRecord(data, fixedAddress,
		expanded) ) {
		std::cout << "  decompressed_length=" << expanded.size() << '\n';
		PrintTraceBytes(expanded, 0U, expanded.size());
	}
	return 0;
}

int TReadPersons::TraceFixedAddress(std::uint32_t address) const
{
	std::ifstream personFile(PERSON_FILE, std::ios::in | std::ios::binary);
	if ( !personFile.is_open() ) {
		std::cerr << "Error: cannot open " << PERSON_FILE << '\n';
		return 1;
	}
	const std::vector<unsigned char> data(
		(std::istreambuf_iterator<char>(personFile)),
		std::istreambuf_iterator<char>());
	if ( static_cast<std::size_t>(address) + 29U > data.size() ) {
		std::cerr << "Error: fixed address 0x" << std::hex << address
			<< std::dec << " is outside the file\n";
		return 2;
	}
	std::cout << "FIXED_ADDRESS_TRACE address=0x" << std::hex << address
		<< std::dec << " page=" << address / PAGE_SIZE
		<< " page_offset=" << address % PAGE_SIZE << '\n';
	std::cout << "  fixed=";
	PrintBinary(data, address, 29U);
	std::cout << '\n';
	std::vector<unsigned char> expanded;
	if ( data[address + 4U] == 0U
		|| !DecodeCompressedRecord(data, address, expanded) ) {
		std::cout << "  status=not-decodable-compressed-record\n";
		return 0;
	}
	std::string name;
	std::size_t nameEnd = 0U;
	std::cout << "  status=decoded expanded_length=" << expanded.size();
	if ( FirstDecodedName(expanded, name, nameEnd) )
		std::cout << " name=\"" << name << "\"";
	PrintFixedRecordFamilies(expanded);
	PrintFixedRecordOccupation(expanded);
	PrintFixedRecordNotes(expanded);
	PrintFixedRecordLocationRefs(expanded);
	PrintFixedRecordSourceRefs(expanded);
	PrintFixedRecordDates(expanded);
	std::cout << '\n';
	PrintTraceBytes(expanded, 0U,
		std::min(expanded.size(), static_cast<std::size_t>(512U)));
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

int TReadPersons::DumpRelations() const
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
	std::cout << "RELATION_SCAN file_size=" << data.size()
		<< " page_size=" << PAGE_SIZE << " pages=" << pageCount
		<< " interpretation=unvalidated-index-pointer-candidates\n";
	for ( std::size_t page = 0U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		for ( std::size_t offset = pageBegin + 5U;
			offset + 17U <= pageEnd; ++offset ) {
			/* Relationship-table entries observed in the data begin with a
			 * two-word header ending in 01. Do not scan arbitrary four-byte
			 * sequences as pointers. */
			if ( data[offset] != 0x00U || data[offset + 2U] != 0x00U
				|| data[offset + 4U] != 0x01U || data[offset + 3U] == 0x20U ) continue;
			bool found = false;
			std::vector<std::uint32_t> targets;
			for ( const std::size_t relative : { 5U, 9U, 13U } ) {
				if ( data[offset + relative + 1U] != 0x00U
					|| data[offset + relative] > 0x02U ) continue;
				const std::uint32_t target = BtrieveAddress(data,
					offset + relative);
				if ( target == 0U || target == 0xffffffffU
					|| target >= data.size() ) continue;
				targets.push_back(target);
				found = true;
			}
			if ( !found ) continue;
			std::cout << "RELATION_CANDIDATE page=" << page << " offset=" << offset
				<< " header=";
			PrintBinary(data, offset, 5U);
			std::cout << " targets=";
			for ( std::size_t i = 0U; i < targets.size(); ++i ) {
				if ( i != 0U ) std::cout << ',';
				std::cout << "0x" << std::hex << targets[i] << std::dec;
			}
			std::cout << '\n';
			/* Candidate entries can contain padding, but a second candidate
			 * inside this one is not a separate record. */
			offset += 16U;
		}
	}
	return 0;
}

int TReadPersons::DumpFamily(std::uint32_t family) const
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
	std::cout << "FAMILY_SCAN family=0x" << std::hex << family << std::dec
		<< " file_size=" << data.size() << " pages=" << pageCount << '\n';
	unsigned int matches = 0U;
	for ( std::size_t page = 0U; page < pageCount; ++page ) {
		const std::size_t pageBegin = page * PAGE_SIZE;
		const std::size_t pageEnd = pageBegin + PAGE_SIZE;
		std::vector<std::pair<std::size_t, std::size_t> > candidates;
		for ( std::size_t offset = pageBegin; offset < pageEnd; ) {
			while ( offset < pageEnd && !Printable(data[offset]) ) offset++;
			const std::size_t begin = offset;
			while ( offset < pageEnd && Printable(data[offset]) ) offset++;
			const std::size_t length = offset - begin;
			if ( NameCandidate(data, begin, length) )
				candidates.push_back(std::make_pair(begin, length));
		}
		for ( std::size_t i = 0U; i < candidates.size(); ++i ) {
			const std::size_t recordEnd = i + 1U < candidates.size()
				? candidates[i + 1U].first : pageEnd;
			matches += PrintFamilyMembers(data, candidates[i].first,
				candidates[i].second, recordEnd, family);
		}
	}
	std::cout << "FAMILY_MEMBERS total=" << matches << '\n';
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
