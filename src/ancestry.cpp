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

#include "ancestry.hpp"

#include "gedcom_reader.hpp"

#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Strings = std::vector<std::string>;
struct Person {
	std::string name;
	std::vector<std::pair<std::string, std::string>> events;
	Strings famc;
};
struct Family {
	Strings parents;
	std::string date;
};
struct Ancestry {
	std::map<std::string, Person> people;
	std::map<std::string, Family> families;

	explicit Ancestry(const std::string& path) {
		std::string current;
		const std::regex personRe("^0 (@I[^ ]+@) INDI$"), familyRe("^0 (@F[^ ]+@) FAM$");
		const std::regex eventRe("^1 (BIRT|BAPM|DEAT|BURI)$");
		const std::regex parentRe("^1 (?:HUSB|WIFE) (@I[^ ]+@)$");
		for ( const auto& line : GedcomInput::ReadLines(path) ) {
			std::smatch match;
			if ( std::regex_match(line, match, personRe) ) {
				current = match[1]; people[current] = Person{}; continue;
			}
			if ( std::regex_match(line, match, familyRe) ) {
				current = match[1]; families[current] = Family{}; continue;
			}
			if ( people.count(current) ) {
				auto& person = people.at(current);
				if ( line.rfind("1 NAME ", 0) == 0 ) person.name = line.substr(7);
				else if ( std::regex_match(line, eventRe) ) person.events.emplace_back(line.substr(2), "");
				else if ( line.rfind("2 DATE ", 0) == 0 && !person.events.empty() ) person.events.back().second = line.substr(7);
				else if ( line.rfind("1 FAMC ", 0) == 0 ) person.famc.push_back(line.substr(7));
			} else if ( families.count(current) ) {
				auto& family = families.at(current);
				if ( std::regex_match(line, match, parentRe) ) family.parents.push_back(match[1]);
				else if ( line.rfind("2 DATE ", 0) == 0 ) family.date = line.substr(7);
			}
		}
	}

	void Tree(const std::string& id, std::size_t depth, std::set<std::string>& seen, std::ostream& out) const {
		const std::string indent(depth * 2, ' ');
		auto found = people.find(id);
		if ( found == people.end() ) { out << indent << id << " [unresolved]\n"; return; }
		const auto& person = found->second;
		out << indent << id << ' ' << person.name;
		for ( const auto& event : person.events ) if ( !event.second.empty() ) {
			std::string tag = event.first;
			for ( char& ch : tag ) ch += 'a' - 'A';
			out << ' ' << tag << ' ' << event.second;
		}
		out << '\n';
		if ( !seen.insert(id).second ) { out << indent << "  [cycle suppressed]\n"; return; }
		for ( const auto& familyId : person.famc ) {
			const auto family = families.find(familyId);
			out << indent << "  " << familyId;
			if ( family == families.end() ) { out << " [unresolved family]\n"; continue; }
			if ( !family->second.date.empty() ) out << " marriage " << family->second.date;
			out << '\n';
			for ( const auto& parent : family->second.parents ) Tree(parent, depth + 2, seen, out);
		}
	}

	void Terminals(const std::string& id, std::set<std::string>& seen, Strings& result) const {
		if ( !seen.insert(id).second ) return;
		const auto person = people.find(id);
		if ( person == people.end() ) return;
		bool followed = false;
		for ( const auto& familyId : person->second.famc ) {
			const auto family = families.find(familyId);
			if ( family == families.end() ) continue;
			followed = true;
			for ( const auto& parent : family->second.parents ) Terminals(parent, seen, result);
		}
		if ( !followed ) result.push_back(id);
	}
};
}

int RunAncestry(int argc, char **argv)
{
	std::string path, person = "3";
	bool terminals = false, positional = false;
	auto usage = []() {
		std::cerr << "Usage: dlbk5dump --ancestry GEDCOM [--person ID] [--terminals]\n";
		return 2;
	};
	for ( int i = 0; i < argc; ++i ) {
		const std::string arg = argv[i];
		if ( !positional && arg == "--" ) { positional = true; continue; }
		if ( !positional && arg == "--terminals" ) terminals = true;
		else if ( !positional && arg == "--person" ) {
			if ( ++i == argc ) return usage();
			person = argv[i];
		} else if ( !positional && arg.rfind("--person=", 0) == 0 ) person = arg.substr(9);
		else if ( !positional && (arg == "--help" || arg == "-h") ) {
			std::cout << "Usage: dlbk5dump --ancestry GEDCOM [--person ID] [--terminals]\n";
			return 0;
		} else if ( !positional && arg.size() > 1 && arg[0] == '-' ) return usage();
		else if ( !path.empty() ) return usage();
		else path = arg;
	}
	if ( path.empty() ) return usage();
	try {
		const Ancestry data(path);
		const std::string id = "@I" + person + "@";
		if ( !data.people.count(id) ) {
			std::cerr << "dlbk5dump: person " << person << " is not present in " << path << '\n';
			return 2;
		}
		std::set<std::string> seen;
		if ( terminals ) {
			Strings result;
			data.Terminals(id, seen, result);
			std::cout << "TERMINAL_ANCESTORS count=" << result.size() << '\n';
			for ( const auto& terminal : result ) {
				const auto& entry = data.people.at(terminal);
				std::cout << terminal << " name=" << entry.name;
				bool first = true;
				for ( const auto& event : entry.events ) if ( !event.second.empty() ) {
					std::cout << (first ? " events=" : ", ") << event.first << ' ' << event.second;
					first = false;
				}
				std::cout << '\n';
			}
		} else data.Tree(id, 0, seen, std::cout);
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write ancestry output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
