#include "paperbak_lzma.h"
#include <lzma.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
namespace paperbak {
namespace {
constexpr std::array<std::uint8_t,16> magic = {'P','a','p','e','r','B','a','c','k','L','Z','M','A','2',0,1};
constexpr std::uint64_t kMaxUncompressed = 512ULL * 1024 * 1024;
void append64(std::vector<std::uint8_t>& v, std::uint64_t n) {
    for(int i=0;i<8;++i) v.push_back(std::uint8_t(n>>(8*i)));
}
std::uint64_t read64(const std::uint8_t *p) {
    std::uint64_t n=0;
    for(int i=0;i<8;++i) n |= std::uint64_t(p[i])<<(8*i);
    return n;
}
}
std::vector<std::uint8_t> wrapLzma2(const std::string &source,int level) {
    if(level!=1 && level!=5 && level!=9) throw std::runtime_error("Invalid LZMA2 compression level");
    std::ifstream file(source,std::ios::binary);
    if(!file) throw std::runtime_error("Unable to open file for LZMA2 compression");
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),{});
    if(data.size()>kMaxUncompressed) throw std::runtime_error("LZMA2 input exceeds the 512 MB safety limit");
    lzma_stream stream=LZMA_STREAM_INIT;
    lzma_ret rc=lzma_easy_encoder(&stream,level,LZMA_CHECK_CRC64);
    if(rc!=LZMA_OK) throw std::runtime_error("Unable to initialize LZMA2 encoder");
    std::vector<std::uint8_t> result(magic.begin(),magic.end());
    append64(result,data.size());
    stream.next_in=data.data(); stream.avail_in=data.size();
    std::array<std::uint8_t,65536> output{};
    do {
        stream.next_out=output.data();stream.avail_out=output.size();
        rc=lzma_code(&stream,LZMA_FINISH);
        result.insert(result.end(),output.data(),output.data()+output.size()-stream.avail_out);
        if(rc!=LZMA_OK && rc!=LZMA_STREAM_END) {lzma_end(&stream);throw std::runtime_error("LZMA2 compression failed");}
    } while(rc!=LZMA_STREAM_END);
    lzma_end(&stream);
    return result;
}
bool unwrapLzma2(const std::vector<std::uint8_t>& stored,std::vector<std::uint8_t>& result) {
    if(stored.size()<magic.size() || !std::equal(magic.begin(),magic.end(),stored.begin())) return false;
    if(stored.size()<24) throw std::runtime_error("Truncated LZMA2 backup wrapper");
    const std::uint64_t length=read64(stored.data()+16);
    if(length>kMaxUncompressed) throw std::runtime_error("LZMA2 restored file exceeds the 512 MB safety limit");
    result.resize(size_t(length));
    // XZ stream framing carries the LZMA2 data and CRC64 integrity check.
    std::uint64_t memlimit=512ULL*1024*1024;
    size_t inPos=0,outPos=0;
    const auto *in=stored.data()+24;
    const size_t inSize=stored.size()-24;
    lzma_ret rc=lzma_stream_buffer_decode(&memlimit,0,nullptr,in,&inPos,inSize,
        result.data(),&outPos,result.size());
    if(rc!=LZMA_OK || outPos!=result.size() || inPos!=inSize)
        throw std::runtime_error("LZMA2 decompression failed or backup is damaged");
    return true;
}
}
