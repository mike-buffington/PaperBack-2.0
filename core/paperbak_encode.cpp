#include "paperbak_encode.h"
#include "paperbak_format.h"
#include "paperbak_crypto.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <limits>
#include <filesystem>
using uchar=unsigned char;
using ushort=unsigned short;
extern ushort Crc16(uchar*,int);
extern void Encode8(uchar*,uchar*,int);
namespace paperbak {
namespace {
constexpr int R=10, DOT=4, CELL=35*DOT, PAD=25;
struct Source { std::vector<uint8_t> bytes; std::string name; int strings,cells; };
Source read(const std::string &path) {
 std::ifstream f(path,std::ios::binary);
 if(!f) throw std::runtime_error("Cannot open input file");
 Source s; s.bytes.assign(std::istreambuf_iterator<char>(f),{});
 if(s.bytes.empty()) throw std::runtime_error("Empty files are not supported yet");
 auto slash=path.find_last_of("/\\"); s.name=path.substr(slash==std::string::npos?0:slash+1);
 if(s.name.size()>63) throw std::runtime_error("Filename is too long for PaperBak: maximum 63 UTF-8 bytes for unencrypted backups (31 bytes for encrypted backups).");
 int n=int((s.bytes.size()+89)/90); s.strings=(n+R-1)/R;
 s.cells=((s.strings+1)*(R+1)+1+9)/10*10; // Pad unused cells with superblocks for usable grid layouts.
 return s;
}
void finish(DataBlock &b) {
 auto *p=reinterpret_cast<uint8_t*>(&b);
 b.crc=uint16_t(Crc16(p,94)^0x55AA);
 Encode8(p,b.ecc,127);
}
void paint(std::vector<uint8_t> &image,int width,int height,int col,int row,const DataBlock &b,int dotPercent) {
 auto *p=reinterpret_cast<const uint8_t*>(&b);
 for(int y=0;y<32;y++) {
  uint32_t t=uint32_t(p[y*4])|(uint32_t(p[y*4+1])<<8)|(uint32_t(p[y*4+2])<<16)|(uint32_t(p[y*4+3])<<24);
  t^=(y%2)?0xAAAAAAAAu:0x55555555u;
  for(int x=0;x<32;x++) if(t&(1u<<x)) {
   int px=PAD+col*CELL+(x+2)*DOT, py=PAD+row*CELL+(y+2)*DOT;
   // The original Windows setting scales the printed black dot relative to its pitch.
   // Keep the 4-pixel pitch and centre a smaller dot in each cell.
   // Supersampling is not needed for the fixed-resolution monochrome raster.
   const int diameter = std::max(1,(DOT*dotPercent+50)/100);
   const int inset = (DOT-diameter)/2;
   for(int yy=0;yy<diameter;yy++) for(int xx=0;xx<diameter;xx++) {
     // At 50%, round corners for a circular dot; intermediate settings gradually round.
     const double radius=double(diameter)/2.0;
     const double cx=(xx+0.5)-radius, cy=(yy+0.5)-radius;
     const double roundness=double(100-dotPercent)/50.0;
     const double box=std::max(std::abs(cx),std::abs(cy));
     const double circle=std::sqrt(cx*cx+cy*cy);
     if ((1.0-roundness)*box+roundness*circle <= radius)
       image[size_t(py+inset+yy)*width+px+inset+xx]=0;
   }
  }
 }
}
void line(std::vector<uint8_t>&img,int w,int h,int x,int y,int len,bool horizontal) {
 for(int i=0;i<len;i++) for(int j=0;j<DOT;j++) {
  int xx=x+(horizontal?i:j),yy=y+(horizontal?j:i);
  if(xx>=0&&xx<w&&yy>=0&&yy<h) img[size_t(yy)*w+xx]=0;
 }
}
}
int requiredCells(const std::string &path){return read(path).cells;}
EncodedPage encodeFile(const std::string &path,int columns,int dotPercent,bool nativeCompressed,std::uint32_t originalSize) {
 if(dotPercent<50 || dotPercent>100) throw std::runtime_error("Dot size must be between 50 and 100 percent");
 Source s=read(path);
 if(columns<1 || s.cells%columns) throw std::runtime_error("Columns must divide the total cell count");
 int rows=s.cells/columns;
 if(columns<3 || rows<3) throw std::runtime_error("For the first round-trip test choose a grid with at least 3 columns and 3 rows");
 int w=columns*CELL+DOT+PAD*2, h=rows*CELL+DOT+PAD*2;
 if(w>20000 || h>20000 || uint64_t(w)*h>150000000) throw std::runtime_error("Selected layout exceeds the supported image size");
 EncodedPage out{columns,rows,w,h,std::vector<uint8_t>(size_t(w)*h,255)};
 for(int x=0;x<=columns;x++) line(out.pixels,w,h,PAD+x*CELL,PAD,rows*CELL+DOT,false);
 for(int y=0;y<=rows;y++) line(out.pixels,w,h,PAD,PAD+y*CELL,columns*CELL+DOT,true);
 std::vector<DataBlock> cells(size_t(columns)*rows);
 SuperDataBlock super{};
 super.address=0xFFFFFFFFu; super.dataSize=uint32_t((s.bytes.size()+89)/90*90);
 super.pageSize=super.dataSize; super.originalSize=nativeCompressed ? originalSize : uint32_t(s.bytes.size()); super.page=1;
 if(nativeCompressed) super.mode |= kCompressed;
 super.fileCrc=0;
 std::vector<uint8_t> padded= s.bytes; padded.resize(super.dataSize,0);
 super.fileCrc=Crc16(padded.data(),int(padded.size()));
 std::memcpy(super.name,s.name.c_str(),s.name.size());
 DataBlock sb{}; std::memcpy(&sb,&super,sizeof(sb)); finish(sb);
 std::fill(cells.begin(),cells.end(),sb);
 auto pos=[&](int chain,int i) {
  int k=chain*(s.strings+1);
  if(s.strings+1<columns) k+=i;
  else { int rot=(columns/(R+1)*chain-k%columns+columns)%columns; k+=(i+rot)%(s.strings+1); }
  if(k<0 || k>=int(cells.size())) throw std::runtime_error("Grid placement failed");
  return k;
 };
 for(int chain=0;chain<=R;chain++) cells[pos(chain,0)]=sb;
 uint32_t offset=0;
 for(int i=0;i<s.strings;i++) {
  DataBlock recovery{}; recovery.address=offset^(R<<28); std::memset(recovery.data,0xFF,90);
  for(int j=0;j<R;j++) {
   DataBlock b{}; b.address=offset;
   if(offset<padded.size()) std::memcpy(b.data,padded.data()+offset,std::min<size_t>(90,padded.size()-offset));
   for(int k=0;k<90;k++) recovery.data[k]^=b.data[k];
   finish(b); cells[pos(j,i+1)]=b; offset+=90;
  }
  finish(recovery); cells[pos(R,i+1)]=recovery;
 }
 for(int y=0;y<rows;y++) for(int x=0;x<columns;x++) paint(out.pixels,w,h,x,y,cells[size_t(y)*columns+x],dotPercent);
 // Crop only the outer page padding. Keep the grid boundary lines and
 // all block-internal quiet space intact for decoder compatibility.
 const int croppedWidth = w - 2*PAD, croppedHeight = h - 2*PAD;
 std::vector<uint8_t> cropped(size_t(croppedWidth)*croppedHeight);
 for(int y=0;y<croppedHeight;y++)
  std::memcpy(cropped.data()+size_t(y)*croppedWidth,
              out.pixels.data()+size_t(y+PAD)*w+PAD, size_t(croppedWidth));
 out.width=croppedWidth;
 out.height=croppedHeight;
 out.pixels=std::move(cropped);
 return out;
}

std::vector<EncodedPage> encodePages(const std::string &path,int columns,int fullRows,int pageCount,int recoveryGroupSize,const std::string &password,int dotPercent,bool nativeCompressed,std::uint32_t originalSize) {
 if(dotPercent<50 || dotPercent>100) throw std::runtime_error("Dot size must be between 50 and 100 percent");
 Source s=read(path);
 const bool encrypted = !password.empty();
 if (encrypted && s.name.size()>31)
  throw std::runtime_error("Filename is too long for an encrypted PaperBak backup: maximum 31 UTF-8 bytes, including the extension. Rename the source file and try again.");
 // Encrypt exactly the original AES-aligned length, then place into 90-byte blocks.
 std::uint8_t saltAndIV[32] = {};
 std::uint16_t plaintextCRC = 0;
 std::uint32_t storedLength = 0;
 if (encrypted) {
  s.bytes.resize((s.bytes.size()+15)/16*16,0);
  plaintextCRC = Crc16(s.bytes.data(),int(s.bytes.size()));
  randomSaltAndIV(saltAndIV);
  s.bytes = cryptBackup(s.bytes,password,saltAndIV,true);
  storedLength = uint32_t(s.bytes.size());
 }
 const int R=recoveryGroupSize;
 if(R<1 || R>15) throw std::runtime_error("Recovery group size must be between 1 and 15");
 if(columns<3 || fullRows<3 || pageCount<1 || pageCount>1000)
  throw std::runtime_error("Invalid columns, rows, or page count");
 if(uint64_t(columns)*fullRows>100000 || uint64_t(columns*CELL+DOT)*(fullRows*CELL+DOT)>150000000)
  throw std::runtime_error("Page dimensions exceed supported image size");
 const int groups=int(((s.bytes.size()+89)/90+R-1)/R);
 // pageCount is a MAXIMUM, not a required count. Fill pages sequentially,
 // creating only the pages needed at the chosen grid density.
 const int perPageCapacity=(columns*fullRows-(R+1))/(R+1);
 if(perPageCapacity<1 || int64_t(perPageCapacity)*pageCount<groups)
  throw std::runtime_error("The backup does not fit within the selected maximum pages; increase the limit or grid size");
 const int actualPages=(groups+perPageCapacity-1)/perPageCapacity;
 std::vector<int> allocation;
 allocation.reserve(actualPages);
 int remaining=groups;
 while(remaining>0) {
  int take=std::min(perPageCapacity,remaining);
  allocation.push_back(take);
  remaining-=take;
 }
 std::vector<uint8_t> padded=s.bytes;
 padded.resize(size_t((s.bytes.size()+89)/90)*90,0);
 SuperDataBlock super{};
 super.address=0xffffffffu;
 super.dataSize=encrypted ? storedLength : uint32_t(padded.size());
 // Windows PaperBak expects the nominal data capacity of ONE full page here.
 // It derives the expected page count as ceil(dataSize / pageSize).
 super.pageSize=uint32_t(uint64_t(perPageCapacity)*R*90);
 super.originalSize=nativeCompressed ? originalSize : uint32_t(std::filesystem::file_size(path));
 super.fileCrc=encrypted ? plaintextCRC : Crc16(padded.data(),int(padded.size()));
 if (encrypted) super.mode |= kEncrypted;
 if (nativeCompressed) super.mode |= kCompressed;
 std::memcpy(super.name,s.name.data(),s.name.size());
 if (encrypted) std::memcpy(super.name+32,saltAndIV,32);
 std::vector<EncodedPage> pages;
 pages.reserve(actualPages);
 int groupStart=0;
 for(int pageIndex=0;pageIndex<actualPages;pageIndex++) {
  const int count=allocation[pageIndex];
  const int rows=pageIndex==actualPages-1
      ?std::max(3,(count*(R+1)+(R+1)+columns-1)/columns)
      :fullRows;
  if(rows>fullRows) throw std::runtime_error("Page rows exceed selected maximum");
  int width=columns*CELL+DOT,height=rows*CELL+DOT;
  int stageW=width+2*PAD,stageH=height+2*PAD;
  EncodedPage out{columns,rows,width,height,std::vector<uint8_t>(size_t(stageW)*stageH,255)};
  for(int x=0;x<=columns;x++) line(out.pixels,stageW,stageH,PAD+x*CELL,PAD,rows*CELL+DOT,false);
  for(int y=0;y<=rows;y++) line(out.pixels,stageW,stageH,PAD,PAD+y*CELL,columns*CELL+DOT,true);
  super.page=uint16_t(pageIndex+1);
  DataBlock sb{};std::memcpy(&sb,&super,sizeof(sb));finish(sb);
  std::vector<DataBlock> blocks(size_t(columns)*rows,sb);
  // Interleave chains of data and recovery blocks in the page grid.
  // Unlike the original single-page encoder, groups are offset globally.
  auto position=[&](int chain,int i) {
   int k=chain*(count+1);
   if(count+1<columns) k+=i;
   else {int rot=(columns/(R+1)*chain-k%columns+columns)%columns;k+=(i+rot)%(count+1);}
   if(k<0 || k>=int(blocks.size())) throw std::runtime_error("Page placement overflow");
   return k;
  };
  for(int chain=0;chain<=R;chain++) blocks[position(chain,0)]=sb;
  for(int i=0;i<count;i++) {
   DataBlock recovery{};
   uint32_t start=uint32_t((groupStart+i)*R*90);
   recovery.address=start^(R<<28);
   std::memset(recovery.data,0xff,90);
   for(int j=0;j<R;j++) {
    uint32_t offset=start+uint32_t(j*90);
    DataBlock b{};b.address=offset;
    if(offset<padded.size()) std::memcpy(b.data,padded.data()+offset,std::min<size_t>(90,padded.size()-offset));
    for(int k=0;k<90;k++) recovery.data[k]^=b.data[k];
    finish(b);blocks[position(j,i+1)]=b;
   }
   finish(recovery);blocks[position(R,i+1)]=recovery;
  }
  for(int y=0;y<rows;y++) for(int x=0;x<columns;x++)
   paint(out.pixels,stageW,stageH,x,y,blocks[size_t(y)*columns+x],dotPercent);
  std::vector<uint8_t> cropped(size_t(width)*height);
  for(int y=0;y<height;y++) std::memcpy(cropped.data()+size_t(y)*width,
      out.pixels.data()+size_t(y+PAD)*stageW+PAD,size_t(width));
  out.pixels=std::move(cropped);
  // paint() uses the legacy 25-pixel offset. Render into a padded staging
  // buffer, then crop it, as in the single-page encoder.
  pages.push_back(std::move(out));
  groupStart+=count;
 }
 return pages;
}

}
