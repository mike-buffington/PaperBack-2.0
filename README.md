# PaperBack 2.0 for macOS
PaperBack is a native macOS application for encoding files as printable PaperBak backup images and restoring files from PNG or BMP backup images. Uses the source code from Oleh Yuschuk's **PaperBack** (https://www.ollydbg.de/Paperbak/index.html).

## New Features:
- Improved interface focusing on generating backups with ease and with more user controls
- Encode files as PaperBak-compatible PNG backup images
- Decode PNG and BMP backup images
- Single-page and multi-page backup layout options
- Paper size-based or manually selected columns and rows
- Recovery-block configuration
- Password-protected backups compatible with the original PaperBak encryption format
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

The app is intended to support the original PaperBak format. Test backup restoration before relying on printed or stored backup images. Original-format encryption is for compatibility and should not be represented as a modern independently audited security design.

A locally built application may be unsigned. Downloaded unsigned applications may prompt macOS Gatekeeper warnings. Signing and notarization are recommended for distribution.

## Attribution and licensing

This project incorporates code derived from the original PaperBak implementation and includes an ECC implementation bearing a GPL notice. **Before redistributing the source or application beyond your own use, review the original component licenses and their requirements.** No new blanket license is asserted for third-party source code in this package.

## Screenshots and generated files

<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 2 36 24 PM" src="https://github.com/user-attachments/assets/296208ac-dcef-4cb4-8dc5-e65e6bb1917e" />
<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 2 37 01 PM" src="https://github.com/user-attachments/assets/376549c5-fc99-4611-9ddc-3a98cb32b189" />
<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 2 38 17 PM" src="https://github.com/user-attachments/assets/699079ed-39fe-477a-a686-af32bfb9638a" />
<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 2 39 23 PM" src="https://github.com/user-attachments/assets/4c718f44-8974-4b71-8823-01ddc71fa261" />


<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0001" src="https://github.com/user-attachments/assets/31f62215-5f50-4da3-ac2b-5c91e6caab5b" />
<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0002" src="https://github.com/user-attachments/assets/afd55f89-5521-4dd1-b586-eb6f12654627" />
<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0003" src="https://github.com/user-attachments/assets/facdb397-7efc-47ba-aa39-7c56b459d4ef" />
