#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace paperbak {
std::vector<std::uint8_t> wrapLzma2(const std::string &source, int level);
// Returns false for ordinary PaperBak files, true if the wrapper was decoded.
bool unwrapLzma2(const std::vector<std::uint8_t> &stored, std::vector<std::uint8_t> &result);
}
