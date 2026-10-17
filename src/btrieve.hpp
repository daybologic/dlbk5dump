#ifndef INC_BTRIEVE_H
#define INC_BTRIEVE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

/* Expand the compressed Btrieve record whose five-byte fixed pointer starts
 * at fixedAddress.  Returns false if the fragment chain cannot be followed. */
bool DecodeCompressedRecord(const std::vector<unsigned char>& data,
	std::size_t fixedAddress, std::vector<unsigned char>& expanded);

struct TBtrieveRecord {
	std::size_t address;
	std::vector<unsigned char> record;
};

struct TBtrieveScan {
	unsigned int headerRecords;
	unsigned int slots;
	unsigned int failures;
	unsigned int duplicates;
	std::map<std::uint32_t, TBtrieveRecord> records;
};

/* Expand every record in a compressed BK5 Btrieve file whose records start
 * with a big-endian record number.  Data pages are those whose 16-bit word
 * at offset 4 has its top bit set; their fixed slots, of the physical record
 * length in the file control record, start at offset 6.  A record is kept
 * only if it expands to exactly recordLength bytes, and it is keyed by the
 * number inside it, never by slot position. */
void ScanCompressedRecords(const std::vector<unsigned char>& data,
	std::size_t recordLength, TBtrieveScan& scan);

#endif //!INC_BTRIEVE_H
