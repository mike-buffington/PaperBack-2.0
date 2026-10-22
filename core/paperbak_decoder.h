#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "paperbak_format.h"

namespace paperbak {
struct DecodedBlock {
    int gridX = 0;
    int gridY = 0;
    int eccCorrections = 0;
    DataBlock block{};
};
struct DecodePageResult {
    bool rasterFound = false;
    int gridColumns = 0;
    int gridRows = 0;
    int goodBlocks = 0;
    int badBlocks = 0;
    int superBlocks = 0;
    int correctedBytes = 0;
    float xStep = 0, yStep = 0, xAngle = 0, yAngle = 0;
    std::string error;
    std::vector<DecodedBlock> blocks;
};
DecodePageResult decode_page(const std::uint8_t* bottomUpGray, int width, int height);
}
