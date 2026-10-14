#ifndef INC_PERSON_H
#define INC_PERSON_H

#include <string>

class TReadPersons {
public:
	int Dump() const;
	int DumpPage(unsigned int page) const;
	int DumpIndexPage(unsigned int page) const;
	int Find(const std::string& query) const;
};

#endif //!INC_PERSON_H
