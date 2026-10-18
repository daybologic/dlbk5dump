#ifndef INC_GEDCOM_H
#define INC_GEDCOM_H

// Export the same audit/clean GEDCOM as the retained Python reference.
// All decoding and formatting run in this process; no Python is required.
int ExportGedcom(bool clean = false);

#endif //!INC_GEDCOM_H
