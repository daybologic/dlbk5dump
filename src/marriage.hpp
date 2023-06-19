#include <cstdio>
#include <map>

#ifndef INC_MARRIAGE_H
#define INC_MARRIAGE_H
//---------------------------------------------------------------------------
struct Marriage {
	unsigned long int HusbandId;
	unsigned long int WifeI;
};

class TReadMarriages {
private:
	std::map<unsigned short int, struct Marriage *> detail;
public:
	TReadMarriages *Load(void);
};
//---------------------------------------------------------------------------
#endif //!INC_MARRIAGE_H
