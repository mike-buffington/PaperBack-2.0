// Portable definitions of PaperBak's on-paper block format.
// Field sizes and offsets intentionally match the original Windows format.
// Keep these layouts unchanged: existing printed backups depend on them.
#ifndef PAPERBAK_FORMAT_H
#define PAPERBAK_FORMAT_H

#include <cstddef>
#include <cstdint>

namespace paperbak {

constexpr std::size_t kDotCount = 32;
constexpr std::size_t kDataBytes = 90;
constexpr std::size_t kBlockBytes = 128;
constexpr std::uint32_t kSuperblockAddress = 0xFFFFFFFFu;
constexpr std::uint8_t kCompressed = 0x01;
constexpr std::uint8_t kEncrypted = 0x02;

#pragma pack(push, 1)
struct DataBlock {
    std::uint32_t address;
    std::uint8_t data[kDataBytes];
    std::uint16_t crc;
    std::uint8_t ecc[kDotCount];
};

// The Windows FILETIME is two 32-bit words. Represent it as its exact 8 bytes
// here so the layout doesn't depend on the host's alignment rules.
struct SuperDataBlock {
    std::uint32_t address;
    std::uint32_t dataSize;
    std::uint32_t pageSize;
    std::uint32_t originalSize;
    std::uint8_t mode;
    std::uint8_t attributes;
    std::uint16_t page;
    std::uint8_t modifiedFileTime[8];
    std::uint16_t fileCrc;
    char name[64];
    std::uint16_t crc;
    std::uint8_t ecc[kDotCount];
};
#pragma pack(pop)

static_assert(sizeof(DataBlock) == kBlockBytes, "PaperBak data block must remain 128 bytes");
static_assert(sizeof(SuperDataBlock) == kBlockBytes, "PaperBak superblock must remain 128 bytes");
static_assert(offsetof(DataBlock, data) == 4, "Unexpected data offset");
static_assert(offsetof(DataBlock, crc) == 94, "Unexpected data CRC offset");
static_assert(offsetof(DataBlock, ecc) == 96, "Unexpected ECC offset");
static_assert(offsetof(SuperDataBlock, modifiedFileTime) == 20, "Unexpected FILETIME offset");
static_assert(offsetof(SuperDataBlock, fileCrc) == 28, "Unexpected file CRC offset");
static_assert(offsetof(SuperDataBlock, name) == 30, "Unexpected filename offset");
static_assert(offsetof(SuperDataBlock, crc) == 94, "Unexpected superblock CRC offset");
static_assert(offsetof(SuperDataBlock, ecc) == 96, "Unexpected superblock ECC offset");

} // namespace paperbak
#endif
