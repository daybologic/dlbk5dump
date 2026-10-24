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

#ifndef INC_TEXT_H
#define INC_TEXT_H

#include <cstddef>
#include <string>
#include <vector>

namespace BK5Text {
// Match the reference's CP437 decoding, Unicode whitespace, and quoted text.
std::string Cp437(const std::string& value);
std::size_t WhitespaceWidth(const std::string& value, std::size_t offset);
std::string RStrip(std::string value);
std::string Strip(std::string value);
std::vector<std::string> Words(const std::string& value);
// Split Unicode line boundaries, retaining one empty field for empty notes.
std::vector<std::string> TextLines(const std::string& value);
std::string PythonRepr(const std::string& value);
}

#endif //!INC_TEXT_H
