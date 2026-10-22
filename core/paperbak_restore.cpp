#include "paperbak_restore.h"
#include "paperbak_format.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace paperbak {

RestoreBuffer::RestoreBuffer(std::uint32_t compressedSize)
    : compressedSize_(compressedSize),
      bytes_(static_cast<std::size_t>(compressedSize), 0),
      valid_((static_cast<std::size_t>(compressedSize) + kDataBytes - 1) / kDataBytes, 0) {}

bool RestoreBuffer::AddDataBlock(std::uint32_t address, const std::uint8_t data[90]) {
    if (data == nullptr || address % kDataBytes != 0 || address >= compressedSize_) return false;
    const std::size_t index = address / kDataBytes;
    if (index >= valid_.size()) return false;
    const std::size_t count = std::min<std::size_t>(kDataBytes, bytes_.size() - address);
    std::memcpy(bytes_.data() + address, data, count);
    valid_[index] = 1;
    return true;
}

bool RestoreBuffer::AddRecoveryBlock(std::uint32_t address, std::uint32_t scopeBytes,
                                     const std::uint8_t data[90]) {
    if (data == nullptr || scopeBytes == 0 || scopeBytes % kDataBytes != 0 ||
        address % scopeBytes != 0 || address >= compressedSize_) return false;
    const std::uint64_t end = static_cast<std::uint64_t>(address) + scopeBytes;
    if (end > compressedSize_) return false;
    RecoveryBlock block{};
    block.address = address;
    block.scopeBytes = scopeBytes;
    std::memcpy(block.data, data, kDataBytes);
    recoveryBlocks_.push_back(block);
    return true;
}

int RestoreBuffer::RecoverMissingBlocks() {
    int recovered = 0;
    for (const RecoveryBlock& parity : recoveryBlocks_) {
        const std::size_t first = parity.address / kDataBytes;
        const std::size_t count = parity.scopeBytes / kDataBytes;
        if (first + count > valid_.size()) continue;

        std::size_t missing = count;
        int missingCount = 0;
        for (std::size_t i = first; i < first + count; ++i) {
            if (valid_[i] != 1) {
                missing = i;
                ++missingCount;
            }
        }
        if (missingCount != 1) continue;

        // PaperBak stores the bitwise-inverted XOR of the covered data blocks.
        std::uint8_t* destination = bytes_.data() + missing * kDataBytes;
        std::memcpy(destination, parity.data, kDataBytes);
        for (std::size_t j = 0; j < kDataBytes; ++j) destination[j] ^= 0xFF;
        for (std::size_t i = first; i < first + count; ++i) {
            if (i == missing) continue;
            const std::uint8_t* source = bytes_.data() + i * kDataBytes;
            for (std::size_t j = 0; j < kDataBytes; ++j) destination[j] ^= source[j];
        }
        valid_[missing] = 1;
        ++recovered;
    }
    return recovered;
}

std::size_t RestoreBuffer::PresentBlockCount() const {
    return static_cast<std::size_t>(std::count(valid_.begin(), valid_.end(), 1));
}

bool RestoreBuffer::IsComplete() const {
    return std::all_of(valid_.begin(), valid_.end(), [](std::uint8_t state) { return state == 1; });
}

} // namespace paperbak
