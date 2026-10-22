#include "bmp_loader.h"
#include <fstream>
#include <stdexcept>
#include <limits>
namespace paperbak {
namespace {
uint16_t u16(const std::vector<uint8_t>& b,size_t p) { if(p+2>b.size()) throw std::runtime_error("Truncated BMP"); return uint16_t(b[p]) | uint16_t(b[p+1])<<8; }
uint32_t u32(const std::vector<uint8_t>& b,size_t p) { if(p+4>b.size()) throw std::runtime_error("Truncated BMP"); return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24; }
}
Bitmap load_bmp(const std::string& filename) {
 std::ifstream f(filename,std::ios::binary);
 if(!f) throw std::runtime_error("Cannot open BMP: "+filename);
 std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
 if(b.size()<62 || b[0]!='B' || b[1]!='M') throw std::runtime_error("Not a Windows BMP");
 const uint32_t off=u32(b,10), dib=u32(b,14);
 if(dib<40 || 14ull+dib>b.size()) throw std::runtime_error("Unsupported BMP header");
 const int32_t w=static_cast<int32_t>(u32(b,18)), h=static_cast<int32_t>(u32(b,22));
 if(w<=0 || h==0 || h==std::numeric_limits<int32_t>::min() || w>20000 || h>20000) throw std::runtime_error("Invalid BMP dimensions");
 const int depth=u16(b,28);
 if(u16(b,26)!=1 || (depth!=1 && depth!=8) || u32(b,30)!=0) throw std::runtime_error("Expected uncompressed 1-bit or 8-bit BMP");
 const int height=h<0?-h:h;
 const uint64_t stride=((uint64_t(w)*depth+31)/32)*4;
 if(uint64_t(off)+stride*height>b.size()) throw std::runtime_error("BMP pixel data truncated");
 const size_t palette=14+dib;
 if(palette+size_t(depth==1?8:1024)>off || palette+size_t(depth==1?8:1024)>b.size()) throw std::runtime_error("Missing BMP palette");
 const uint8_t p0=uint8_t((uint32_t(b[palette])+b[palette+1]+b[palette+2])/3);
 const uint8_t p1=uint8_t((uint32_t(b[palette+4])+b[palette+5]+b[palette+6])/3);
 Bitmap out; out.width=w; out.height=height; out.pixels.resize(size_t(w)*height);
 for(int y=0;y<height;y++) {
   const int fileRow=h>0?y:height-1-y; // file order is bottom-up if height positive
   for(int x=0;x<w;x++) {
     if (depth==8) {
       const uint8_t index=b[size_t(off)+size_t(fileRow)*size_t(stride)+size_t(x)];
       const size_t c=palette+size_t(index)*4;
       out.pixels[size_t(y)*w+x]=uint8_t((unsigned(b[c])+b[c+1]+b[c+2])/3);
     } else {
       const uint8_t bits=b[size_t(off)+size_t(fileRow)*size_t(stride)+size_t(x/8)];
       out.pixels[size_t(y)*w+x]=(bits & (0x80u>>(x%8)))?p1:p0;
     }
   }
 }
 return out;
}
}
