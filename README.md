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

Requirements: macOS 26 or later, Xcode, CMake, and BZip2 development support.

```bash
cmake -S . -B xcode -G Xcode
open xcode/PaperBack.xcodeproj
```

In Xcode, choose the **PaperBack** scheme, destination **My Mac**, then **Product → Build**. Locate `PaperBack.app` in Xcode's **Products** group using **Show in Finder**. Copy the application to `/Applications` to install it.

## Downloading the app

Download the macOS application ZIP from the repository's **Releases** page.

## Compatibility and security

The app is intended to support the original PaperBak format. Test backup restoration before relying on printed or stored backup images. Original-format encryption is for compatibility and should not be represented as a modern independently audited security design.

A locally built application may be unsigned. Downloaded unsigned applications may prompt macOS Gatekeeper warnings. Signing and notarization are recommended for distribution.

## Attribution and licensing

This project incorporates code derived from the original PaperBak implementation and includes an ECC implementation bearing a GPL notice. **Before redistributing the source or application beyond your own use, review the original component licenses and their requirements.** No new blanket license is asserted for third-party source code in this package.

## Screenshots and generated files

<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 1 44 39 PM" src="https://github.com/user-attachments/assets/0cf92150-1d81-42d3-bd6d-d481cf376618" />
<img width="1304" height="1152" alt="Screenshot 2026-10-22 at 2 12 44 PM" src="https://github.com/user-attachments/assets/ce7bed88-076d-4aed-8885-36d68aca5da0" />
<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 1 42 36 PM" src="https://github.com/user-attachments/assets/85f5b70f-a70e-4656-b4ef-df876f66ea50" />
<img width="1348" height="1196" alt="Screenshot 2026-10-22 at 1 58 24 PM" src="https://github.com/user-attachments/assets/4abe493b-fab0-4b6c-b73c-bcef43489e6e" />

<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0001" src="https://github.com/user-attachments/assets/31f62215-5f50-4da3-ac2b-5c91e6caab5b" />
<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0002" src="https://github.com/user-attachments/assets/afd55f89-5521-4dd1-b586-eb6f12654627" />
<img width="6164" height="7844" alt="PaperBack-2 0-macOS_0003" src="https://github.com/user-attachments/assets/facdb397-7efc-47ba-aa39-7c56b459d4ef" />
