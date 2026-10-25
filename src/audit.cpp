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

#include "audit.hpp"

#include "diagnostic.hpp"
#include "mail.hpp"
#include "person.hpp"
#include "text.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Strings = std::vector<std::string>;
using StringSet = std::set<std::string>;

bool StartsWith(const std::string& value, const std::string& prefix)
{
	return value.rfind(prefix, 0) == 0;
}

bool ValidUtf8(const std::string& text)
{
	for ( std::size_t i = 0; i < text.size(); ) {
		const unsigned char first = text[i++];
		if ( first < 128 ) continue;
		unsigned int remaining;
		std::uint32_t value, minimum;
		if ( first >= 0xc2 && first <= 0xdf ) {
			remaining = 1; value = first & 0x1f; minimum = 0x80;
		} else if ( first >= 0xe0 && first <= 0xef ) {
			remaining = 2; value = first & 0x0f; minimum = 0x800;
		} else if ( first >= 0xf0 && first <= 0xf4 ) {
			remaining = 3; value = first & 0x07; minimum = 0x10000;
		} else return false;
		if ( remaining > text.size() - i ) return false;
		while ( remaining-- ) {
			const unsigned char next = text[i++];
			if ( (next & 0xc0) != 0x80 ) return false;
			value = (value << 6) | (next & 0x3f);
		}
		if ( value < minimum || value > 0x10ffff
			|| (value >= 0xd800 && value <= 0xdfff) ) return false;
	}
	return true;
}

Strings CommaFields(const std::string& value)
{
	Strings result;
	std::size_t start = 0;
	for ( std::size_t end = 0; end <= value.size(); ++end ) {
		if ( end == value.size() || value[end] == ',' ) {
			if ( end > start ) result.push_back(value.substr(start, end - start));
			start = end + 1;
		}
	}
	return result;
}

std::string FamilyTag(const std::string& number)
{
	const std::uint64_t id = std::stoull(number, nullptr, 16);
	if ( !id ) return "";
	std::ostringstream tag;
	tag << "@F" << std::hex << id << '@';
	return tag.str();
}

void EmitSet(std::ostream& report, const char *label, const StringSet& values)
{
	if ( values.empty() ) return;
	report << label << ' ';
	bool first = true;
	for ( const std::string& value : values ) {
		if ( !first ) report << ' ';
		report << value;
		first = false;
	}
	report << '\n';
}

