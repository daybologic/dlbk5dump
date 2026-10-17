#include "btrieve.hpp"

#include <cstdint>

namespace {
const std::size_t PAGE_SIZE = 1024U;

std::uint16_t LittleEndian16(const std::vector<unsigned char>& data,
	std::size_t offset)
{
	return static_cast<std::uint16_t>(data[offset])
		| (static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}
}

bool DecodeCompressedRecord(const std::vector<unsigned char>& data,
	std::size_t fixedAddress, std::vector<unsigned char>& expanded)
{
	/* DUMPDATA temporarily restores a carried byte in the fragment buffer
	 * when a run count straddles fragments.  Keep that mutation local. */
	std::vector<unsigned char> work(data);
	std::uint16_t page = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(work[fixedAddress]) << 8U)
		| work[fixedAddress + 1U]);
	unsigned int fragment = work[fixedAddress + 3U];
	if ( work[fixedAddress + 4U] == 0U ) return false;
	int count = 0;
	bool copyState = true;
	bool countSpans = false;
	unsigned char saved = 0U;
	for ( unsigned int chain = 0U; chain < 256U; ++chain ) {
		if ( page >= work.size() / PAGE_SIZE || fragment > 254U ) return false;
		const std::size_t pageBegin = static_cast<std::size_t>(page) * PAGE_SIZE;
		const std::size_t tableIndex = ((PAGE_SIZE - 1U) / 2U) - fragment;
		const std::size_t tableOffset = pageBegin + tableIndex * 2U;
		const std::uint16_t currentRaw = LittleEndian16(work, tableOffset);
		std::size_t previousIndex = tableIndex;
		while ( previousIndex > 0U
			&& LittleEndian16(work, pageBegin + (previousIndex - 1U) * 2U)
				== 0xffffU ) previousIndex--;
		if ( previousIndex == 0U ) return false;
		const std::uint16_t current = currentRaw & 0x7fffU;
		const std::uint16_t previous = LittleEndian16(work,
			pageBegin + (previousIndex - 1U) * 2U) & 0x7fffU;
		if ( previous < current ) return false;
		std::size_t valueOffset = pageBegin + current;
		std::size_t valueLength = previous - current;
		std::uint16_t nextPage = 0xffffU;
		unsigned int nextFragment = 0xffU;
		if ( currentRaw & 0x8000U ) {
			if ( valueLength < 4U ) return false;
			nextPage = static_cast<std::uint16_t>(
				(static_cast<std::uint16_t>(work[valueOffset]) << 8U)
				| work[valueOffset + 1U]);
			nextFragment = work[valueOffset + 3U];
			valueOffset += 4U;
			valueLength -= 4U;
		}
		if ( countSpans ) {
			if ( valueOffset == pageBegin ) return false;
			valueOffset--;
			valueLength++;
			/* The source algorithm restores the byte preceding the fragment
			 * so a two-byte repeat count can cross a page boundary. */
			work[valueOffset] = saved;
			countSpans = false;
		}
		std::size_t i = 0U;
		while ( i < valueLength ) {
			if ( count < 1 ) {
				/* A fragment holding only a continuation pointer and one
				 * data byte carries just the low byte of the next count;
				 * the high byte starts the following fragment.  BKMARR
				 * family 128 and four BKPERSON records use this layout. */
				if ( i + 1U == valueLength ) {
					saved = work[valueOffset + i++];
					countSpans = true;
					break;
				}
				count = static_cast<int>(LittleEndian16(work, valueOffset + i));
				i += 2U;
			}
			if ( i >= valueLength ) break;
			if ( copyState ) {
				while ( count-- > 0 ) {
					expanded.push_back(work[valueOffset + i++]);
					if ( expanded.size() > 4096U ) return false;
					if ( i == valueLength && count > 0 ) break;
				}
				if ( count < 1 ) copyState = false;
			} else {
				const unsigned char repeated = work[valueOffset + i];
				while ( count-- > 0 ) {
					expanded.push_back(repeated);
					if ( expanded.size() > 4096U ) return false;
				}
				i++;
				copyState = true;
			}
			if ( i == valueLength - 1U ) {
				saved = work[valueOffset + i++];
				countSpans = true;
				break;
			}
		}
		if ( nextPage == 0xffffU ) return true;
		page = nextPage;
		fragment = nextFragment;
	}
	return false;
}
