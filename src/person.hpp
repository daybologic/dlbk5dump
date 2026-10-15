#ifndef INC_PERSON_H
#define INC_PERSON_H

#include <cstdint>
#include <string>

class TReadPersons {
public:
	int Dump() const;
	int DumpPage(unsigned int page) const;
	int DumpIndexPage(unsigned int page) const;
	int DumpIndex() const;
	int DumpRelations() const;
	int TraceAddress(std::uint32_t address) const;
	int Find(const std::string& query) const;
};

#endif //!INC_PERSON_H
