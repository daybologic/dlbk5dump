CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic

TARGET  := dlbk5dump
SOURCES := src/btrieve.cpp src/dlbk5dump.cpp src/lookup.cpp src/marriage.cpp src/person.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean gedcom gedcom-file gedcom-clean-file audit

all: $(TARGET)

gedcom: $(TARGET)
	@./tools/bk5-indexed-to-gedcom.py

gedcom-file: $(TARGET)
	@./tools/bk5-indexed-to-gedcom.py > indexed.ged

gedcom-clean-file: $(TARGET)
	@./tools/bk5-indexed-to-gedcom.py --clean > indexed-clean.ged

audit: $(TARGET)
	@./tools/bk5-indexed-to-gedcom.py | ./tools/bk5-audit.py

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
