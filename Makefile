CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic

TARGET  := dlbk5dump
SOURCES := src/dlbk5dump.cpp src/marriage.cpp src/person.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
