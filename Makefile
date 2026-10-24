# Family History Database Recovery Project (dlbk5dump)
# Copyright (c) 2012-2026, Rev. Duncan Ross Palmer (M6KVM, 2E0EOL),
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#     * Neither the name of the Daybo Logic nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic

TARGET  := dlbk5dump
SOURCES := src/altkeys.cpp src/ancestry.cpp src/audit.cpp src/btrieve.cpp \
	src/diagnostic.cpp src/dlbk5dump.cpp src/gedcom.cpp src/gedcom_check.cpp \
	src/gedcom_places.cpp src/gedcom_reader.cpp src/gedcom_roles.cpp \
	src/lookup.cpp src/mail.cpp src/marriage.cpp src/options.cpp \
	src/person.cpp src/text.cpp src/unicode.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean gedcom gedcom-file gedcom-clean-file audit check-gedcom check-altkeys check-options check-mail check-audit check-ancestry

all: $(TARGET)

gedcom: $(TARGET)
	@./dlbk5dump --gedcom

gedcom-file: $(TARGET)
	@set -e; gedcom_tmp=$$(mktemp ./indexed.ged.XXXXXX); \
		trap 'rm -f "$$gedcom_tmp"' EXIT; trap 'exit 1' HUP INT TERM; \
		./dlbk5dump --gedcom > "$$gedcom_tmp"; \
		mv -f "$$gedcom_tmp" indexed.ged

gedcom-clean-file: $(TARGET)
	@set -e; gedcom_tmp=$$(mktemp ./indexed-clean.ged.XXXXXX); \
		trap 'rm -f "$$gedcom_tmp"' EXIT; trap 'exit 1' HUP INT TERM; \
		./dlbk5dump --gedcom --clean > "$$gedcom_tmp"; \
		mv -f "$$gedcom_tmp" indexed-clean.ged

audit: $(TARGET)
	@./dlbk5dump --gedcom | ./dlbk5dump --audit

check-audit: $(TARGET)
	@sh tools/check-audit.sh

check-ancestry: $(TARGET)
	@sh tools/check-ancestry.sh

.PHONY: check-structure
check-structure: $(TARGET)
	@sh tools/check-structure.sh

.PHONY: check-roles
check-roles: $(TARGET)
	@sh tools/check-roles.sh

.PHONY: check-places check-all
check-places: $(TARGET)
	@sh tools/check-places.sh

check-all: check-gedcom check-altkeys check-options check-mail check-audit check-ancestry check-structure check-roles check-places

check-gedcom: $(TARGET)
	@sh tools/check-gedcom.sh

check-altkeys: $(TARGET)
	@sh tools/check-altkeys.sh

check-options: $(TARGET)
	@sh tools/check-options.sh

check-mail: $(TARGET)
	@sh tools/check-mail.sh

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