int RunAudit(const std::string& gedcom)
{
	TReadPersons persons;
	const Strings personDump = BK5Text::TextLines(CaptureDiagnosticOutput(
		[&]() { return persons.DumpIdRecords(); }));
	const Strings rawDump = BK5Text::TextLines(CaptureDiagnosticOutput(
		[&]() { return persons.DumpRecords(); }));
	std::size_t diagnosticRecords = 0, named = 0, failed = 0, unnamed = 0;
	std::size_t dateFields = 0, rawDateFields = 0;
	StringSet familyRefs;
	const std::regex personDates(" dates=(.*?)(?: source=|$)");
	const std::regex personFamilies(" families=([^ ]*) ");
	const std::regex rawDates(" dates=(.*)$");
	for ( const std::string& line : personDump ) {
		if ( !StartsWith(line, "PERSON_ID_RECORD ") ) continue;
		++diagnosticRecords;
		const bool decoded = line.find("status=decoded") != std::string::npos;
		const bool emptyName = line.find("name=\"\"") != std::string::npos;
		if ( decoded && !emptyName ) ++named;
		if ( decoded && emptyName ) ++unnamed;
		if ( line.find("status=compressed-decode-failed") != std::string::npos
			|| line.find("status=address-out-of-range") != std::string::npos
			|| line.find("status=deleted-or-uncompressed") != std::string::npos ) ++failed;
		std::smatch match;
		if ( std::regex_search(line, match, personDates) )
			dateFields += CommaFields(match[1]).size();
		if ( std::regex_search(line, match, personFamilies) )
			for ( const std::string& value : CommaFields(match[1]) ) {
				const std::string tag = FamilyTag(value);
				if ( !tag.empty() ) familyRefs.insert(tag);
			}
	}
	for ( const std::string& line : rawDump ) {
		if ( !StartsWith(line, "PERSON_RECORD ") ) continue;
		std::smatch match;
		if ( std::regex_search(line, match, rawDates) )
			rawDateFields += CommaFields(match[1]).size();
	}

	std::size_t individuals = 0, families = 0, dates = 0;
	std::size_t rawDateNotes = 0, mailNotes = 0, altkeyNotes = 0, unresolved = 0;
	StringSet individualIds, familyIds, refs;
	const std::regex individualRe("^0 @I[^ ]+@ INDI$");
	const std::regex familyRe("^0 @F[^ ]+@ FAM$");
	const std::regex referenceRe("^1 (?:HUSB|WIFE|CHIL|FAMC|FAMS) @[^ ]+@$");
	for ( const std::string& line : BK5Text::TextLines(gedcom) ) {
		if ( std::regex_match(line, individualRe) ) {
			++individuals;
			individualIds.insert(BK5Text::Words(line).at(1));
		}
		if ( std::regex_match(line, familyRe) ) {
			++families;
			familyIds.insert(BK5Text::Words(line).at(1));
		}
		if ( std::regex_match(line, referenceRe) ) refs.insert(BK5Text::Words(line).back());
		if ( StartsWith(line, "2 DATE ") ) ++dates;
		if ( StartsWith(line, "1 NOTE BK5 raw date at ") ) ++rawDateNotes;
		if ( StartsWith(line, "1 CONT BKMAIL offset ") || StartsWith(line, "2 CONT BKMAIL offset ") ) ++mailNotes;
		if ( StartsWith(line, "1 CONT ALTKEYS slot ") || StartsWith(line, "2 CONT ALTKEYS slot ") ) ++altkeyNotes;
		if ( line.find("[not decoded]") != std::string::npos ) ++unresolved;
	}
	const std::size_t mailRuns = TReadMail().Load().size();
	StringSet missingFamilies, dangling;
	for ( const std::string& ref : familyRefs )
		if ( !familyIds.count(ref) ) missingFamilies.insert(ref);
	for ( const std::string& ref : refs )
		if ( !individualIds.count(ref) && !familyIds.count(ref) ) dangling.insert(ref);

	std::ostringstream report;
	report << "AUDIT diagnostic_records=" << diagnosticRecords << '\n'
		<< "AUDIT named_decoded=" << named << " undecodable=" << failed
		<< " unnamed_decoded=" << unnamed << '\n'
		<< "AUDIT diagnostic_date_fields=" << dateFields << " gedcom_dates=" << dates << '\n'
		<< "AUDIT raw_date_fields=" << rawDateFields << " raw_date_notes=" << rawDateNotes << '\n'
		<< "AUDIT mail_runs=" << mailRuns << " mail_notes=" << mailNotes << " altkey_notes=" << altkeyNotes << '\n'
		<< "AUDIT unresolved_lookup_notes=" << unresolved << '\n'
		<< "AUDIT individuals=" << individuals << " families=" << families
		<< " duplicate_individuals=" << individuals - individualIds.size()
		<< " duplicate_families=" << families - familyIds.size() << '\n'
		<< "AUDIT family_refs=" << familyRefs.size() << " missing_families=" << missingFamilies.size()
		<< " dangling_refs=" << dangling.size() << '\n';
	EmitSet(report, "AUDIT_MISSING_FAMILIES", missingFamilies);
	EmitSet(report, "AUDIT_DANGLING_REFS", dangling);
	std::cout << report.str();
	std::cout.flush();
	if ( !std::cout ) throw std::runtime_error("cannot write audit output");

	// Keep the reference's exact failure policy. Duplicate IDs, ordinary date
	// counts and decoder failures are reported, but do not alone change status.
	// The one-ALTKEYS-note requirement is also a retained corpus assumption.
	return !missingFamilies.empty() || !dangling.empty()
		|| rawDateFields != rawDateNotes || mailRuns != mailNotes
		|| altkeyNotes != 1 || unresolved ? 1 : 0;
}
}

int AuditGedcom(std::istream& input)
{
	try {
		const std::string gedcom((std::istreambuf_iterator<char>(input)), {});
		if ( input.bad() ) throw std::runtime_error("cannot read GEDCOM input");
		if ( !ValidUtf8(gedcom) ) throw std::runtime_error("GEDCOM input is not valid UTF-8");
		return RunAudit(gedcom);
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}

int AuditGedcomFile(const std::string& path)
{
	std::ifstream input(path, std::ios::binary);
	if ( !input ) {
		std::cerr << "dlbk5dump: cannot open " << path << '\n';
		return 1;
	}
	return AuditGedcom(input);
}
