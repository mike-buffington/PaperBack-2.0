# PaperBack 2.0 for macOS
PaperBack is a native macOS application for encoding files as printable PaperBack backup images and restoring files from PNG or BMP backup images. Uses the source code from Oleh Yuschuk's **PaperBack** (https://www.ollydbg.de/Paperbak/index.html).

## New Features:
- Improved interface focusing on generating backups with ease and with more user controls
- Encode files as PaperBack-compatible PNG backup images or original format BMP images
- Decode PNG and BMP backup images
- Single-page and multi-page backup layout options
- Paper size-based or manually selected columns and rows
- Recovery-block configuration
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

<img width="946" height="1225" alt="Screenshot 2026-10-24 at 12 40 04 PM" src="https://github.com/user-attachments/assets/5ee72235-62dc-4c23-8c08-c7ebdff07514" />
Generate a grid of PaperBack blocks from the Column x Rows menu. Options for dot size, compression (with 7zip option), encryption, and PNG or BMP images.

<img width="946" height="1225" alt="Screenshot 2026-10-24 at 12 40 50 PM" src="https://github.com/user-attachments/assets/960dcb1d-ffa0-4bc8-9964-9d0f7136d262" />
Multiple page options include page number limit, page size, and margins.

<img width="946" height="1225" alt="Screenshot 2026-10-24 at 12 41 18 PM" src="https://github.com/user-attachments/assets/9dd602e0-0b4b-4735-977e-702724db8db6" />
Options to customize the columns and rows for multiple pages. 

<img width="946" height="1225" alt="Screenshot 2026-10-24 at 12 42 32 PM" src="https://github.com/user-attachments/assets/ba8e43c7-0d61-4c28-b1ef-50754db786f7" />
Decoding can select multiple pages at once. Metadata in the files is shown and quality maps are generated.

This is the app encoded, and can be decoded with the original Windows application. There is no compression and no password, but the images are PNG files. Convert to BMP for the Windows application.
<img width="5184" height="6864" alt="PaperBack-2 0-macOS_0001" src="https://github.com/user-attachments/assets/03b19bb3-fcbd-4119-a579-51b4abb13fd6" />
<img width="5184" height="6864" alt="PaperBack-2 0-macOS_0002" src="https://github.com/user-attachments/assets/1d9e4f87-3cb7-4a1f-b283-22d5dc089c0d" />
<img width="5184" height="6584" alt="PaperBack-2 0-macOS_0003" src="https://github.com/user-attachments/assets/b7448e6d-71b0-4f29-bb4e-f47e49a6d652" />

