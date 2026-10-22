# PaperBack 2.0 for macOS

PaperBack is a native macOS application for encoding files as printable PaperBak backup images and restoring files from PNG or BMP backup images.

Based on **Oleh Yuschuk's PaperBack**, [original PaperBak website](https://www.ollydbg.de/Paperbak/index.html). Ported to macOS by **Mike Buffington**.

## Features

- Encode files as PaperBak-compatible PNG backup images
- Decode supported PNG and BMP backup images
- Single-page and multi-page backup layouts
- Paper-based or manually selected columns and rows
- Recovery-block configuration
- Password-protected backups compatible with the original PaperBak encryption format

## Build on macOS

Requirements: macOS 13 or later, Xcode, CMake, and BZip2 development support.

```bash
cmake -S . -B xcode -G Xcode
open xcode/PaperBack.xcodeproj
```

In Xcode, choose the **PaperBack** scheme, destination **My Mac**, then **Product → Build**. Locate `PaperBack.app` in Xcode's **Products** group using **Show in Finder**. Copy the application to `/Applications` to install it.

## Downloading the app

Authorized collaborators can download the application ZIP from the repository's **Releases** page. The macOS application is built on a Mac and is **not included** in the source repository.

## Compatibility and security

The app is intended to support the original PaperBak format. Test backup restoration before relying on printed or stored backup images. Original-format encryption is for compatibility and should not be represented as a modern independently audited security design.

A locally built application may be unsigned. Downloaded unsigned applications may prompt macOS Gatekeeper warnings. Signing and notarization are recommended for distribution.

## Attribution and licensing

This project incorporates code derived from the original PaperBak implementation and includes an ECC implementation bearing a GPL notice. **Before redistributing the source or application beyond your own use, review the original component licenses and their requirements.** No new blanket license is asserted for third-party source code in this package.
