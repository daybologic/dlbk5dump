#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <string>
//#include <map>

#include "marriage.hpp"
#include "person.hpp"

using namespace std;
//---------------------------------------------------------------------------
static bool CheckFiles(void);
//---------------------------------------------------------------------------
static bool CheckFiles()
{
	const char *const files[] = {
		"ALTKEYS.DT5",
		"BKLOCATE.DT5",
		"BKMAIL.DT5",
		"BKMARR.DT5",
		"BKOPTION.DT5",
		"BKPERSON.DT5",
		"BKSOURCE.DT5",
		"REPORTS.DT5",
		NULL
	};
	//map<string, bool> filesSeen;
	unsigned short int seenCount = 0U;
	DIR *bk5Dir = opendir("BK5");
	if ( bk5Dir ) {
		struct dirent *dbFnEnt;
		while ( dbFnEnt = readdir(bk5Dir) ) {
			const char *knownPtr;
			unsigned int ki = 0U;
			while ( knownPtr = files[ki++] ) {
				if ( strcmp(knownPtr, dbFnEnt->d_name) == 0 ) {
					fprintf(stderr, "DEBUG: Seen %s\n", dbFnEnt->d_name);
					seenCount++;
					break;
				}
			}
		}
		closedir(bk5Dir);
	}
	if ( seenCount == sizeof(files)/sizeof(files[0])-1 ) return true;
	return false;
}
//---------------------------------------------------------------------------
int main(int argc, char **argv)
{
	TReadMarriages marriages;
	TReadPersons persons;

	if ( !CheckFiles() ) {
		puts("Error: Files don\'t exist");
		return 1;
	}
	if ( argc == 3 && strcmp(argv[1], "--person") == 0 ) {
		return persons.Find(argv[2]);
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-persons") == 0 ) {
		return persons.Dump();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-records") == 0 ) {
		return persons.DumpRecords();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-id-index") == 0 ) {
		return persons.DumpIdIndex();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-id-records") == 0 ) {
		return persons.DumpIdRecords();
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-id") == 0 ) {
		char *end = NULL;
		const unsigned long id = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || id > 0xffffffffUL ) {
			fprintf(stderr, "Error: id must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.TracePersonId(static_cast<std::uint32_t>(id));
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-fixed") == 0 ) {
		char *end = NULL;
		const unsigned long address = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || address > 0xffffffffUL ) {
			fprintf(stderr, "Error: fixed address must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.TraceFixedAddress(static_cast<std::uint32_t>(address));
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-records") == 0 ) {
		return persons.DumpRecords();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-id-index") == 0 ) {
		return persons.DumpIdIndex();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-id-records") == 0 ) {
		return persons.DumpIdRecords();
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-id") == 0 ) {
		char *end = NULL;
		const unsigned long id = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || id > 0xffffffffUL ) {
			fprintf(stderr, "Error: id must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.TracePersonId(static_cast<std::uint32_t>(id));
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-fixed") == 0 ) {
		char *end = NULL;
		const unsigned long address = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || address > 0xffffffffUL ) {
			fprintf(stderr, "Error: fixed address must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.TraceFixedAddress(static_cast<std::uint32_t>(address));
	}
	if ( argc == 3 && strcmp(argv[1], "--page") == 0 ) {
		char *end = NULL;
		const unsigned long page = strtoul(argv[2], &end, 10);
		if ( end == argv[2] || *end != '\0' || page > 0xffffffffUL ) {
			fprintf(stderr, "Error: page must be a non-negative integer\n");
			return 2;
		}
		return persons.DumpPage(static_cast<unsigned int>(page));
	}
	if ( argc == 3 && strcmp(argv[1], "--index-page") == 0 ) {
		char *end = NULL;
		const unsigned long page = strtoul(argv[2], &end, 10);
		if ( end == argv[2] || *end != '\0' || page > 0xffffffffUL ) {
			fprintf(stderr, "Error: page must be a non-negative integer\n");
			return 2;
		}
		return persons.DumpIndexPage(static_cast<unsigned int>(page));
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-index") == 0 ) {
		return persons.DumpIndex();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-relations") == 0 ) {
		return persons.DumpRelations();
	}
	if ( argc == 3 && strcmp(argv[1], "--family") == 0 ) {
		char *end = NULL;
		const unsigned long family = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || family > 0xffffffffUL ) {
			fprintf(stderr, "Error: family must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.DumpFamily(static_cast<std::uint32_t>(family));
	}
	if ( argc == 3 && strcmp(argv[1], "--family") == 0 ) {
		char *end = NULL;
		const unsigned long family = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || family > 0xffffffffUL ) {
			fprintf(stderr, "Error: family must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.DumpFamily(static_cast<std::uint32_t>(family));
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-marriages") == 0 ) {
		return marriages.Dump();
	}
	if ( argc == 2 && strcmp(argv[1], "--dump-families") == 0 ) {
		return marriages.DumpFamilies();
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-family") == 0 ) {
		char *end = NULL;
		const unsigned long family = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || family > 0xffffffffUL ) {
			fprintf(stderr, "Error: family must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return marriages.TraceFamily(static_cast<std::uint32_t>(family));
	}
	if ( argc == 3 && strcmp(argv[1], "--trace-address") == 0 ) {
		char *end = NULL;
		const unsigned long address = strtoul(argv[2], &end, 0);
		if ( end == argv[2] || *end != '\0' || address > 0xffffffffUL ) {
			fprintf(stderr, "Error: address must be a non-negative integer (decimal or 0x-prefixed hexadecimal)\n");
			return 2;
		}
		return persons.TraceAddress(static_cast<std::uint32_t>(address));
	}
	if ( argc != 1 ) {
		fprintf(stderr, "Usage: %s [--person TEXT|--dump-persons|--dump-records|--dump-id-index|--dump-id-records|--trace-id N|--trace-fixed ADDRESS|--page N|--index-page N|--dump-index|--dump-relations|--family N|--dump-marriages|--dump-families|--trace-family N|--trace-address N]\n", argv[0]);
		return 2;
	}
	marriages.Load();
	puts("All files exist.");
	return 0;
}
//---------------------------------------------------------------------------
