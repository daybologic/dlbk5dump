#include <cstdio>
#include <iostream>
#include <dirent.h>
#include <cstring>
//#include <map>

#include "marriage.hpp"

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
int main()
{
	TReadMarriages marriages;

	if ( !CheckFiles() ) {
		puts("Error: Files don\'t exist");
		return 1;
	}
	marriages.Load();
	puts("All files exist.");
	return 0;
}
//---------------------------------------------------------------------------
