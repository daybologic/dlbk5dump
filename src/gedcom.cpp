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

#include "gedcom.hpp"

#include "lookup.hpp"
#include "marriage.hpp"
#include "person.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using Strings = std::vector<std::string>;
using Numbers = std::vector<std::uint32_t>;
using Dates = std::vector<std::pair<std::string, std::string>>;
using Membership = std::pair<std::uint32_t, std::string>;
using Links = std::map<std::uint32_t, std::vector<Membership>>;

const std::array<std::string, 5> DATE_SLOTS = {
	"0x111", "0x120", "0x12f", "0x13e", "0x14d"
};
const std::array<std::string, 5> DATE_TAGS = {
	"BIRT", "BAPM", "DEAT", "BURI", ""
};

struct Person {
	std::uint32_t id = 0;
	std::string address, length, name, sex, occupation;
	Strings notes, details, quality;
	Numbers locations, sources, families;
	std::map<std::string, std::string> dates;
};

struct RawPerson {
	std::uint32_t id = 0;
	std::string xref, offset, name, sex;
	Numbers families;
	Dates dates;
};

struct Family {
	std::string address, stamp, status;
	std::uint32_t husband = 0, wife = 0;
	Strings dates, details, quality;
	Numbers locations, sources, children;
};

std::size_t WhitespaceWidth(const std::string& value, std::size_t offset)
{
	if ( offset >= value.size() ) return 0;
	const unsigned char ch = value[offset];
	if ( ch < 128 ) return ch == ' ' || (ch >= 9 && ch <= 13)
		|| (ch >= 28 && ch <= 31) ? 1 : 0;
	// Python str.strip/split use Unicode whitespace, including the CP437
	// non-breaking space. Do not depend on the process's C locale.
	static const Strings spaces = {u8"\u0085", u8"\u00a0", u8"\u1680",
		u8"\u2000", u8"\u2001", u8"\u2002", u8"\u2003", u8"\u2004",
		u8"\u2005", u8"\u2006", u8"\u2007", u8"\u2008", u8"\u2009",
		u8"\u200a", u8"\u2028", u8"\u2029", u8"\u202f", u8"\u205f", u8"\u3000"};
	for ( const std::string& space : spaces )
		if ( value.compare(offset, space.size(), space) == 0 ) return space.size();
	return 0;
}

std::string RStrip(std::string value)
{
	std::size_t end = 0;
	for ( std::size_t offset = 0; offset < value.size(); ) {
		const std::size_t width = WhitespaceWidth(value, offset);
		if ( width ) offset += width;
		else end = ++offset;
	}
	value.resize(end);
	return value;
}

std::string Strip(std::string value)
{
	value = RStrip(value);
	std::size_t start = 0;
	while ( const std::size_t width = WhitespaceWidth(value, start) ) start += width;
	return value.substr(start);
}

Strings Split(const std::string& value, char delimiter)
{
	Strings result;
	std::size_t start = 0;
	for ( std::size_t end = 0; end <= value.size(); ++end ) {
		if ( end == value.size() || value[end] == delimiter ) {
			result.push_back(value.substr(start, end - start));
			start = end + 1;
		}
	}
	return result;
}

Strings Words(const std::string& value)
{
	Strings result;
	std::size_t start = 0;
	for ( std::size_t offset = 0; offset <= value.size(); ) {
		const std::size_t width = WhitespaceWidth(value, offset);
		if ( width || offset == value.size() ) {
			if ( offset > start ) result.push_back(value.substr(start, offset - start));
			if ( offset == value.size() ) break;
			offset += width;
			start = offset;
		} else ++offset;
	}
	return result;
}

Strings TextLines(const std::string& value)
{
	Strings result;
	std::size_t start = 0;
	for ( std::size_t offset = 0; offset < value.size(); ) {
		const unsigned char ch = value[offset];
		std::size_t width = 0;
		if ( (ch >= 10 && ch <= 13) || (ch >= 28 && ch <= 30) ) width = 1;
		else if ( value.compare(offset, 2, u8"\u0085") == 0 ) width = 2;
		else if ( value.compare(offset, 3, u8"\u2028") == 0
			|| value.compare(offset, 3, u8"\u2029") == 0 ) width = 3;
		if ( !width ) { ++offset; continue; }
		result.push_back(value.substr(start, offset - start));
		if ( ch == '\r' && offset + 1 < value.size() && value[offset + 1] == '\n' ) ++width;
		offset += width;
		start = offset;
	}
	if ( start < value.size() ) result.push_back(value.substr(start));
	if ( result.empty() ) result.push_back("");
	return result;
}

