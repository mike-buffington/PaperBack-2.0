cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version" FORCE)
project(PaperBack LANGUAGES CXX Swift OBJCXX)

add_executable(PaperBack MACOSX_BUNDLE
    app/PaperBackApp.swift
    app/PaperBack.icns
    app/PaperBakBridge.mm
    core/Ecc.cpp core/Crc16.cpp core/paperbak_block.cpp
    core/paperbak_restore.cpp core/bmp_loader.cpp core/paperbak_decoder.cpp core/paperbak_encode.cpp
    core/paperbak_lzma.cpp core/paperbak_crypto.cpp
)
set_source_files_properties(app/PaperBack.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
target_include_directories(PaperBack PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/core")
set_target_properties(PaperBack PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.paperback.macos"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.paperback.macos"
    MACOSX_BUNDLE_BUNDLE_NAME "PaperBack"
    MACOSX_BUNDLE_ICON_FILE "PaperBack.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/app/PaperBakBridge.h"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "13.0"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17"
)
find_package(BZip2 REQUIRED)
target_link_libraries(PaperBack PRIVATE BZip2::BZip2 "-framework Foundation" "-framework AppKit" "-framework Security" "-framework SwiftUI")

# liblzma is linked statically so the shipped .app does not require Homebrew.
# Install the XZ library on the build Mac with: brew install xz
find_path(LZMA_INCLUDE_DIR lzma.h HINTS /opt/homebrew/opt/xz/include /usr/local/opt/xz/include)
find_file(LZMA_STATIC_LIBRARY NAMES liblzma.a HINTS /opt/homebrew/opt/xz/lib /usr/local/opt/xz/lib)
if(NOT LZMA_INCLUDE_DIR OR NOT LZMA_STATIC_LIBRARY)
    message(FATAL_ERROR "liblzma static library not found. Run 'brew install xz' and configure again.")
endif()
target_include_directories(PaperBack PRIVATE "${LZMA_INCLUDE_DIR}")
target_link_libraries(PaperBack PRIVATE "${LZMA_STATIC_LIBRARY}")
