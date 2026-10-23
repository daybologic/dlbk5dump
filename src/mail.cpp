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

#include "mail.hpp"

#include "text.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
const char *const MAIL_FILE = "BK5/BKMAIL.DT5";
const std::size_t MINIMUM_TEXT_LENGTH = 3U;
}

TMailTextRuns TReadMail::Load() const
{
	std::ifstream file(MAIL_FILE, std::ios::binary);
	if ( !file ) throw std::runtime_error(std::string("cannot open ") + MAIL_FILE);
	const std::string data((std::istreambuf_iterator<char>(file)), {});
	if ( file.bad() ) throw std::runtime_error(std::string("cannot read ") + MAIL_FILE);

	TMailTextRuns result;
	std::size_t first = 0;
	bool inRun = false;
	// Match the reference's printable byte range, not a locale-dependent
	// character classification. High CP437 bytes also delimit text runs.
	// The extra iteration flushes a run at EOF without changing the input.
	for ( std::size_t offset = 0; offset <= data.size(); ++offset ) {
		const unsigned char ch = offset < data.size()
			? static_cast<unsigned char>(data[offset]) : 0;
		const bool printable = ch >= 32 && ch < 127;
		if ( printable && !inRun ) {
			first = offset;
			inRun = true;
		} else if ( !printable && inRun ) {
			// Every accepted byte is ASCII, so CP437 decoding is identity.
			const std::string text = BK5Text::Strip(data.substr(first, offset - first));
			if ( text.size() >= MINIMUM_TEXT_LENGTH )
				result.push_back({first, offset, text});
			inRun = false;
		}
	}
	return result;
}

int TReadMail::Dump() const
{
	try {
		const TMailTextRuns runs = Load();
		for ( const auto& run : runs )
			std::cout << "BKMAIL_TEXT offset=0x" << std::hex << run.end
				<< std::dec << " text=" << BK5Text::PythonRepr(run.text) << '\n';
		std::cout << "BKMAIL_TEXT_RUNS count=" << runs.size() << '\n';
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write BKMAIL output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
