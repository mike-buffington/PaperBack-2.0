#include "paperbak_block.h"
#include "paperbak_format.h"

using uchar = unsigned char;
using ushort = unsigned short;

int Decode8(uchar *data, int *eras_pos, int no_eras, int pad);
ushort Crc16(uchar *data, int length);

namespace paperbak {

static std::uint16_t ReadLE16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

int CorrectAndValidateBlock(std::uint8_t block[128]) {
    if (block == nullptr) return -1;

    // PaperBak's 128-byte codeword uses the original RS padding value 127.
    const int corrected = Decode8(block, nullptr, 0, 127);
    if (corrected < 0 || corrected > 16) return -1;

    // The stored CRC is little-endian and covers address + 90 data bytes.
    const std::uint16_t expected = static_cast<std::uint16_t>(Crc16(block, 94) ^ 0x55AA);
    if (ReadLE16(block + 94) != expected) return -1;

    return corrected;
}

} // namespace paperbak
