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

#include "text.hpp"

#include <iomanip>
#include <sstream>
#include <vector>

namespace BK5Text {
namespace {
void AppendUtf8(std::string& result, char32_t ch)
{
	if ( ch < 0x80 ) result += static_cast<char>(ch);
	else if ( ch < 0x800 ) {
		result += static_cast<char>(0xc0 | (ch >> 6));
		result += static_cast<char>(0x80 | (ch & 0x3f));
	} else {
		result += static_cast<char>(0xe0 | (ch >> 12));
		result += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
		result += static_cast<char>(0x80 | (ch & 0x3f));
	}
}

}

std::string Cp437(const std::string& value)
{
	static const std::u32string high =
		U"ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ"
		U"áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
		U"└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
		U"αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";
	std::string result;
	for ( unsigned char ch : value ) AppendUtf8(result, ch < 128 ? ch : high[ch - 128]);
	return result;
}

std::size_t WhitespaceWidth(const std::string& value, std::size_t offset)
{
	if ( offset >= value.size() ) return 0;
	const unsigned char ch = value[offset];
	if ( ch < 128 ) return ch == ' ' || (ch >= 9 && ch <= 13)
		|| (ch >= 28 && ch <= 31) ? 1 : 0;
	// Python str.strip/split use Unicode whitespace, including the CP437
	// non-breaking space. Do not depend on the process's C locale.
	static const std::vector<std::string> spaces = {u8"\u0085", u8"\u00a0", u8"\u1680",
		u8"\u2000", u8"\u2001", u8"\u2002", u8"\u2003", u8"\u2004",
		u8"\u2005", u8"\u2006", u8"\u2007", u8"\u2008", u8"\u2009",
		u8"\u200a", u8"\u2028", u8"\u2029", u8"\u202f", u8"\u205f", u8"\u3000"};
	for ( const std::string& space : spaces )
		if ( value.compare(offset, space.size(), space) == 0 ) return space.size();
	return 0;
}

std::string RStrip(std::string value)
{
	std::size_t end = 0;
	for ( std::size_t offset = 0; offset < value.size(); ) {
		const std::size_t width = WhitespaceWidth(value, offset);
		if ( width ) offset += width;
		else end = ++offset;
	}
	value.resize(end);
	return value;
}

std::string Strip(std::string value)
{
	value = RStrip(value);
	std::size_t start = 0;
	while ( const std::size_t width = WhitespaceWidth(value, start) ) start += width;
	return value.substr(start);
}

std::vector<std::string> Words(const std::string& value)
{
	std::vector<std::string> result;
	std::size_t start = 0;
	for ( std::size_t offset = 0; offset <= value.size(); ) {
		const std::size_t width = WhitespaceWidth(value, offset);
		if ( width || offset == value.size() ) {
			if ( offset > start ) result.push_back(value.substr(start, offset - start));
			if ( offset == value.size() ) break;
			offset += width;
			start = offset;
		} else ++offset;
	}
	return result;
}

std::vector<std::string> TextLines(const std::string& value)
{
	std::vector<std::string> result;
	std::size_t start = 0;
	for ( std::size_t offset = 0; offset < value.size(); ) {
		const unsigned char ch = value[offset];
		std::size_t width = 0;
		if ( (ch >= 10 && ch <= 13) || (ch >= 28 && ch <= 30) ) width = 1;
		else if ( value.compare(offset, 2, u8"\u0085") == 0 ) width = 2;
		else if ( value.compare(offset, 3, u8"\u2028") == 0
			|| value.compare(offset, 3, u8"\u2029") == 0 ) width = 3;
		if ( !width ) { ++offset; continue; }
		result.push_back(value.substr(start, offset - start));
		if ( ch == '\r' && offset + 1 < value.size() && value[offset + 1] == '\n' ) ++width;
		offset += width;
		start = offset;
	}
	if ( start < value.size() ) result.push_back(value.substr(start));
	if ( result.empty() ) result.push_back("");
	return result;
}

std::string PythonRepr(const std::string& value)
{
	// Match Python's repr for CP437 text and citation values.
	const char quote = value.find('\'') != std::string::npos
		&& value.find('"') == std::string::npos ? '"' : '\'';
	std::string result(1, quote);
	for ( std::size_t i = 0; i < value.size(); ++i ) {
		const unsigned char ch = value[i];
		// CP437's non-breaking space is escaped by Python even within text.
		if ( value.compare(i, 2, u8"\u00a0") == 0 ) {
			result += "\\xa0";
			++i;
		} else if ( ch == static_cast<unsigned char>(quote) || ch == '\\' ) {
			result += '\\';
			result += static_cast<char>(ch);
		} else if ( ch == '\n' ) result += "\\n";
		else if ( ch == '\r' ) result += "\\r";
		else if ( ch == '\t' ) result += "\\t";
		else if ( ch < 32 || ch == 127 ) {
			std::ostringstream escaped;
			escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(ch);
			result += escaped.str();
		} else result += static_cast<char>(ch);
	}
	return result + quote;
}

}
