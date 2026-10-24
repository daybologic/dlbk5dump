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

#include "gedcom_check.hpp"

#include "gedcom_reader.hpp"
#include "text.hpp"
#include "unicode.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Strings = std::vector<std::string>;
using UnicodeText::Repr;
struct Entry {
	std::uint64_t level = 0;
	std::string xref, tag, value;
	std::size_t line = 0;
};
using Record = std::vector<Entry>;
std::string Join(const Strings& values, const char *separator = ", ") {
	std::string result;
	for ( const auto& value : values ) { if ( !result.empty() ) result += separator; result += value; }
	return result;
}
std::string List(const Strings& values, std::size_t limit = static_cast<std::size_t>(-1)) {
	return "[" + Join(Strings(values.begin(), values.begin() + std::min(limit, values.size()))) + "]";
}
std::string Tuple(const Strings& values) { return "(" + Join(values) + ")"; }
std::string Id(const std::string& value) { return value.empty() ? "None" : Repr(value); }
Strings Tags(const Record& record, const std::string& tag, std::uint64_t level = 1) {
	Strings result;
	for ( const auto& entry : record ) if ( entry.level == level && entry.tag == tag ) result.push_back(entry.value);
	return result;
}
Strings Quoted(const Strings& values) {
	Strings result;
	for ( const auto& value : values ) result.push_back(Repr(value));
	return result;
}
bool Has(const Strings& values, const std::string& value) {
	return std::find(values.begin(), values.end(), value) != values.end();
}
bool Parse(const std::string& line, Entry& entry) {
	std::size_t position = 0;
	while ( position < line.size() ) {
		auto next = position;
		if ( UnicodeText::Decimal(UnicodeText::Next(line, next)) < 0 ) break;
		position = next;
	}
	if ( position == 0 || position >= line.size() || line[position] != ' ' ) return false;
	entry.level = std::stoull(UnicodeText::AsciiDigits(line.substr(0, position)));
	const auto tagBegin = ++position;
	auto parseTag = [&](std::size_t start) {
		std::size_t end = start;
		while ( end < line.size() && !BK5Text::WhitespaceWidth(line, end) ) UnicodeText::Next(line, end);
		if ( end == start || (end < line.size() && line[end] != ' ') ) return false;
		entry.tag = line.substr(start, end - start);
		entry.value = end < line.size() ? line.substr(end + 1) : "";
		return true;
	};
	if ( position < line.size() && line[position] == '@' ) {
		const auto close = line.find('@', position + 1);
		if ( close != std::string::npos && close > position + 1
			&& close + 1 < line.size() && line[close + 1] == ' ' && parseTag(close + 2) ) {
			entry.xref = line.substr(position, close - position + 1); return true;
		}
	}
	return parseTag(tagBegin);
}
struct Counter {
	std::vector<std::pair<std::string, std::size_t>> counts;
	void Add(const std::string& key) {
		for ( auto& item : counts ) if ( item.first == key ) { ++item.second; return; }
		counts.emplace_back(key, 1);
	}
	std::string Format() const {
		if ( counts.empty() ) return "Counter()";
		auto sorted = counts;
		std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
		Strings fields;
		for ( const auto& item : sorted ) fields.push_back(Repr(item.first) + ": " + std::to_string(item.second));
		return "Counter({" + Join(fields) + "})";
	}
};
struct OrderedRecords {
	std::vector<std::pair<std::string, const Record *>> items;
	std::map<std::string, const Record *> lookup;
	void Add(const Record& record) {
		const auto& id = record.front().xref;
		if ( !lookup.count(id) ) items.emplace_back(id, &record);
		else for ( auto& item : items ) if ( item.first == id ) item.second = &record;
		lookup[id] = &record;
	}
};
int Year(const OrderedRecords& people, const std::string& id) {
	const auto person = people.lookup.find(id);
	if ( person == people.lookup.end() ) return 0;
	const auto& record = *person->second;
	const std::regex yearRe("[0-9]{4}");
	for ( std::size_t i = 0; i < record.size(); ++i ) if ( record[i].tag == "BIRT" )
		for ( std::size_t j = i + 1; j < record.size() && record[j].level > 1; ++j )
			if ( record[j].tag == "DATE" ) {
				std::smatch match;
				const auto date = UnicodeText::AsciiDigits(record[j].value);
				return std::regex_search(date, match, yearRe) ? std::stoi(match[0]) : 0;
			}
	return 0;
}
void Report(const std::string& path, std::ostream& out) {
	auto lines = GedcomInput::ReadLines(path, true);
	if ( !lines.empty() && lines.back().empty() ) lines.pop_back();
	std::vector<Record> records;
	std::map<std::string, std::vector<std::size_t>> byKey;
	Strings keys, bad;
	std::uint64_t previous = 0;
	bool first = true;
	for ( std::size_t i = 0; i < lines.size(); ++i ) {
		Entry entry;
		entry.line = i + 1;
		if ( !Parse(lines[i], entry) ) { bad.push_back(Tuple({std::to_string(i + 1), Repr("syntax"), Repr(lines[i])})); continue; }
		if ( entry.level > (first ? 0 : previous + 1) ) bad.push_back(Tuple({std::to_string(i + 1), Repr("level jump"), Repr(lines[i])}));
		previous = entry.level; first = false;
		if ( UnicodeText::Length(lines[i]) > 255 ) bad.push_back(Tuple({std::to_string(i + 1), Repr("line>255"), Repr(UnicodeText::Prefix(lines[i], 60))}));
		if ( entry.level == 0 ) {
			const auto key = entry.xref.empty() ? entry.tag : entry.xref;
			if ( !byKey.count(key) ) keys.push_back(key);
			byKey[key].push_back(records.size());
			records.push_back({});
		} else if ( records.empty() ) throw std::runtime_error("GEDCOM data precedes its first record");
		records.back().push_back(entry);
	}
	out << "bad lines " << bad.size() << '\n';
	for ( std::size_t i = 0; i < std::min<std::size_t>(20, bad.size()); ++i ) out << "  " << bad[i] << '\n';
	Strings duplicates;
	for ( const auto& key : keys ) if ( byKey.at(key).size() > 1 && key[0] == '@' ) duplicates.push_back(Repr(key));
	out << "dup xrefs " << List(duplicates, 10) << '\n';
	Strings submitters;
	if ( byKey.count("HEAD") ) submitters = Tags(records[byKey.at("HEAD")[0]], "SUBM");
	const bool validSubmitter = !submitters.empty() && byKey.count(submitters[0])
		&& records[byKey.at(submitters[0])[0]][0].tag == "SUBM";
	out << "header SUBM " << List(Quoted(submitters)) << ' ' << (validSubmitter ? "ok" : "MISSING (required by 5.5.1)") << '\n';
	Counter types, events;
	OrderedRecords people, families;
	Strings dangling;
	std::set<std::pair<std::string, std::string>> danglingSet;
	for ( const auto& record : records ) {
		types.Add(record[0].tag);
		if ( record[0].tag == "INDI" ) people.Add(record);
		if ( record[0].tag == "FAM" ) families.Add(record);
		for ( std::size_t i = 0; i < record.size(); ++i ) {
			const auto& entry = record[i];
			if ( entry.level == 1 ) events.Add(entry.tag);
			const auto& value = entry.value;
			if ( i && value.size() > 2 && value[0] == '@' && value.back() == '@'
				&& value.find('@', 1) == value.size() - 1 && !byKey.count(value)
				&& danglingSet.emplace(entry.tag, value).second ) dangling.push_back(Tuple({Repr(entry.tag), Repr(value)}));
		}
	}
	out << types.Format() << '\n' << "dangling pointers " << dangling.size() << ' ' << List(dangling, 15) << '\n';
	Strings reciprocal, multi, sexbad, isolated, noname, odd, famc2, cycles, dates, chronology;
	std::size_t emptyFamilies = 0, smallFamilies = 0, noSurname = 0, noBirth = 0;
	for ( const auto& item : families.items ) {
		const auto& f = item.first; const auto& record = *item.second;
		for ( const auto& role : {std::make_pair("HUSB", "FAMS"), std::make_pair("WIFE", "FAMS"), std::make_pair("CHIL", "FAMC")} )
			for ( const auto& p : Tags(record, role.first) )
				if ( people.lookup.count(p) && !Has(Tags(*people.lookup.at(p), role.second), f) )
					reciprocal.push_back(Tuple({Id(f), Repr(role.first), Repr(p)}));
		const auto husbands = Tags(record, "HUSB"), wives = Tags(record, "WIFE"), children = Tags(record, "CHIL");
		if ( husbands.size() > 1 || wives.size() > 1 ) multi.push_back(Tuple({Id(f), std::to_string(husbands.size()), std::to_string(wives.size())}));
		for ( const auto& role : {std::make_pair("HUSB", "M"), std::make_pair("WIFE", "F")} )
			for ( const auto& p : Tags(record, role.first) ) if ( people.lookup.count(p) ) {
				const auto sex = Tags(*people.lookup.at(p), "SEX");
				if ( sex.empty() || sex[0] != role.second ) sexbad.push_back(Tuple({Id(f), Repr(role.first), Repr(p), List(Quoted(sex)), List(Quoted(Tags(*people.lookup.at(p), "NAME")))}));
			}
		if ( husbands.empty() && wives.empty() ) ++emptyFamilies;
		if ( husbands.size() + wives.size() + children.size() <= 1 ) ++smallFamilies;
		bool cycle = false;
		Strings parents = husbands; parents.insert(parents.end(), wives.begin(), wives.end());
		for ( const auto& child : children ) if ( Has(parents, child) ) cycle = true;
		if ( cycle ) cycles.push_back(Id(f));
		for ( const auto& parent : parents ) for ( const auto& child : children ) {
			const int a = Year(people, parent), b = Year(people, child);
			if ( a && b && (b - a < 13 || b - a > 65) ) chronology.push_back(Tuple({Id(f), Repr(parent), std::to_string(a), Repr(child), std::to_string(b)}));
		}
	}
	for ( const auto& item : people.items ) {
		const auto& p = item.first; const auto& record = *item.second;
		for ( const auto& f : Tags(record, "FAMS") ) if ( families.lookup.count(f) ) {
			const auto& family = *families.lookup.at(f);
			if ( !Has(Tags(family, "HUSB"), p) && !Has(Tags(family, "WIFE"), p) ) reciprocal.push_back(Tuple({Id(p), Repr("FAMS"), Repr(f)}));
		}
		for ( const auto& f : Tags(record, "FAMC") ) if ( families.lookup.count(f) && !Has(Tags(*families.lookup.at(f), "CHIL"), p) )
			reciprocal.push_back(Tuple({Id(p), Repr("FAMC"), Repr(f)}));
		if ( Tags(record, "FAMC").empty() && Tags(record, "FAMS").empty() ) isolated.push_back(Id(p));
		const auto names = Tags(record, "NAME");
		if ( names.empty() ) noname.push_back(Id(p));
		if ( !names.empty() && names[0].find('/') == std::string::npos ) ++noSurname;
		for ( const auto& name : names ) {
			bool strange = false;
			for ( unsigned char ch : name ) if ( !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || std::string(" .'-,/()").find(ch) != std::string::npos) ) strange = true;
			if ( strange ) odd.push_back(Tuple({Id(p), Repr(name)}));
		}
		if ( Tags(record, "FAMC").size() > 1 ) famc2.push_back(Id(p));
		if ( Tags(record, "BIRT").empty() ) ++noBirth;
	}
	out << "non-reciprocal links " << reciprocal.size() << ' ' << List(reciprocal, 15) << '\n'
		<< "fams with >1 HUSB/WIFE " << multi.size() << ' ' << List(multi, 15) << '\n'
		<< "role/sex mismatch " << sexbad.size() << '\n';
	for ( std::size_t i = 0; i < std::min<std::size_t>(20, sexbad.size()); ++i ) out << "  " << sexbad[i] << '\n';
	out << "fams without spouses " << emptyFamilies << '\n' << "fams with <=1 member " << smallFamilies << '\n'
		<< "isolated INDI " << isolated.size() << ' ' << List(isolated, 10) << '\n'
		<< "no NAME " << noname.size() << '\n' << "NAME without /surname/ " << noSurname << " of " << people.items.size() << '\n'
		<< "names w/ odd chars " << odd.size() << ' ' << List(odd, 25) << '\n'
		<< "INDI with >1 FAMC " << famc2.size() << ' ' << List(famc2, 10) << '\n';
	const std::regex dateRe(R"(^(((ABT|BEF|AFT|EST|CAL) )?(([0-9]{1,2} )?(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC) )?[0-9]{3,4}(/[0-9][0-9])?|\([^()]*\))$)");
	for ( const auto& record : records ) for ( const auto& entry : record )
		if ( entry.tag == "DATE" && !std::regex_match(UnicodeText::AsciiDigits(entry.value), dateRe) )
			dates.push_back(Tuple({std::to_string(entry.line), Repr(entry.value)}));
	out << "non-standard DATE values " << dates.size() << ' ' << List(dates, 25) << '\n'
		<< events.Format() << '\n' << "INDI without BIRT " << noBirth << '\n'
		<< "fams where spouse is also child " << List(cycles) << '\n'
		<< "implausible parent/child birth gaps " << chronology.size() << '\n';
	for ( std::size_t i = 0; i < std::min<std::size_t>(30, chronology.size()); ++i ) out << "  " << chronology[i] << '\n';
}
}

int CheckGedcom(const std::string& path)
{
	try {
		std::ostringstream report;
		Report(path, report);
		std::cout << report.str();
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write GEDCOM check output");
		// Like the independent reference, findings are report-only.
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
