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

#include "gedcom_reader.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace GedcomInput {
std::vector<std::string> ReadLines(const std::string& path, bool replaceInvalid)
{
	std::ifstream input(path, std::ios::binary);
	if ( !input ) throw std::runtime_error("cannot open " + path);
	const std::string bytes((std::istreambuf_iterator<char>(input)), {});
	if ( input.bad() ) throw std::runtime_error("cannot read " + path);
	std::string text;
	for ( std::size_t i = 0; i < bytes.size(); ) {
		const unsigned char first = bytes[i];
		std::size_t length = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
			: first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
		std::size_t consumed = 1;
		if ( length > 1 ) {
			for ( ; consumed < length && i + consumed < bytes.size(); ++consumed ) {
				const unsigned char next = bytes[i + consumed];
				if ( next < 0x80 || next > 0xbf ) break;
				if ( consumed == 1 && ((first == 0xe0 && next < 0xa0)
					|| (first == 0xed && next >= 0xa0) || (first == 0xf0 && next < 0x90)
					|| (first == 0xf4 && next >= 0x90)) ) break;
			}
		}
		if ( !length || consumed != length ) {
			if ( !replaceInvalid ) throw std::runtime_error("GEDCOM input is not valid UTF-8");
			text += u8"\ufffd";
			i += consumed;
		} else {
			text.append(bytes, i, length);
			i += length;
		}
	}
	std::vector<std::string> lines;
	std::string line;
	for ( std::size_t i = 0; i < text.size(); ++i ) {
		if ( text[i] == '\r' || text[i] == '\n' ) {
			lines.push_back(line);
			line.clear();
			if ( text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n' ) ++i;
		} else line += text[i];
	}
	lines.push_back(line);
	return lines;
}
}
