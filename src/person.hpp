#ifndef INC_PERSON_H
#define INC_PERSON_H

#include <cstdint>
#include <string>

class TReadPersons {
public:
	int Dump() const;
	int DumpRecords() const;
	int DumpIdIndex() const;
	int DumpIdRecords() const;
	int TracePersonId(std::uint32_t id) const;
	int TraceFixedAddress(std::uint32_t address) const;
	int DumpPage(unsigned int page) const;
	int DumpIndexPage(unsigned int page) const;
	int DumpIndex() const;
	int DumpRelations() const;
	int DumpFamily(std::uint32_t family) const;
	int TraceAddress(std::uint32_t address) const;
	int Find(const std::string& query) const;
};

#endif //!INC_PERSON_H
