// Portable file-data assembler for decoded PaperBak blocks.
#ifndef PAPERBAK_RESTORE_H
#define PAPERBAK_RESTORE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace paperbak {

class RestoreBuffer {
public:
    explicit RestoreBuffer(std::uint32_t compressedSize);

    // Add an ordinary block. Address must be aligned to 90 bytes.
    bool AddDataBlock(std::uint32_t address, const std::uint8_t data[90]);

    // Add a PaperBak XOR recovery block. scopeBytes is the size of the group
    // covered by this block and must be a multiple of 90.
    bool AddRecoveryBlock(std::uint32_t address, std::uint32_t scopeBytes,
                          const std::uint8_t data[90]);

    // Attempt recovery for groups with exactly one missing ordinary block.
    // Returns the number of blocks recovered during this call.
    int RecoverMissingBlocks();

    std::uint32_t Size() const { return compressedSize_; }
    std::size_t BlockCount() const { return valid_.size(); }
    std::size_t PresentBlockCount() const;
    bool IsComplete() const;
    const std::vector<std::uint8_t>& Bytes() const { return bytes_; }

private:
    std::uint32_t compressedSize_;
    std::vector<std::uint8_t> bytes_;
    // 0 = missing, 1 = ordinary data, 2 = recovery parity placeholder.
    std::vector<std::uint8_t> valid_;
    struct RecoveryBlock {
        std::uint32_t address;
        std::uint32_t scopeBytes;
        std::uint8_t data[90];
    };
    std::vector<RecoveryBlock> recoveryBlocks_;
};

} // namespace paperbak
#endif
