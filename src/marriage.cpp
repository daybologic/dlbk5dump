#include <cstring>
#include <cstdlib>
#include <map>
#include <fstream>
#include <iostream>

#include "marriage.hpp"

using namespace std;
//---------------------------------------------------------------------------
TReadMarriages *TReadMarriages::Load()
{
	unsigned int marriageCount = 0;
	off_t record_offset = 0xc00; // Start of first marriage
	struct rawMarriage {
		char unknown0[12];
		unsigned long int husband_id;
		unsigned long int wife_id;
		char date[11];
		char unknown1[33];
		char ceremony;
		char status;
	} rawMarr;

	ifstream marrFile("BK5/BKMARR.DT5", ios::in | ios::binary);
	if ( !marrFile.is_open() ) {
		cout << "Fail" << endl;
		abort;
	}
	do {
		marrFile.seekg(record_offset, ios::beg);
		marrFile.read((char*)&rawMarr, sizeof(rawMarr));

		char dateCopy[12];
		memcpy(dateCopy, rawMarr.date, sizeof(rawMarr.date));
		dateCopy[11] = '\0';
		cout << "Husband " << rawMarr.husband_id << " was married to wife " << rawMarr.wife_id
			<< " on " << dateCopy << ", ceremony: " << rawMarr.ceremony
			<< " Status: " << rawMarr.status << endl;

		marriageCount++;
		record_offset += 0x54;
	} while ( marriageCount < 3 );
	marrFile.close();
	return this;
}
//---------------------------------------------------------------------------
