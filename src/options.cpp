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

#include "options.hpp"

#include "text.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {
using Options = std::map<std::string, std::string>;
const char *const ACTIVE_FILE = "BK5/BKOPTION.DT5";
const char *const BACKUP_FILE = "BK5/BKOPTION.BAK";

Options ReadOptions(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	if ( !file ) throw std::runtime_error(std::string("cannot open ") + path);
	const std::string data((std::istreambuf_iterator<char>(file)), {});
	if ( file.bad() ) throw std::runtime_error(std::string("cannot read ") + path);

	Options result;
	std::size_t first = 0;
	// Match bytes.splitlines(): CR, LF and CRLF delimit byte records.
	// Other controls stay in the field; trimming happens after CP437 decoding.
	for ( std::size_t last = 0; last <= data.size(); ++last ) {
		if ( last < data.size() && data[last] != '\r' && data[last] != '\n' )
			continue;
		const std::string line = BK5Text::RStrip(
			BK5Text::Cp437(data.substr(first, last - first)));
		const std::size_t equals = line.find('=');
		if ( equals != std::string::npos )
			result[line.substr(0, equals)] = line.substr(equals + 1);
		if ( last < data.size() && data[last] == '\r'
			&& last + 1 < data.size() && data[last + 1] == '\n' ) ++last;
		first = last + 1;
	}
	return result;
}
}

int TReadOptions::Dump() const
{
	try {
		// Read both inputs before printing so a missing backup cannot leave
		// partial comparison output.
		const Options active = ReadOptions(ACTIVE_FILE);
		const Options backup = ReadOptions(BACKUP_FILE);
		std::set<std::string> keys;
		for ( const auto& entry : active ) keys.insert(entry.first);
		for ( const auto& entry : backup ) keys.insert(entry.first);

		std::size_t differences = 0;
		for ( const std::string& key : keys ) {
			const auto currentEntry = active.find(key);
			const auto savedEntry = backup.find(key);
			const std::string current = currentEntry == active.end()
				? "<missing>" : currentEntry->second;
			const std::string saved = savedEntry == backup.end()
				? "<missing>" : savedEntry->second;
			// Preserve the reference's literal <missing> comparison.
			if ( current == saved ) continue;
			std::cout << "BKOPTION_DIFFERENCE key=" << BK5Text::PythonRepr(key)
				<< " active=" << BK5Text::PythonRepr(current)
				<< " backup=" << BK5Text::PythonRepr(saved) << '\n';
			++differences;
		}
		std::cout << "BKOPTION_SUMMARY active_keys=" << active.size()
			<< " backup_keys=" << backup.size()
			<< " differences=" << differences << '\n';
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write BKOPTION output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
