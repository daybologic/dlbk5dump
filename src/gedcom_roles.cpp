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

#include "gedcom_roles.hpp"

#include "gedcom_reader.hpp"
#include "text.hpp"
#include "unicode.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Strings = std::vector<std::string>;
using Year = std::optional<int>;
struct RoleData {
	std::map<std::string, Strings> records;
	Strings order;
	explicit RoleData(const std::string& path) {
		std::string current;
		for ( const auto& line : GedcomInput::ReadLines(path) ) {
			bool header = false;
			if ( line.rfind("0 @", 0) == 0 ) {
				const auto end = line.find('@', 3);
				if ( end != std::string::npos && end > 3 && end + 2 < line.size() && line[end + 1] == ' ' ) {
					auto offset = end + 2;
					header = UnicodeText::IsWord(UnicodeText::Next(line, offset));
					if ( header ) {
						current = line.substr(2, end - 1);
						if ( !records.count(current) ) order.push_back(current);
						records[current] = {};
					}
				}
			}
			if ( header ) continue;
			if ( line.rfind("0 ", 0) == 0 ) { current.clear(); continue; }
			if ( !current.empty() ) records.at(current).push_back(line);
		}
	}
	const Strings& Get(const std::string& id) const {
		const auto found = records.find(id);
		if ( found == records.end() ) throw std::runtime_error("unresolved GEDCOM individual " + id);
		return found->second;
	}
	Year EventYear(const Strings& record, const std::string& event) const {
		const std::regex yearRe("[0-9]{4}");
		for ( std::size_t i = 0; i < record.size(); ++i ) if ( record[i] == "1 " + event ) {
			for ( std::size_t j = i + 1; j < record.size() && record[j].rfind("2 ", 0) == 0; ++j ) {
				if ( record[j].rfind("2 DATE ", 0) != 0 ) continue;
				const auto date = UnicodeText::AsciiDigits(record[j].substr(7));
				std::smatch match;
				if ( std::regex_search(date, match, yearRe) ) return std::stoi(match[0]);
			}
		}
		return std::nullopt;
	}
	Year Birth(const std::string& id) const {
		const auto& record = Get(id);
		const auto birth = EventYear(record, "BIRT");
		return birth && *birth ? birth : EventYear(record, "BAPM");
	}
	std::string Name(const std::string& id) const {
		for ( const auto& line : Get(id) ) if ( line.rfind("1 NAME", 0) == 0 ) return line.size() > 7 ? line.substr(7) : "";
		return "?";
	}
};
std::string Pointer(const std::string& line) {
	const auto fields = BK5Text::Words(line);
	if ( fields.size() < 3 ) throw std::runtime_error("incomplete GEDCOM family pointer");
	return fields[2];
}
std::string YearList(const std::vector<Year>& years) {
	std::string text = "[";
	for ( std::size_t i = 0; i < years.size(); ++i ) {
		if ( i ) text += ", ";
		text += years[i] ? std::to_string(*years[i]) : "None";
	}
	return text + "]";
}
}

int CheckGedcomRoles(const std::string& path)
{
	try {
		const RoleData data(path);
		std::size_t oneSpouse = 0;
		for ( const auto& id : data.order ) {
			if ( id.rfind("@F", 0) != 0 ) continue;
			const auto& record = data.Get(id);
			Strings spouses, children;
			for ( const auto& line : record ) {
				if ( line.rfind("1 HUSB", 0) == 0 || line.rfind("1 WIFE", 0) == 0 ) spouses.push_back(Pointer(line));
				if ( line.rfind("1 CHIL", 0) == 0 ) children.push_back(Pointer(line));
			}
			if ( spouses.size() == 1 ) ++oneSpouse;
			const auto marriage = data.EventYear(record, "MARR");
			for ( const auto& child : children ) {
				const auto birth = data.Birth(child);
				std::vector<Year> spouseYears;
				for ( const auto& spouse : spouses ) spouseYears.push_back(data.Birth(spouse));
				Strings flags;
				if ( marriage && *marriage && birth && *birth && *birth < *marriage - 15 )
					flags.push_back("born " + std::to_string(*birth) + ", " + std::to_string(*marriage - *birth) + "y before marriage " + std::to_string(*marriage));
				bool close = false;
				if ( birth && *birth ) for ( const auto& year : spouseYears ) if ( year && *year && std::abs(*year - *birth) < 12 ) close = true;
				if ( close ) flags.push_back("born " + std::to_string(*birth) + ", spouse born " + YearList(spouseYears));
				if ( !flags.empty() ) {
					std::cout << id << ' ' << child << ' ' << data.Name(child) << " | ";
					for ( std::size_t i = 0; i < flags.size(); ++i ) { if ( i ) std::cout << ';'; std::cout << flags[i]; }
					std::cout << '\n';
				}
			}
		}
		std::cout << "families with exactly one spouse: " << oneSpouse << '\n';
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write GEDCOM role output");
		// Warnings are nonfatal in the independent reference.
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
