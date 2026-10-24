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

#include "diagnostic.hpp"

#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>

// Keep the established dump decoders as the single decoding path. Capture
// their diagnostic records in memory, using the same contract as the Python
// reference, without starting subprocesses or duplicating binary layouts.
std::string CaptureDiagnosticOutput(const std::function<int()>& dump)
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
