#import "PaperBakBridge.h"
#include "bmp_loader.h"
#include "paperbak_decoder.h"
#include "paperbak_format.h"
#include "paperbak_restore.h"
#include "paperbak_encode.h"
#include "paperbak_crypto.h"
#include "paperbak_lzma.h"
#include <filesystem>
#import <AppKit/AppKit.h>
#include <bzlib.h>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>

using uchar=unsigned char;
using ushort=unsigned short;
extern ushort Crc16(uchar*,int);

namespace {
struct PreparedInput {
    std::string path;
    std::filesystem::path folder;
    std::uint32_t originalSize = 0;
    bool nativeCompressed = false;
    PreparedInput(const std::string &source,const std::string &method,int level):path(source) {
        const auto sourceLength=std::filesystem::file_size(source);
        if(sourceLength==0 || sourceLength>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Source file size is unsupported");
        originalSize=std::uint32_t(sourceLength);
        // Uncompressed backups use the original input directly. No temporary
        // file or compression flag is required; the encoder retains its name.
        if(method=="none") return;
        std::vector<std::uint8_t> compressed;
        if(method=="original") {
            // PaperBak's native compressed-data mode uses a BZip2 stream.
            std::ifstream in(source,std::ios::binary);
            if(!in) throw std::runtime_error("Unable to open input for BZip2 compression");
            std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
            if(bytes.size()>std::numeric_limits<unsigned int>::max()-600)
                throw std::runtime_error("File too large for BZip2 compression");
            unsigned int capacity=unsigned(bytes.size()+bytes.size()/100+601);
            compressed.resize(capacity);
            const int rc=BZ2_bzBuffToBuffCompress(reinterpret_cast<char*>(compressed.data()),&capacity,
                reinterpret_cast<char*>(bytes.data()),unsigned(bytes.size()),9,0,30);
            if(rc!=BZ_OK) throw std::runtime_error("Native BZip2 compression failed");
            compressed.resize(capacity);
            nativeCompressed=true;
        } else if(method=="lzma2") {
            compressed=paperbak::wrapLzma2(source,level);
        } else {
            throw std::runtime_error("Unknown compression method");
        }
        NSString *tmp=[[NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]] stringByStandardizingPath];
        folder=std::string(tmp.fileSystemRepresentation);
        std::filesystem::create_directories(folder);
        path=(folder/std::filesystem::path(source).filename()).string();
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out || !out.write(reinterpret_cast<const char*>(compressed.data()),std::streamsize(compressed.size())))
            throw std::runtime_error("Unable to write temporary compressed backup data");
    }
    ~PreparedInput() { if(!folder.empty()) {std::error_code ec;std::filesystem::remove_all(folder,ec);} }
    PreparedInput(const PreparedInput&)=delete;
    PreparedInput& operator=(const PreparedInput&)=delete;
};
struct Scan {
    paperbak::DecodePageResult decoded;
    paperbak::SuperDataBlock metadata{};
    std::string name;
};
Scan scan(const std::string &path) {
    paperbak::Bitmap bmp;
    const auto dot = path.find_last_of('.');
    std::string extension = dot == std::string::npos ? "" : path.substr(dot);
    for (char &c : extension) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    if (extension == ".png") {
        NSString *filePath = [NSString stringWithUTF8String:path.c_str()];
        NSBitmapImageRep *image = [[NSBitmapImageRep alloc] initWithData:[NSData dataWithContentsOfFile:filePath]];
        if (!image) throw std::runtime_error("Unable to open PNG image");
        const NSInteger w = image.pixelsWide, h = image.pixelsHigh;
        if (w <= 0 || h <= 0 || w > 20000 || h > 20000 || (uint64_t)w * (uint64_t)h > 150000000)
            throw std::runtime_error("Unsupported PNG dimensions");
        // Decode all pixels at once using Core Graphics. AppKit's colorAtX:y:
        // is extremely slow for multi-million-pixel backup images.
        CGImageRef cgImage = image.CGImage;
        if (!cgImage) throw std::runtime_error("Unable to decode PNG pixels");
        CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
        if (!gray) throw std::runtime_error("Unable to create grayscale color space");
        bmp.width = int(w);
        bmp.height = int(h);
        bmp.pixels.resize(size_t(w) * size_t(h));
        CGContextRef context = CGBitmapContextCreate(bmp.pixels.data(),
            size_t(w), size_t(h), 8, size_t(w), gray, kCGImageAlphaNone);
        CGColorSpaceRelease(gray);
        if (!context) throw std::runtime_error("Unable to create PNG pixel buffer");
        // Convert Core Graphics' bottom-left coordinate system to top-left.
        CGContextTranslateCTM(context, 0, CGFloat(h));
        CGContextScaleCTM(context, 1, -1);
        CGContextSetInterpolationQuality(context, kCGInterpolationNone);
        CGContextDrawImage(context, CGRectMake(0, 0, CGFloat(w), CGFloat(h)), cgImage);
        CGContextRelease(context);
    } else {
        bmp = paperbak::load_bmp(path);
    }
    Scan result;
    result.decoded=paperbak::decode_page(bmp.pixels.data(),bmp.width,bmp.height);
    if (!result.decoded.rasterFound || result.decoded.blocks.empty()) throw std::runtime_error("No readable PaperBack blocks");
    bool found=false;
    for (const auto &entry:result.decoded.blocks) {
        if (entry.block.address!=paperbak::kSuperblockAddress) continue;
        paperbak::SuperDataBlock s{};
        std::memcpy(&s,&entry.block,sizeof(s));
        if (!found) { result.metadata=s; found=true; }
        else if (s.dataSize!=result.metadata.dataSize || s.originalSize!=result.metadata.originalSize ||
                 s.mode!=result.metadata.mode || s.fileCrc!=result.metadata.fileCrc ||
                 std::memcmp(s.name,result.metadata.name,(s.mode & paperbak::kEncrypted) ? 32 : 64)!=0)
            throw std::runtime_error("Conflicting superblock metadata");
    }
    if (!found) throw std::runtime_error("No valid PaperBack backup information was found in this image.");
    if (!result.metadata.dataSize || result.metadata.dataSize>100000000 || result.metadata.originalSize>100000000)
        throw std::runtime_error("Unsupported backup size");
    const size_t nameLimit = (result.metadata.mode & paperbak::kEncrypted) ? 32 : 64;
    result.name.assign(result.metadata.name,result.metadata.name+nameLimit);
    const auto terminator = result.name.find('\0');
    if (terminator != std::string::npos) result.name.resize(terminator);
    return result;
}
// Assemble independently scanned pages of one Windows-compatible backup.
struct MultiScan {
    paperbak::SuperDataBlock metadata{};
    std::string name;
    std::vector<Scan> pages;
    int readableBlocks = 0;
};
MultiScan scanPages(const std::vector<std::string> &paths) {
    if (paths.empty()) throw std::runtime_error("Select at least one backup page");
    MultiScan combined;
    std::vector<unsigned> pageNumbers;
    for (const auto &path : paths) {
        Scan page = scan(path);
        const auto &m = page.metadata;
        if (pageNumbers.empty()) { combined.metadata = m; combined.name = page.name; }
        else if (m.dataSize != combined.metadata.dataSize ||
                 m.pageSize != combined.metadata.pageSize ||
                 m.originalSize != combined.metadata.originalSize ||
                 m.mode != combined.metadata.mode ||
                 m.fileCrc != combined.metadata.fileCrc ||
                 page.name != combined.name)
            throw std::runtime_error("Selected pages belong to different backups");
        if (m.page == 0 || std::find(pageNumbers.begin(), pageNumbers.end(), m.page) != pageNumbers.end())
            throw std::runtime_error("Duplicate or invalid PaperBack page number");
        pageNumbers.push_back(m.page);
        combined.readableBlocks += page.decoded.goodBlocks;
        combined.pages.push_back(std::move(page));
    }
    return combined;
}
void restorePages(const std::vector<std::string> &paths,const std::string &destination,const std::string &password) {
    MultiScan s=scanPages(paths);
    const auto &m=s.metadata;
    if (m.mode & ~unsigned(paperbak::kCompressed|paperbak::kEncrypted)) throw std::runtime_error("Unknown backup mode");
    paperbak::RestoreBuffer buffer(m.dataSize);
    for (const auto &page:s.pages) for (const auto &entry:page.decoded.blocks) {
        const auto &b=entry.block;
        if (b.address==paperbak::kSuperblockAddress) continue;
        const std::uint32_t group=b.address>>28;
        if (!group) buffer.AddDataBlock(b.address,b.data);
        else buffer.AddRecoveryBlock(b.address & 0x0fffffffu,group*paperbak::kDataBytes,b.data);
    }
    buffer.RecoverMissingBlocks();
    if (!buffer.IsComplete()) throw std::runtime_error("Incomplete backup: select all pages (some data blocks are missing)");
    auto stored=buffer.Bytes();
    if (m.mode & paperbak::kEncrypted) {
        if (password.empty()) throw std::runtime_error("Password required for this encrypted backup");
        // PaperBak stores salt and IV in bytes 32..63 of the 64-byte name field.
        const auto* salt = reinterpret_cast<const std::uint8_t*>(m.name+32);
        stored = paperbak::cryptBackup(stored,password,salt,false);
    }
    if (Crc16(stored.data(),static_cast<int>(stored.size()))!=m.fileCrc)
        throw std::runtime_error((m.mode & paperbak::kEncrypted) ? "Incorrect password or damaged backup" : "Backup integrity check failed. The data may be damaged or incomplete.");
    std::vector<std::uint8_t> bytes;
    if (m.mode & paperbak::kCompressed) {
        bytes.resize(m.originalSize);
        unsigned int size=static_cast<unsigned int>(bytes.size());
        const int rc=BZ2_bzBuffToBuffDecompress(reinterpret_cast<char*>(bytes.data()),&size,
             reinterpret_cast<char*>(stored.data()),static_cast<unsigned int>(stored.size()),0,0);
        if (rc!=BZ_OK || size!=bytes.size()) throw std::runtime_error("BZip2 decompression failed");
    } else {
        if (m.originalSize>stored.size()) throw std::runtime_error("Original size exceeds stored data");
        bytes.assign(stored.begin(),stored.begin()+m.originalSize);
    }
    std::vector<std::uint8_t> expanded;
    if(paperbak::unwrapLzma2(bytes,expanded)) bytes=std::move(expanded);
    std::ofstream f(destination,std::ios::binary|std::ios::trunc);
    if (!f || !f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to write restored file");
}
std::vector<std::string> pathsFromNSArray(NSArray<NSString *> *paths) {
    std::vector<std::string> result;
    for (NSString *path in paths) result.emplace_back(path.fileSystemRepresentation);
    return result;
}
void restore(const std::string &source,const std::string &destination,const std::string &password) {
    auto s=scan(source);
    const auto &m=s.metadata;
    if (m.mode & ~unsigned(paperbak::kCompressed|paperbak::kEncrypted)) throw std::runtime_error("Unknown backup mode");
    paperbak::RestoreBuffer buffer(m.dataSize);
    for (const auto &entry:s.decoded.blocks) {
        const auto &b=entry.block;
        if (b.address==paperbak::kSuperblockAddress) continue;
        const std::uint32_t group=b.address>>28;
        if (!group) buffer.AddDataBlock(b.address,b.data);
        else buffer.AddRecoveryBlock(b.address & 0x0fffffffu,group*paperbak::kDataBytes,b.data);
    }
    buffer.RecoverMissingBlocks();
    if (!buffer.IsComplete()) throw std::runtime_error("Incomplete backup: some blocks are missing");
    auto stored=buffer.Bytes();
    if (m.mode & paperbak::kEncrypted) {
        if (password.empty()) throw std::runtime_error("Password required for this encrypted backup");
        // PaperBak stores salt and IV in bytes 32..63 of the 64-byte name field.
        const auto* salt = reinterpret_cast<const std::uint8_t*>(m.name+32);
        stored = paperbak::cryptBackup(stored,password,salt,false);
    }
    if (Crc16(stored.data(),static_cast<int>(stored.size()))!=m.fileCrc)
        throw std::runtime_error((m.mode & paperbak::kEncrypted) ? "Incorrect password or damaged backup" : "Backup integrity check failed. The data may be damaged or incomplete.");
    std::vector<std::uint8_t> bytes;
    if (m.mode & paperbak::kCompressed) {
        bytes.resize(m.originalSize);
        unsigned int size=static_cast<unsigned int>(bytes.size());
        const int rc=BZ2_bzBuffToBuffDecompress(reinterpret_cast<char*>(bytes.data()),&size,
             reinterpret_cast<char*>(stored.data()),static_cast<unsigned int>(stored.size()),0,0);
        if (rc!=BZ_OK || size!=bytes.size()) throw std::runtime_error("BZip2 decompression failed");
    } else {
        if (m.originalSize>stored.size()) throw std::runtime_error("Original size exceeds stored data");
        bytes.assign(stored.begin(),stored.begin()+m.originalSize);
    }
    std::vector<std::uint8_t> expanded;
    if(paperbak::unwrapLzma2(bytes,expanded)) bytes=std::move(expanded);
    std::ofstream f(destination,std::ios::binary|std::ios::trunc);
    if (!f || !f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to write restored file");
}
NSError *errorFor(const std::exception &e) {
    return [NSError errorWithDomain:@"PaperBakMac" code:1 userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithUTF8String:e.what()]}];
}
}
@implementation PaperBakBridge
+ (NSDictionary<NSString *, id> *)inspectPages:(NSArray<NSString *> *)paths error:(NSError **)error {
    try {
        auto s=scanPages(pathsFromNSArray(paths));
        NSMutableArray<NSNumber *> *numbers=[NSMutableArray array];
        for (const auto &page:s.pages) [numbers addObject:@(page.metadata.page)];
        NSMutableArray *qualityPages=[NSMutableArray array];
        for (const auto &page:s.pages) {
            const auto &d=page.decoded;
            // -1 = unprocessed/unreadable, 0...16 = Reed-Solomon corrections.
            NSMutableArray<NSNumber *> *cells=[NSMutableArray arrayWithCapacity:size_t(d.gridColumns)*d.gridRows];
            for (int i=0;i<d.gridColumns*d.gridRows;i++) [cells addObject:@(-1)];
            for (const auto &block:d.blocks) {
                const int index=block.gridY*d.gridColumns+block.gridX;
                if (index>=0 && index<(int)cells.count) cells[index]=@(block.eccCorrections);
            }
            [qualityPages addObject:@{ @"page": @(page.metadata.page), @"columns": @(d.gridColumns),
                                       @"rows": @(d.gridRows), @"cells": cells,
                                       @"bad": @(d.badBlocks), @"good": @(d.goodBlocks+d.superBlocks) }];
        }
        return @{ @"name": [NSString stringWithUTF8String:s.name.c_str()],
                  @"size": @(s.metadata.originalSize), @"storedSize": @(s.metadata.dataSize),
                  @"pages": @(s.pages.size()), @"pageNumbers": numbers,
                  @"blocks": @(s.readableBlocks), @"qualityPages": qualityPages,
                  @"encrypted": @((s.metadata.mode & paperbak::kEncrypted)!=0) };
    } catch (const std::exception &e) { if(error) *error=errorFor(e); return nil; }
}
+ (BOOL)restorePages:(NSArray<NSString *> *)paths to:(NSString *)destination password:(NSString *)password error:(NSError **)error {
    try { ::restorePages(pathsFromNSArray(paths),std::string(destination.fileSystemRepresentation),std::string(password.UTF8String ?: "")); return YES; }
    catch(const std::exception &e) { if(error) *error=errorFor(e); return NO; }
}