std::string Join(const Strings& values, const std::string& separator)
{
	std::string result;
	for ( std::size_t i = 0; i < values.size(); ++i ) {
		if ( i ) result += separator;
		result += values[i];
	}
	return result;
}

std::string Hex(std::uint32_t value)
{
	std::ostringstream result;
	result << std::hex << value;
	return result.str();
}

Numbers ParseNumbers(const std::string& value, int base = 10)
{
	Numbers result;
	for ( const std::string& number : Split(value, ',') ) {
		if ( !number.empty() ) result.push_back(static_cast<std::uint32_t>(
			std::stoul(number, nullptr, base)));
	}
	return result;
}

Dates ParseDates(const std::string& value)
{
	Dates result;
	for ( const std::string& date : Split(value, ',') ) {
		const std::size_t colon = date.find(':');
		if ( colon != std::string::npos )
			result.emplace_back(date.substr(0, colon), date.substr(colon + 1));
	}
	return result;
}

std::string Unquote(const std::string& value)
{
	std::string result;
	for ( std::size_t i = 0; i < value.size(); ++i ) {
		if ( value[i] == '\\' && i + 1 < value.size() ) ++i;
		result += value[i];
	}
	return result;
}

std::string PythonRepr(const std::string& value)
{
	// Match Python's repr used in notes about unsupported citation values.
	const char quote = value.find('\'') != std::string::npos
		&& value.find('"') == std::string::npos ? '"' : '\'';
	std::string result(1, quote);
	for ( unsigned char ch : value ) {
		if ( ch == static_cast<unsigned char>(quote) || ch == '\\' ) {
			result += '\\';
			result += static_cast<char>(ch);
		} else if ( ch == '\n' ) result += "\\n";
		else if ( ch == '\r' ) result += "\\r";
		else if ( ch == '\t' ) result += "\\t";
		else if ( ch < 32 || ch == 127 ) {
			std::ostringstream escaped;
			escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(ch);
			result += escaped.str();
		} else result += static_cast<char>(ch);
	}
	return result + quote;
}

// Keep the established dump decoders as the single decoding path. Capture
// their diagnostic records in memory, using the same contract as the Python
// reference, without starting subprocesses or duplicating binary layouts.
std::string Capture(const std::function<int()>& dump)
{
	std::ostringstream buffer;
	struct Restore {
		std::ios state{nullptr};
		std::streambuf *previous;
		explicit Restore(std::streambuf *replacement) : previous(std::cout.rdbuf()) {
			state.copyfmt(std::cout);
			std::cout.rdbuf(replacement);
		}
		~Restore() {
			std::cout.rdbuf(previous);
			std::cout.copyfmt(state);
		}
	} restore(buffer.rdbuf());
	if ( dump() != 0 ) throw std::runtime_error("record decoding failed");
	return buffer.str();
}

struct ExportData {
	std::map<std::uint32_t, Person> people;
	std::vector<RawPerson> raw;
	std::map<std::uint32_t, Family> families;
	std::map<std::uint32_t, std::string> locations, sources;
	std::map<std::uint32_t, Strings> sourceLines;
	Strings unresolved;
	std::vector<std::pair<std::size_t, std::string>> altkeys, mail;
	Links links;
	void Load();
};

std::string ReadFile(const char *path)
{
	std::ifstream input(path, std::ios::binary);
	if ( !input ) throw std::runtime_error(std::string("cannot open ") + path);
	std::string result((std::istreambuf_iterator<char>(input)), {});
	if ( input.bad() ) throw std::runtime_error(std::string("cannot read ") + path);
	return result;
}

void AppendUtf8(std::string& result, char32_t ch)
{
	if ( ch < 0x80 ) result += static_cast<char>(ch);
	else if ( ch < 0x800 ) {
		result += static_cast<char>(0xc0 | (ch >> 6));
		result += static_cast<char>(0x80 | (ch & 0x3f));
	} else {
		result += static_cast<char>(0xe0 | (ch >> 12));
		result += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
		result += static_cast<char>(0x80 | (ch & 0x3f));
	}
}

