# PaperBack 2.0 for macOS
PaperBack is a native macOS application for encoding files as printable PaperBack backup images and restoring files from PNG or BMP backup images. Uses the source code from Oleh Yuschuk's **PaperBack** (https://www.ollydbg.de/Paperbak/index.html).

## New Features:
- Improved interface focusing on generating backups with ease and with more user controls
- Encode files as PaperBack-compatible PNG backup images or original format BMP images
- Decode PNG and BMP backup images
- Single-page and multi-page backup layout options
- Paper size-based or manually selected columns and rows
- Select multiple files at once for decoding
- Password-protected backups compatible with the original PaperBack encryption format
- Added 7zip compression

## Requirements: macOS 26 or later

Important: PaperBack is not currently signed with an Apple Developer ID or notarized by Apple. If macOS blocks the app, open System Settings → Privacy & Security, scroll to the Security section, and select Open Anyway after attempting to launch it.

PaperBack is provided as-is. Please report any problems through GitHub Issues.

## Build on macOS

Requirements: macOS 26 or later, Xcode, CMake, and BZip2 development support.

```bash
cmake -S . -B xcode -G Xcode
open xcode/PaperBack.xcodeproj
```

In Xcode, choose the **PaperBack** scheme, destination **My Mac**, then **Product → Build**. Locate `PaperBack.app` in Xcode's **Products** group using **Show in Finder**. Copy the application to `/Applications` to install it.

## Compatibility and security

The app is intended to support the original PaperBack format. Test backup restoration before relying on printed or stored backup images. Original-format encryption is for compatibility and should not be represented as a modern independently audited security design.

A locally built application may be unsigned. Downloaded unsigned applications may prompt macOS Gatekeeper warnings. Signing and notarization are recommended for distribution.

## Attribution and licensing

This project incorporates code derived from the original PaperBack implementation and includes an ECC implementation bearing a GPL notice. **Before redistributing the source or application beyond your own use, review the original component licenses and their requirements.** No new blanket license is asserted for third-party source code in this package.

## Screenshots and generated files

Generate a grid of PaperBack blocks from the Column x Rows menu. Options for recovery group size, dot size, compression (with 7zip option), encryption, and PNG or BMP images:
<img width="872" height="1211" alt="Screenshot 2026-10-29 at 6 57 18 PM" src="https://github.com/user-attachments/assets/47d0ce06-3206-4207-b26b-df0c3afc39a4" />
<br>
<br>
<br>
Multiple page options include page number limit, page size, and margins:
<img width="872" height="1211" alt="Screenshot 2026-10-29 at 6 56 04 PM" src="https://github.com/user-attachments/assets/27b5c585-3077-47ac-be93-118677cdc955" />
<br>
<br>
<br>
Options to customize the columns and rows for multiple pages:
<img width="872" height="1211" alt="Screenshot 2026-10-29 at 6 56 19 PM" src="https://github.com/user-attachments/assets/dd8a4aab-b931-4185-b2b5-f84f18d99b8d" />
<br>
<br>
<br>
Decoding can select multiple pages at once. Metadata in the files is shown and quality maps are generated:
<img width="872" height="1211" alt="Screenshot 2026-10-29 at 6 56 27 PM" src="https://github.com/user-attachments/assets/5386fe25-bc36-4bcf-b7f6-8f82518d1be8" />
<br>
<br>
<br>
This is the app encoded, and can be decoded with the original Windows application. There is no compression and no password, but the images are PNG files. Convert to BMP for the Windows application.
<img width="5884" height="7704" alt="PaperBack-2 0-macOS_0001" src="https://github.com/user-attachments/assets/ce00d55b-035b-49d8-a215-db71d46f7213" />
<img width="5884" height="7704" alt="PaperBack-2 0-macOS_0002" src="https://github.com/user-attachments/assets/95d4ba62-a102-4e4c-b2a8-4f10d35aaad0" />
<img width="5884" height="7704" alt="PaperBack-2 0-macOS_0003" src="https://github.com/user-attachments/assets/0e7f52f1-67cd-4631-8c39-5550f9a68869" />
