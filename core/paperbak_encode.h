#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace paperbak {
struct EncodedPage { int columns,rows,width,height; std::vector<uint8_t> pixels; };
int requiredCells(const std::string &path,int recoveryGroupSize=10);
EncodedPage encodeFile(const std::string &path,int columns,int dotPercent=100,bool nativeCompressed=false,std::uint32_t originalSize=0,int recoveryGroupSize=10);
// Encode a file as several independently readable pages sharing global metadata.
std::vector<EncodedPage> encodePages(const std::string &path,int columns,int fullRows,int pageCount,int recoveryGroupSize,const std::string &password="",int dotPercent=100,bool nativeCompressed=false,std::uint32_t originalSize=0);
}
