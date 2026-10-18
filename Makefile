CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic

TARGET  := dlbk5dump
SOURCES := src/btrieve.cpp src/dlbk5dump.cpp src/gedcom.cpp src/lookup.cpp src/marriage.cpp src/person.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean gedcom gedcom-file gedcom-clean-file audit check-gedcom

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
	@./dlbk5dump --gedcom | ./tools/bk5-audit.py

check-gedcom: $(TARGET)
	@sh tools/check-gedcom.sh

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