+ (NSDictionary<NSString *, id> *)inspect:(NSString *)path error:(NSError **)error {
    try {
        auto s=scan(std::string(path.fileSystemRepresentation));
        return @{ @"name": [NSString stringWithUTF8String:s.name.c_str()],
                  @"size": @(s.metadata.originalSize), @"storedSize": @(s.metadata.dataSize),
                  @"page": @(s.metadata.page), @"blocks": @(s.decoded.goodBlocks),
                  @"encrypted": @((s.metadata.mode & paperbak::kEncrypted)!=0) };
    } catch (const std::exception &e) { if(error) *error=errorFor(e); return nil; }
}
+ (BOOL)restore:(NSString *)source to:(NSString *)destination password:(NSString *)password error:(NSError **)error {
    try { ::restore(std::string(source.fileSystemRepresentation),std::string(destination.fileSystemRepresentation),std::string(password.UTF8String ?: "")); return YES; }
    catch(const std::exception &e) { if(error) *error=errorFor(e); return NO; }
}
+ (NSDictionary<NSString *, NSNumber *> *)sourceLayout:(NSString *)source compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel error:(NSError **)error {
    try {
        PreparedInput prepared(std::string(source.fileSystemRepresentation),std::string(compression.UTF8String ?: "original"),int(lzmaLevel));
        auto bytes=std::filesystem::file_size(prepared.path);
        int cells=paperbak::requiredCells(prepared.path);
        return @{ @"bytes": @(bytes), @"cells": @(cells) };
    } catch(const std::exception &e) { if(error) *error=errorFor(e); return nil; }
}
+ (NSInteger)requiredCells:(NSString *)path error:(NSError **)error {
    try { return paperbak::requiredCells(std::string(path.fileSystemRepresentation)); }
    catch(const std::exception &e) { if(error) *error=errorFor(e); return -1; }
}
// Write a binary black/white PaperBak page. BMP uses an explicit Windows
// BITMAPINFOHEADER, an 8-bit grayscale palette, and bottom-up padded rows.
// This avoids AppKit silently exporting a 24-bit BMP.
static void writeLE16(std::vector<uint8_t> &v, uint16_t n) {
    v.push_back(uint8_t(n)); v.push_back(uint8_t(n >> 8));
}
static void writeLE32(std::vector<uint8_t> &v, uint32_t n) {
    for (int i = 0; i < 4; ++i) v.push_back(uint8_t(n >> (8*i)));
}
static void savePaperBakImage(const paperbak::EncodedPage &page, NSString *destination, NSString *imageFormat) {
    const int w = page.width, h = page.height;
    if (w <= 0 || h <= 0 || page.pixels.size() != size_t(w)*size_t(h))
        throw std::runtime_error("Invalid encoded page dimensions");
    if ([imageFormat isEqualToString:@"bmp"]) {
        const uint64_t stride = (uint64_t(w) + 3u) & ~uint64_t(3u);
        const uint64_t imageBytes = stride * uint64_t(h);
        const uint64_t offset = 14u + 40u + 256u*4u;
        if (imageBytes + offset > UINT32_MAX) throw std::runtime_error("BMP image exceeds Windows BMP size limit");
        std::vector<uint8_t> bmp;
        bmp.reserve(size_t(offset + imageBytes));
        bmp.push_back('B'); bmp.push_back('M');
        writeLE32(bmp, uint32_t(offset + imageBytes));
        writeLE16(bmp, 0); writeLE16(bmp, 0);
        writeLE32(bmp, uint32_t(offset));
        writeLE32(bmp, 40); writeLE32(bmp, uint32_t(w)); writeLE32(bmp, uint32_t(h));
        writeLE16(bmp, 1); writeLE16(bmp, 8);
        writeLE32(bmp, 0); writeLE32(bmp, uint32_t(imageBytes));
        writeLE32(bmp, 2835); writeLE32(bmp, 2835);
        writeLE32(bmp, 256); writeLE32(bmp, 0);
        for (int i=0;i<256;++i) {
            bmp.push_back(uint8_t(i)); bmp.push_back(uint8_t(i));
            bmp.push_back(uint8_t(i)); bmp.push_back(0);
        }
        for (int y=h-1; y>=0; --y) {
            const size_t rowStart = bmp.size();
            const uint8_t *src = page.pixels.data() + size_t(y)*size_t(w);
            for (int x=0;x<w;++x) bmp.push_back(src[x] < 128 ? 0 : 255);
            bmp.resize(rowStart + size_t(stride), 0);
        }
        NSData *data=[NSData dataWithBytes:bmp.data() length:bmp.size()];
        if (![data writeToFile:destination atomically:YES]) throw std::runtime_error("Unable to save BMP image");
    } else if ([imageFormat isEqualToString:@"png"]) {
        NSBitmapImageRep *bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:1 hasAlpha:NO isPlanar:NO colorSpaceName:NSCalibratedWhiteColorSpace bytesPerRow:w bitsPerPixel:8];
        if (!bitmap || !bitmap.bitmapData) throw std::runtime_error("Unable to allocate PNG bitmap");
        for(int y=0;y<h;++y) {
            uint8_t *dst = bitmap.bitmapData + y*bitmap.bytesPerRow;
            const uint8_t *src = page.pixels.data()+size_t(y)*size_t(w);
            for(int x=0;x<w;++x) dst[x] = src[x] < 128 ? 0 : 255;
        }
        NSData *png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (!png || ![png writeToFile:destination atomically:YES]) throw std::runtime_error("Unable to save PNG image");
    } else throw std::runtime_error("Unsupported image format");
}