std::string Cp437(const std::string& value)
{
	static const std::u32string high =
		U"ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ"
		U"áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
		U"└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
		U"αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";
	std::string result;
	for ( unsigned char ch : value ) AppendUtf8(result, ch < 128 ? ch : high[ch - 128]);
	return result;
}

void ExportData::Load()
{
	TReadPersons persons;
	TReadLookups lookups;
	TReadMarriages marriages;
	const std::string quoted = R"re("((?:[^"\\]|\\.)*)")re";
	Strings citationFields(5, quoted);
	const std::regex personRe(
		R"re(^PERSON_ID_RECORD id=(\d+) address=0x([0-9a-f]+) status=decoded decoded_length=(\d+) name="(.*?)" sex=(.) families=([^ ]*) occupation="(.*?)" notes="(.*?)" location_refs=([^ ]*) source_refs=([^ ]*) source_details=)re"
		+ Join(citationFields, ",") + " source_quality=" + Join(citationFields, ",")
		+ R"re( dates=(.*?)(?: source=([^ ]+))?$)re");
	for ( const std::string& line : Split(Capture([&]() { return persons.DumpIdRecords(); }), '\n') ) {
		std::smatch m;
		if ( std::regex_match(line, m, personRe) ) {
			Person p;
			p.id = std::stoul(m[1]); p.address = m[2]; p.length = m[3];
			p.name = Strip(m[4]); p.sex = m[5] == "M" || m[5] == "F" ? m[5].str() : "";
			p.families = ParseNumbers(m[6], 16); p.occupation = Strip(m[7]);
			for ( const std::string& note : Split(m[8], '|') )
				if ( !note.empty() ) p.notes.push_back(note);
			p.locations = ParseNumbers(m[9]); p.sources = ParseNumbers(m[10]);
			for ( int i = 11; i <= 15; ++i ) p.details.push_back(Unquote(m[i]));
			for ( int i = 16; i <= 20; ++i ) p.quality.push_back(Unquote(m[i]));
			for ( const auto& date : ParseDates(m[21]) ) p.dates[date.first] = date.second;
			people[p.id] = std::move(p);
		} else if ( line.rfind("PERSON_ID_RECORD ", 0) == 0 ) unresolved.push_back(line);
	}
	const std::regex locationRe("^LOCATION_RECORD id=(\\d+) address=0x[0-9a-f]+ name=" + quoted + "$");
	for ( const std::string& line : Split(Capture([&]() { return lookups.DumpLocations(); }), '\n') ) {
		std::smatch m;
		if ( std::regex_match(line, m, locationRe) ) locations[std::stoul(m[1])] = Unquote(m[2]);
	}
	const std::regex sourceRe("^SOURCE_RECORD id=(\\d+) address=0x[0-9a-f]+ line_1="
		+ quoted + " line_2=" + quoted + " line_3=" + quoted + "$");
	for ( const std::string& line : Split(Capture([&]() { return lookups.DumpSources(); }), '\n') ) {
		std::smatch m;
		if ( !std::regex_match(line, m, sourceRe) ) continue;
		Strings lines;
		for ( int i = 2; i <= 4; ++i ) {
			const std::string text = Unquote(m[i]);
			if ( !text.empty() ) lines.push_back(text);
		}
		const std::uint32_t id = std::stoul(m[1]);
		sourceLines[id] = lines; sources[id] = Join(lines, " / ");
	}
	const std::string alt = ReadFile("BK5/ALTKEYS.DT5");
	if ( alt.size() % 162 ) throw std::runtime_error("ALTKEYS.DT5 size is not a multiple of 162");
	for ( std::size_t offset = 0; offset < alt.size(); offset += 162 ) {
		const std::string slot = alt.substr(offset, 162);
		const std::string text = Strip(Cp437(slot.substr(0, slot.find('<'))));
		if ( !text.empty() ) altkeys.emplace_back(offset / 162, text);
	}
	const std::string mailData = ReadFile("BK5/BKMAIL.DT5");
	std::size_t start = 0;
	for ( std::size_t offset = 0; offset <= mailData.size(); ++offset ) {
		if ( offset < mailData.size() && mailData[offset] >= 32 && mailData[offset] < 127 ) continue;
		const std::string text = Strip(mailData.substr(start, offset - start));
		if ( text.size() >= 3 ) mail.emplace_back(start, text);
		start = offset + 1;
	}
	const std::regex rawRe(R"re(^PERSON_RECORD offset=(\d+)(?: person_id=(\d+))? leading_numeric=0x[0-9a-f]+ raw_name="(.*?)" sex=(.) families=([^ ]*) dates=(.*)$)re");
	for ( const std::string& line : Split(Capture([&]() { return persons.DumpRecords(); }), '\n') ) {
		std::smatch m;
		if ( !std::regex_match(line, m, rawRe) ) continue;
		RawPerson p;
		p.offset = m[1]; p.id = m[2].matched ? std::stoul(m[2]) : 0;
		p.xref = p.id ? m[2].str() : "R" + p.offset;
		p.name = Strip(m[3]); p.sex = m[4] == "M" || m[4] == "F" ? m[4].str() : "";
		p.families = ParseNumbers(m[5], 16); p.dates = ParseDates(m[6]);
		raw.push_back(std::move(p));
	}
	const std::regex familyRe(R"re(^FAMILY_RECORD id=(\d+) address=0x([0-9a-f]+) indexed=(yes|no) husband=(\d+) wife=(\d+) date_1="(.*?)" date_2="(.*?)" location_refs=(\d+),(\d+) source_refs=(\d+),(\d+),(\d+) source_details="(.*?)","(.*?)","(.*?)" source_quality="(.*?)","(.*?)","(.*?)" status="(.*?)" children=([\d,]*) stamp=(\S*) flag="(.*?)"$)re");
	for ( const std::string& line : Split(Capture([&]() { return marriages.DumpFamilies(); }), '\n') ) {
		std::smatch m;
		if ( !std::regex_match(line, m, familyRe) ) continue;
		Family f;
		f.address = m[2]; f.husband = std::stoul(m[4]); f.wife = std::stoul(m[5]);
		f.dates = {m[6], m[7]}; f.locations = {static_cast<std::uint32_t>(std::stoul(m[8])), static_cast<std::uint32_t>(std::stoul(m[9]))};
		for ( int i = 10; i <= 12; ++i ) f.sources.push_back(std::stoul(m[i]));
		for ( int i = 13; i <= 15; ++i ) f.details.push_back(m[i]);
		for ( int i = 16; i <= 18; ++i ) f.quality.push_back(m[i]);
		f.status = m[19]; f.children = ParseNumbers(m[20]); f.stamp = m[21];
		families[std::stoul(m[1])] = std::move(f);
	}
	for ( const auto& entry : families ) {
		const Family& f = entry.second;
		for ( std::uint32_t spouse : {f.husband, f.wife} )
			if ( spouse ) links[spouse].emplace_back(entry.first, "FAMS");
		for ( std::uint32_t child : f.children ) links[child].emplace_back(entry.first, "FAMC");
	}
}

std::string GedcomDate(const std::string& value)
{
	static const Strings months = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
		"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
	Strings fields = Words(value);
	if ( fields.size() >= 2 ) {
		std::string month = fields[fields.size() - 2].substr(0, 3);
		for ( char& ch : month ) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
		if ( std::find(months.begin(), months.end(), month) != months.end() )
			fields[fields.size() - 2] = month;
	}
	const std::string date = Join(fields, " ");
	static const std::regex valid("^(?:\\d{1,2} )?(?:(?:JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC) )?\\d{3,4}$");
	return !date.empty() && !std::regex_match(date, valid) ? "(" + date + ")" : date;
}

std::string GedcomName(const std::string& value)
{
	const std::string name = Strip(value);
	if ( name.rfind("THIS INFORMATION", 0) == 0 ) return "//";
	if ( name.find('/') != std::string::npos ) return name;
	Strings fields = Words(name);
	if ( fields.size() < 2 ) return name;
	const std::string surname = fields.back();
	fields.pop_back();
	if ( surname.find('?') != std::string::npos || surname.front() == '(' )
		return Join(fields, " ") + " //";
	return Join(fields, " ") + " /" + surname + "/";
}

bool NameRequiresNote(const std::string& value)
{
	const Strings fields = Words(value);
	return Strip(value).rfind("THIS INFORMATION", 0) == 0
		|| (fields.size() >= 2 && (fields.back().find('?') != std::string::npos
			|| fields.back().front() == '('));
}

std::string RawDisplayName(const std::string& name)
{
	if ( name.rfind("*I", 0) == 0 && name.size() > 3 ) {
		const std::string candidate = Strip(name.substr(2, name.size() - 3));
		if ( !candidate.empty() && std::isalpha(static_cast<unsigned char>(candidate[0]))
			&& candidate.find(' ') != std::string::npos ) return candidate;
	}
	if ( name.rfind("*", 0) == 0 || name.size() <= 11 ) return name;
	const std::string candidate = Strip(name.substr(10, name.size() - 11));
	if ( !candidate.empty() && std::isalpha(static_cast<unsigned char>(candidate[0]))
		&& candidate.find(' ') != std::string::npos ) return candidate;
	return name;
}

std::string FormatBk5Note(const std::string& note)
{
	if ( note.size() != 554 || !std::all_of(note.begin() + 546, note.end(),
		[](unsigned char ch) { return ch >= '0' && ch <= '9'; }) ) return note;
	Strings lines;
	for ( std::size_t offset = 0; offset < 546; offset += 78 )
		lines.push_back(RStrip(note.substr(offset, 78)));
	while ( !lines.empty() && lines.back().empty() ) lines.pop_back();
	lines.push_back("[BK5 note stamp " + note.substr(546) + "]");
	return Join(lines, "\n");
}

std::string FamilyTag(std::uint32_t id) { return "@F" + Hex(id) + "@"; }

std::string Lookup(const std::map<std::uint32_t, std::string>& table, std::uint32_t id)
{
	const auto found = table.find(id);
	return found == table.end() ? "[not decoded]" : found->second;
}

std::string DateAt(const Person& person, const std::string& offset)
{
	const auto found = person.dates.find(offset);
	return found == person.dates.end() ? "" : found->second;
}

std::uint32_t NumberAt(const Numbers& values, std::size_t index)
{
	return index < values.size() ? values[index] : 0;
}

// Python counts Unicode characters rather than UTF-8 bytes when wrapping.
// Keep an index of character boundaries so CONC never divides an encoding.
std::vector<std::size_t> Utf8Boundaries(const std::string& text)
{
	std::vector<std::size_t> result;
	for ( std::size_t i = 0; i < text.size(); ++i )
		if ( (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80 ) result.push_back(i);
	result.push_back(text.size());
	return result;
}

class Writer {
	const ExportData& data;
	bool clean;
	std::ostream& output;
	void Emit(const std::string& line) { output << line << '\n'; }
	void Wrapped(const std::string& text, std::string prefix, const std::string& conc);
	void Note(const std::string& text, int level = 1);
	void AuditNote(const std::string& text) { if ( !clean ) Note(text); }
	void SourceCitation(std::uint32_t source, const std::string& detail, const std::string& quality, int level);
	void Citation(std::size_t slot, std::uint32_t source, const std::string& detail,
		const std::string& quality, const std::string& label = "BK5", bool onEvent = false);
	void FamilyLinks(std::uint32_t id, const Numbers& refs);
	void FamilyStatus(const std::string& status);
public:
	Writer(const ExportData& decoded, bool cleanExport, std::ostream& stream)
		: data(decoded), clean(cleanExport), output(stream) {}
	void Write();
};

void Writer::Wrapped(const std::string& value, std::string prefix, const std::string& conc)
{
	std::string text = RStrip(value);
	while ( true ) {
		const auto boundaries = Utf8Boundaries(text);
		const std::size_t width = 255 - prefix.size();
		if ( boundaries.size() - 1 <= width ) { Emit(prefix + text); return; }
		std::size_t split = width;
		while ( split > 1 && (text[boundaries[split - 1]] == ' ' || text[boundaries[split]] == ' ') ) --split;
		if ( split == 1 ) split = width;
		Emit(prefix + text.substr(0, boundaries[split]));
		text.erase(0, boundaries[split]);
		prefix = conc;
	}
}

void Writer::Note(const std::string& text, int level)
{
	const Strings lines = TextLines(text);
	for ( std::size_t i = 0; i < lines.size(); ++i )
		Wrapped(lines[i], std::to_string(i ? level + 1 : level) + (i ? " CONT " : " NOTE "),
			std::to_string(level + 1) + " CONC ");
}

void Writer::SourceCitation(std::uint32_t source, const std::string& detail,
	const std::string& quality, int level)
{
	Emit(std::to_string(level) + " SOUR @S" + std::to_string(source) + "@");
	if ( !detail.empty() ) Emit(std::to_string(level + 1) + " PAGE " + detail);
	if ( quality.size() == 1 && quality[0] >= '0' && quality[0] <= '3' )
		Emit(std::to_string(level + 1) + " QUAY " + quality);
}

void Writer::Citation(std::size_t slot, std::uint32_t source, const std::string& detail,
	const std::string& quality, const std::string& label, bool onEvent)
{
	if ( !source ) {
		if ( !detail.empty() || !quality.empty() ) Note(label + " citation slot " + std::to_string(slot)
			+ " without source: page=" + PythonRepr(detail) + " quality=" + PythonRepr(quality));
		return;
	}
	if ( data.sources.count(source) && !onEvent ) SourceCitation(source, detail, quality, 1);
	if ( data.sources.count(source) && !quality.empty()
		&& !(quality.size() == 1 && quality[0] >= '0' && quality[0] <= '3') )
		Note(label + " citation slot " + std::to_string(slot) + ": quality code " + PythonRepr(quality)
			+ " is outside GEDCOM's 0-3 range");
	const std::string text = label + " source slot " + std::to_string(slot) + " ("
		+ std::to_string(source) + "): " + Lookup(data.sources, source);
	if ( data.sources.count(source) ) AuditNote(text);
	else Note(text);
}

void Writer::FamilyLinks(std::uint32_t id, const Numbers& refs)
{
	std::map<std::uint32_t, std::size_t> order;
	for ( std::size_t i = 0; i < refs.size(); ++i ) if ( refs[i] ) order[refs[i]] = i;
	std::vector<Membership> memberships;
	const auto found = data.links.find(id);
	if ( found != data.links.end() ) memberships = found->second;
	auto key = [&](const Membership& m) {
		return std::make_tuple(m.second != "FAMC", order.count(m.first) ? order.at(m.first) : order.size(), m.first);
	};
	std::stable_sort(memberships.begin(), memberships.end(), [&](const Membership& a, const Membership& b) { return key(a) < key(b); });
	std::set<std::uint32_t> confirmed;
	for ( const auto& m : memberships ) {
		Emit("1 " + m.second + " " + FamilyTag(m.first));
		confirmed.insert(m.first);
	}
	Strings unconfirmed;
	for ( std::uint32_t family : refs )
		if ( family && !confirmed.count(family) ) unconfirmed.push_back(FamilyTag(family));
	if ( !unconfirmed.empty() ) Note("BK5 person family references not listed in BKMARR.DT5: " + Join(unconfirmed, ", "));
}

void Writer::FamilyStatus(const std::string& value)
{
	std::string status = value;
	status.resize(std::max<std::size_t>(6, status.size()), ' ');
	const std::string ceremony = Strip(status.substr(0, 1));
	const std::string code = Strip(status.substr(1, 1));
	const std::string year = Strip(status.substr(2, 4));
	if ( ceremony == "N" ) Note("BK5 marriage ceremony field: N (no ceremony).");
	if ( code == "D" || code == "A" ) {
		const std::string tag = code == "D" ? "DIV" : "ANUL";
		Emit("1 " + tag + (year.empty() ? " Y" : ""));
		if ( !year.empty() ) Emit("2 DATE " + GedcomDate(year));
	} else if ( code == "S" ) {
		Emit("1 EVEN"); Emit("2 TYPE Separated");
		if ( !year.empty() ) Emit("2 DATE " + GedcomDate(year));
	} else if ( !code.empty() ) {
		Note("BK5 marriage end code " + PythonRepr(code) + " is not documented in CHANGES.DOC (Divorced/Annulled/Separated); end year: "
			+ (year.empty() ? "none" : year));
	} else if ( !year.empty() ) Note("BK5 marriage end year: " + year + " (no reason recorded).");
}

void Writer::Write()
{
	std::map<std::uint32_t, const RawPerson *> rawById;
	std::vector<const RawPerson *> unmerged;
	std::set<std::uint32_t> unmergedIds;
	for ( const RawPerson& raw : data.raw ) {
		if ( raw.id ) rawById[raw.id] = &raw;
		if ( !raw.id || !data.people.count(raw.id) ) {
			unmerged.push_back(&raw); unmergedIds.insert(raw.id);
		}
	}
	Emit("0 HEAD"); Emit("1 SOUR dlbk5dump"); Emit("1 SUBM @U1@");
	Emit("1 GEDC"); Emit("2 VERS 5.5.1"); Emit("2 FORM LINEAGE-LINKED");
	Strings header = {
		"This is an intermediate export of decoded BK5 fixed records.",
		"Raw-only and unresolved records require a later reconciliation pass.",
		"Families, spouses, children and marriage details come from the",
		std::to_string(data.families.size()) + " BKMARR.DT5 family records; person-record family references",
		"that BKMARR.DT5 does not confirm are kept as notes.",
		std::to_string(data.altkeys.size()) + " non-empty ALTKEYS.DT5 slots are preserved without a person link."
	};
	for ( const auto& slot : data.altkeys ) header.push_back("ALTKEYS slot " + std::to_string(slot.first) + ": " + slot.second);
	header.push_back(std::to_string(data.mail.size()) + " unassigned BKMAIL.DT5 text runs are preserved.");
	for ( const auto& run : data.mail ) header.push_back("BKMAIL offset 0x" + Hex(run.first) + ": " + run.second);
	header.push_back("BK5 date slots 1-4 are exported as BIRT, BAPM, DEAT, and BURI;");
	header.push_back("slot 5 remains an audit note because it is the other-date field.");
	if ( clean ) Note("Exported from Brother's Keeper 5 data by dlbk5dump. Extraction notes are omitted; run without --clean for the audit export.");
	else Note(Join(header, "\n"));
	Emit("1 CHAR UTF-8"); Emit("0 @U1@ SUBM"); Emit("1 NAME dlbk5dump BK5 extraction");
	for ( const auto& source : data.sourceLines ) {
		Emit("0 @S" + std::to_string(source.first) + "@ SOUR");
		Emit(RStrip("1 TITL BK5 source " + std::to_string(source.first) + ": "
			+ (source.second.empty() ? "" : source.second.front())));
		for ( std::size_t i = 1; i < source.second.size(); ++i ) Emit("2 CONT " + source.second[i]);
	}
	for ( const auto& entry : data.people ) {
		const Person& p = entry.second;
		Emit("0 @I" + std::to_string(p.id) + "@ INDI");
		Emit("1 NAME " + GedcomName(p.name));
		if ( NameRequiresNote(p.name) ) Note("BK5 original name: " + p.name);
		if ( !p.sex.empty() ) Emit("1 SEX " + p.sex);
		if ( !p.occupation.empty() ) Emit("1 OCCU " + p.occupation);
		for ( const std::string& note : p.notes ) Note("BK5: " + FormatBk5Note(note));
		for ( std::size_t slot = 0; slot < p.locations.size(); ++slot ) {
			const std::uint32_t location = p.locations[slot];
			if ( !location ) continue;
			const std::string text = "BK5 location slot " + std::to_string(slot + 1)
				+ " (" + std::to_string(location) + "): " + Lookup(data.locations, location);
			if ( slot < 4 && data.locations.count(location) ) AuditNote(text);
			else Note(text);
		}
		for ( std::size_t slot = 0; slot < p.sources.size(); ++slot ) {
			const bool onEvent = slot < 4 && (!DateAt(p, DATE_SLOTS[slot]).empty() || NumberAt(p.locations, slot));
			Citation(slot + 1, p.sources[slot], p.details.at(slot), p.quality.at(slot), "BK5", onEvent);
		}
		for ( std::size_t slot = 0; slot < DATE_SLOTS.size(); ++slot ) {
			const std::string date = DateAt(p, DATE_SLOTS[slot]);
			if ( slot == 4 ) {
				if ( !date.empty() ) Note("BK5 date slot 5 (" + DATE_SLOTS[slot] + "): " + GedcomDate(date));
				continue;
			}
			const std::uint32_t location = NumberAt(p.locations, slot);
			if ( date.empty() && !location ) continue;
			Emit("1 " + DATE_TAGS[slot]);
			if ( !date.empty() ) Emit("2 DATE " + GedcomDate(date));
			if ( location ) Emit("2 PLAC " + Lookup(data.locations, location));
			const std::uint32_t source = NumberAt(p.sources, slot);
			if ( data.sources.count(source) ) SourceCitation(source, p.details.at(slot), p.quality.at(slot), 2);
		}
		for ( const auto& date : p.dates )
			if ( std::find(DATE_SLOTS.begin(), DATE_SLOTS.end(), date.first) == DATE_SLOTS.end() )
				Note("BK5 date at " + date.first + ": " + GedcomDate(date.second));
		FamilyLinks(p.id, p.families);
		AuditNote("BK5 fixed record address 0x" + p.address + "; expanded length " + p.length);
		if ( rawById.count(p.id) ) {
			const RawPerson& raw = *rawById.at(p.id);
			AuditNote("BK5 raw record offset " + raw.offset);
			AuditNote("BK5 raw name: " + raw.name);
			for ( const auto& date : raw.dates ) AuditNote("BK5 raw date at " + date.first + ": " + GedcomDate(date.second));
			Strings extra;
			for ( std::uint32_t family : raw.families )
				if ( family && std::find(p.families.begin(), p.families.end(), family) == p.families.end() )
					extra.push_back(FamilyTag(family));
			if ( !extra.empty() ) AuditNote("BK5 raw family references not present in fixed record: " + Join(extra, ", "));
		}
	}
	for ( const RawPerson *raw : unmerged ) {
		Emit("0 @I" + raw->xref + "@ INDI"); Emit("1 NAME " + GedcomName(RawDisplayName(raw->name)));
		AuditNote("BK5 raw name: " + raw->name);
		if ( !raw->sex.empty() ) Emit("1 SEX " + raw->sex);
		for ( const auto& date : raw->dates ) Note("BK5 raw date at " + date.first + ": " + GedcomDate(date.second));
		if ( raw->id ) FamilyLinks(raw->id, raw->families);
		else for ( std::uint32_t family : raw->families )
			if ( family ) Note("BK5 raw family reference " + FamilyTag(family) + " (role unresolved)");
		AuditNote("BK5 raw person record offset " + raw->offset);
	}
	for ( const auto& link : data.links ) {
		if ( data.people.count(link.first) || unmergedIds.count(link.first) ) continue;
		Emit("0 @I" + std::to_string(link.first) + "@ INDI"); Emit("1 NAME //"); FamilyLinks(link.first, {});
		Note("BK5 person " + std::to_string(link.first) + " is listed in BKMARR.DT5 but its person record has no decoded name.");
	}
	for ( const auto& entry : data.families ) {
		const Family& f = entry.second;
		Emit("0 " + FamilyTag(entry.first) + " FAM");
		if ( f.husband ) Emit("1 HUSB @I" + std::to_string(f.husband) + "@");
		if ( f.wife ) Emit("1 WIFE @I" + std::to_string(f.wife) + "@");
		for ( std::uint32_t child : f.children ) Emit("1 CHIL @I" + std::to_string(child) + "@");
		const std::uint32_t location = f.locations[0];
		if ( !f.dates[0].empty() || location ) {
			Emit("1 MARR");
			if ( !f.dates[0].empty() ) Emit("2 DATE " + GedcomDate(f.dates[0]));
			if ( location ) Emit("2 PLAC " + Lookup(data.locations, location));
		} else if ( !f.status.empty() && f.status[0] == 'Y' ) Emit("1 MARR Y");
		FamilyStatus(f.status);
		if ( location ) AuditNote("BK5 family location slot 1 (" + std::to_string(location) + "): " + Lookup(data.locations, location));
		if ( !f.dates[1].empty() || f.locations[1] ) Note("BK5 family second date/location slot: " + GedcomDate(f.dates[1]) + " " + std::to_string(f.locations[1]));
		for ( std::size_t slot = 0; slot < f.sources.size(); ++slot ) Citation(slot + 1, f.sources[slot], f.details[slot], f.quality[slot], "BK5 family");
		if ( !f.status.empty() ) AuditNote("BK5 family status field (ceremony, end code, end year): " + f.status);
		AuditNote("BK5 family record address 0x" + f.address + "; stamp " + f.stamp);
	}
	if ( !clean ) {
		Strings lines = {std::to_string(data.unresolved.size()) + " indexed entries were not decoded or were not named."};
		lines.insert(lines.end(), data.unresolved.begin(), data.unresolved.end());
		for ( std::size_t i = 0; i < lines.size(); ++i ) Wrapped(lines[i], i ? "1 CONT " : "0 @NUNRESOLVED@ NOTE ", "1 CONC ");
	}
	Emit("0 TRLR");
}
}

int ExportGedcom(bool clean)
{
	try {
		ExportData data;
		data.Load();
		// Finish the document before touching stdout, so decoding failures
		// cannot produce a plausible but incomplete GEDCOM.
		std::ostringstream document;
		Writer(data, clean, document).Write();
		std::cout << document.str();
		if ( !std::cout ) throw std::runtime_error("cannot write GEDCOM output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
