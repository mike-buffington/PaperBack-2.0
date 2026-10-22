#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace paperbak {
struct Bitmap { int width=0, height=0; std::vector<uint8_t> pixels; }; // 8-bit grayscale, bottom row first
Bitmap load_bmp(const std::string& filename);
}
