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

#include "gedcom_places.hpp"

#include "gedcom_reader.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int CheckGedcomPlaces(const std::string& path)
{
	try {
		const auto lines = GedcomInput::ReadLines(path);
		std::size_t total = 0, placeOnly = 0;
		for ( std::size_t i = 0; i < lines.size(); ++i ) {
			const auto& line = lines[i];
			if ( line != "1 BIRT" && line != "1 BAPM" && line != "1 DEAT" && line != "1 BURI" ) continue;
			bool place = false, date = false;
			// Preserve the reference's contiguous level-2 scan and four-byte
			// tag prefix. Deeper nesting ends the scan, even if more level-2
			// lines follow; do not silently broaden the reference's counts.
			for ( std::size_t j = i + 1; j < lines.size() && lines[j].rfind("2 ", 0) == 0; ++j ) {
				const auto tag = lines[j].substr(2, 4);
				if ( tag == "PLAC" ) place = true;
				if ( tag == "DATE" ) date = true;
			}
			if ( place ) ++total;
			if ( place && !date ) ++placeOnly;
		}
		std::cout << "events with PLAC " << total << " place-only " << placeOnly << '\n';
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write GEDCOM place output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