+ (BOOL)createPNG:(NSString *)source destination:(NSString *)destination columns:(NSInteger)columns dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel imageFormat:(NSString *)imageFormat error:(NSError **)error {
    try {
        PreparedInput prepared(std::string(source.fileSystemRepresentation),std::string(compression.UTF8String ?: "original"),int(lzmaLevel));
        auto page=paperbak::encodeFile(prepared.path,int(columns),int(dotPercent),prepared.nativeCompressed,prepared.originalSize);
        savePaperBakImage(page, destination, imageFormat);
        return YES;
    } catch(const std::exception &e) { if(error) *error=errorFor(e); return NO; }
}

+ (BOOL)createEncryptedPNG:(NSString *)source destination:(NSString *)destination columns:(NSInteger)columns rows:(NSInteger)rows recoveryGroupSize:(NSInteger)recoveryGroupSize password:(NSString *)password dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel imageFormat:(NSString *)imageFormat error:(NSError **)error {
    try {
        if (password.length == 0) throw std::runtime_error("A password is required");
        if (columns < 1 || columns > 100 || rows < 1 || rows > 100) throw std::runtime_error("Columns and Rows must each be between 1 and 100");
        PreparedInput prepared(std::string(source.fileSystemRepresentation),std::string(compression.UTF8String ?: "original"),int(lzmaLevel));
        const auto size = std::filesystem::file_size(prepared.path);
        const int r = int(recoveryGroupSize);
        if (r < 1 || r > 15) throw std::runtime_error("Invalid recovery group size");
        const uint64_t aligned = ((size + 15) / 16) * 16;
        const uint64_t dataBlocks = (aligned + 89) / 90;
        const uint64_t groups = (dataBlocks + r - 1) / r;
        const uint64_t cells = (groups + 1) * (r + 1);
        if (uint64_t(columns) * uint64_t(rows) < cells)
            throw std::runtime_error("Selected Columns × Rows grid is too small for this encrypted backup. Increase the dimensions or enable Multi-page PNG.");
        auto pages = paperbak::encodePages(prepared.path,int(columns),int(rows),1,r,std::string(password.UTF8String ?: ""),int(dotPercent),prepared.nativeCompressed,prepared.originalSize);
        if (pages.size() != 1) throw std::runtime_error("Expected one encrypted PNG");
        const auto &page=pages.front();
        savePaperBakImage(page, destination, imageFormat);
        return YES;
    } catch(const std::exception &e) { if(error) *error=errorFor(e); return NO; }
}

