#ifndef INC_BTRIEVE_H
#define INC_BTRIEVE_H

#include <cstddef>
#include <vector>

/* Expand the compressed Btrieve record whose five-byte fixed pointer starts
 * at fixedAddress.  Returns false if the fragment chain cannot be followed. */
bool DecodeCompressedRecord(const std::vector<unsigned char>& data,
	std::size_t fixedAddress, std::vector<unsigned char>& expanded);

#endif //!INC_BTRIEVE_H
