#ifndef INC_MARRIAGE_H
#define INC_MARRIAGE_H

#include <cstdint>
//---------------------------------------------------------------------------
class TReadMarriages {
public:
	TReadMarriages *Load(void);
	int Dump(void) const;
	int DumpFamilies(void) const;
	int TraceFamily(std::uint32_t family) const;
};
//---------------------------------------------------------------------------
#endif //!INC_MARRIAGE_H