+ (BOOL)createMultiPagePNGs:(NSString *)source destinationFolder:(NSString *)folder columns:(NSInteger)columns rows:(NSInteger)rows pages:(NSInteger)pages recoveryGroupSize:(NSInteger)recoveryGroupSize password:(NSString *)password dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel imageFormat:(NSString *)imageFormat error:(NSError **)error {
    try {
        PreparedInput prepared(std::string(source.fileSystemRepresentation),std::string(compression.UTF8String ?: "original"),int(lzmaLevel));
        auto images=paperbak::encodePages(prepared.path,int(columns),int(rows),int(pages),int(recoveryGroupSize),std::string(password.UTF8String ?: ""),int(dotPercent),prepared.nativeCompressed,prepared.originalSize);
        NSString *stem=[[source lastPathComponent] stringByDeletingPathExtension];
        if (stem.length == 0) stem=@"PaperBak";
        NSFileManager *fm=[NSFileManager defaultManager];
        for(size_t i=0;i<images.size();i++) {
            NSString *name=[NSString stringWithFormat:@"%@_%04lu.%@",stem,(unsigned long)i+1,imageFormat];
            NSString *path=[folder stringByAppendingPathComponent:name];
            if([fm fileExistsAtPath:path]) throw std::runtime_error("Output already exists; choose an empty folder to avoid overwriting pages");
        }
        for(size_t i=0;i<images.size();i++) {
            const auto &page=images[i];
            NSString *name=[NSString stringWithFormat:@"%@_%04lu.%@",stem,(unsigned long)i+1,imageFormat];
            NSString *path=[folder stringByAppendingPathComponent:name];
            savePaperBakImage(page, path, imageFormat);
        }
        return YES;
    } catch(const std::exception &e) { if(error) *error=errorFor(e); return NO; }
}
@end
