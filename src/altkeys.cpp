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

#include "altkeys.hpp"

#include "text.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
const char *const ALTKEYS_FILE = "BK5/ALTKEYS.DT5";
const std::size_t SLOT_SIZE = 162U;
}

TAltKeys TReadAltKeys::Load() const
{
	std::ifstream file(ALTKEYS_FILE, std::ios::binary);
	if ( !file ) throw std::runtime_error(std::string("cannot open ") + ALTKEYS_FILE);
	const std::string data((std::istreambuf_iterator<char>(file)), {});
	if ( file.bad() ) throw std::runtime_error(std::string("cannot read ") + ALTKEYS_FILE);
	if ( data.size() % SLOT_SIZE )
		throw std::runtime_error("ALTKEYS.DT5 size is not a multiple of 162");

	TAltKeys result;
	result.totalSlots = data.size() / SLOT_SIZE;
	for ( std::size_t slot = 0; slot < result.totalSlots; ++slot ) {
		const std::string raw = data.substr(slot * SLOT_SIZE, SLOT_SIZE);
		const std::string text = BK5Text::Strip(
			BK5Text::Cp437(raw.substr(0, raw.find('<'))));
		if ( !text.empty() ) result.nonemptySlots.emplace_back(slot, text);
	}
	return result;
}

int TReadAltKeys::Dump() const
{
	try {
		const TAltKeys data = Load();
		for ( const auto& slot : data.nonemptySlots )
			std::cout << "ALTKEYS_SLOT slot=" << slot.first
				<< " text=" << BK5Text::PythonRepr(slot.second) << '\n';
		std::cout << "ALTKEYS_SLOTS total=" << data.totalSlots
			<< " nonempty=" << data.nonemptySlots.size() << '\n';
		std::cout.flush();
		if ( !std::cout ) throw std::runtime_error("cannot write ALTKEYS output");
		return 0;
	} catch ( const std::exception& error ) {
		std::cerr << "dlbk5dump: " << error.what() << '\n';
		return 1;
	}
}
