// Portable validation and error-correction entry point for one PaperBak block.
// The 128-byte on-paper representation is preserved exactly.
#ifndef PAPERBAK_BLOCK_H
#define PAPERBAK_BLOCK_H

#include <cstdint>

namespace paperbak {

// Corrects a block in place using PaperBak's original Reed-Solomon code and
// verifies its CRC. Returns the number of corrected bytes (0..16), or -1 if
// error correction or CRC verification fails. The input must be exactly 128
// bytes. Both data blocks and superblocks use this same on-paper check.
int CorrectAndValidateBlock(std::uint8_t block[128]);

} // namespace paperbak
#endif
