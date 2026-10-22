import SwiftUI
import AppKit
import UniformTypeIdentifiers

@main
struct PaperBackApp: App {
    var body: some Scene {
        WindowGroup("PaperBack") { ContentView() }
            .defaultSize(width: 800, height: 780)
        .commands {
            CommandGroup(replacing: .appInfo) {
                Button("About PaperBack") {
                    let urlText = "https://www.ollydbg.de/Paperbak/index.html"
                    let attribution = NSMutableAttributedString(string: "Based on Oleh Yuschuk's PaperBack \n\(urlText)")
                    let linkRange = (attribution.string as NSString).range(of: urlText)
                    attribution.addAttribute(.link, value: URL(string: urlText)!, range: linkRange)
                    NSApp.orderFrontStandardAboutPanel(options: [
                        .applicationName: "PaperBack",
                        .applicationVersion: "2.0",
                        .version: "2.0",
                        .credits: attribution
                    ])
                }
            }
        }
    }
}

struct ContentView: View {
    @State private var selectedURLs: [URL] = []
    @State private var info: [String: Any]?
    @State private var status = ""
    @State private var working = false
    @State private var activeArea: Area? = nil
    @State private var restorePassword = ""

    private enum Area { case encode, decode }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            VStack(alignment: .leading, spacing: 14) {
                Text("PaperBack").font(.largeTitle.bold())
                HStack(spacing: 12) {
                    Button("Encode PNG Backup from File") { activeArea = .encode }
                        .disabled(working)
                    Button("Decode File from PNG or BMP Backup") {
                        activeArea = .decode
                    }
                    .disabled(working)
                }
            }
            .padding(.horizontal, 26)
            .padding(.top, 24)
            .padding(.bottom, 18)

            Divider()

            if activeArea == .encode {
                // Keep the encoder below the divider. Its action footer is
                // outside the scrolling controls, so Generate PNG stays visible.
                BackupLayoutView()
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            } else if activeArea == .decode {
                ScrollView {
                    VStack(alignment: .leading, spacing: 14) {
                        Text("Decode File from PaperBack Backup")
                            .font(.title2.bold())
                        HStack(alignment: .firstTextBaseline, spacing: 12) {
                            Button("Choose File…") { openPage() }
                                .disabled(working)
                            if !selectedURLs.isEmpty {
                                Text(selectedURLs.count == 1 ? selectedURLs[0].lastPathComponent : "\(selectedURLs.count) selected pages")
                                    .lineLimit(1)
                                    .truncationMode(.middle)
                                    .textSelection(.enabled)
                            }
                        }
                        if !selectedURLs.isEmpty {
                            if let info {
                                LabeledContent("Backup file", value: info["name"] as? String ?? "Unknown")
                                LabeledContent("Original size", value: "\(info["size"] ?? 0) bytes")
                                LabeledContent("Pages selected", value: "\(info["pages"] ?? 0)")
                                LabeledContent("Page numbers", value: (info["pageNumbers"] as? [Int] ?? []).sorted().map(String.init).joined(separator: ", "))
                                LabeledContent("Readable blocks", value: "\(info["blocks"] ?? 0)")
                                if let qualityPages = info["qualityPages"] as? [[String: Any]], !qualityPages.isEmpty {
                                    Text("Quality Map").font(.headline)
                                    HStack(spacing: 12) {
                                        qualityLegendItem("Clean", color: Color(red: 124/255, green: 197/255, blue: 118/255))
                                        qualityLegendItem("Corrected", color: Color(red: 251/255, green: 175/255, blue: 93/255))
                                        qualityLegendItem("Heavily corrected", color: Color(red: 242/255, green: 108/255, blue: 79/255))
                                        qualityLegendItem("Unreadable", color: .black)
                                    }
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                    QualityMapPreview(pages: qualityPages)
                                        .frame(height: 230)
                                }
                                if info["encrypted"] as? Bool == true {
                                    SecureField("Password", text: $restorePassword)
                                        .textFieldStyle(.roundedBorder)
                                        .frame(maxWidth: 320)
                                }
                                Button("Restore File…", action: restoreFile)
                                    .disabled(working || ((info["encrypted"] as? Bool == true) && restorePassword.isEmpty))
                                    .buttonStyle(.borderedProminent)
                            }
                        }
                        if !status.isEmpty {
                            Text(status).textSelection(.enabled).foregroundStyle(.secondary)
                        }
                    }
                    .padding(26)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
            } else {
                Spacer(minLength: 0)
            }
        }
        .frame(minWidth: 650, minHeight: 480)
    }

    private func openPage() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.bmp, .png]
        panel.allowsMultipleSelection = true
        guard panel.runModal() == .OK, !panel.urls.isEmpty else { return }
        let urls = panel.urls
        selectedURLs = urls
        restorePassword = ""
        info = nil
        working = true
        status = "Reading PaperBack blocks…"
        // Keep the UI responsive while the original C++ decoder scans the image.
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let result = try PaperBakBridge.inspectPages(urls.map { $0.path })
                DispatchQueue.main.async {
                    info = result
                    status = "\(urls.count) \(urls.count == 1 ? "page" : "pages") recognized successfully."
                    working = false
                }
            } catch {
                DispatchQueue.main.async {
                    info = nil
                    status = error.localizedDescription
                    working = false
                }
            }
        }
    }

    private func restoreFile() {
        guard !selectedURLs.isEmpty, let info else { return }
        let urls = selectedURLs
        let password = restorePassword
        let panel = NSSavePanel()
        panel.nameFieldStringValue = (info["name"] as? String ?? "restored-file").components(separatedBy: "/").last ?? "restored-file"
        guard panel.runModal() == .OK, let destination = panel.url else { return }
        working = true
        status = "Restoring…"
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try PaperBakBridge.restorePages(urls.map { $0.path }, to: destination.path, password: password)
                let message = "Restored successfully: \(destination.lastPathComponent)"
                DispatchQueue.main.async { status = message; working = false }
            } catch {
                DispatchQueue.main.async {
                    status = "Restore failed: \(error.localizedDescription)"
                    working = false
                }
            }
        }
    }
}


// Create a PaperBak PNG backup using the selected compression method.
struct BackupLayoutView: View {
    @State private var source: URL?
    @State private var count = 0
    @State private var encodedSourceBytes: Int64 = 0
    @State private var columns = 0
    @State private var message = ""
    @State private var busy = false
    @State private var fourPageMode = false
    @State private var recoveryGroupSize = 2
    @State private var maximumPages = 4
    @State private var layoutMode = "paper"
    @State private var units = "in"
    @State private var unitConversionInProgress = false
    @State private var paperWidth = 8.5
    @State private var paperHeight = 11.0
    @State private var marginLeft = 0.25
    @State private var marginRight = 0.25
    @State private var marginTop = 0.25
    @State private var marginBottom = 0.25
    @State private var manualColumns = 23
    @State private var manualRows = 30
    @State private var dotPercent = 100
    @State private var compressionMethod = "original"
    @State private var lzmaLevel = 5
    @State private var encryptBackup = false
    @State private var backupPassword = ""
    @State private var confirmPassword = ""

    private var proposedGrid: (x: Int, y: Int, pages: Int, blockSize: Double)? {
        guard source != nil, count > 0 else { return nil }
        let bytes = Int(encodedSourceBytes)
        guard bytes > 0 else { return nil }
        let dataBlocks = (bytes + 89) / 90
        let groups = (dataBlocks + recoveryGroupSize - 1) / recoveryGroupSize
        let r = recoveryGroupSize
        func capacity(_ x: Int, _ y: Int) -> Int { (x * y - (r + 1)) / (r + 1) }
        func pageCount(_ x: Int, _ y: Int) -> Int {
            let c = capacity(x, y)
            return c > 0 ? (groups + c - 1) / c : Int.max
        }
        if layoutMode == "grid" {
            let x = manualColumns, y = manualRows
            let n = pageCount(x, y)
            return n <= maximumPages ? (x, y, n, 0) : nil
        }
        let scale = units == "mm" ? 1.0 : 25.4
        let usableW = (paperWidth - marginLeft - marginRight) * scale
        let usableH = (paperHeight - marginTop - marginBottom) * scale
        guard usableW > 0, usableH > 0 else { return nil }
        var best: (x: Int, y: Int, pages: Int, blockSize: Double)?
        // A cell is square. Maximize its physical size, with X favored on ties.
        // Restrict image dimensions to the existing encoder's safe pixel budget.
        for x in 3...100 {
            for y in 3...100 {
                if x * y > 6000 || Double(x * 140 + 4) * Double(y * 140 + 4) > 150_000_000 { continue }
                let n = pageCount(x, y)
                if n > maximumPages { continue }
                let size = min(usableW / Double(x), usableH / Double(y))
                if let b = best {
                    if size > b.blockSize + 0.000001 ||
                       (abs(size - b.blockSize) < 0.000001 && x > b.x) {
                        best = (x, y, n, size)
                    }
                } else { best = (x, y, n, size) }
            }
        }
        return best
    }

    // The original encrypted format pads to AES's 16-byte boundary and
    // stores one extra recovery/metadata group. Use that count to choose
    // a fresh, compact near-square grid when encryption is toggled.
    private var encryptedRequiredCells: Int {
        guard source != nil else { return 0 }
        // Account for the selected compression method when estimating layout.
        let fileBytes = encodedSourceBytes
        guard fileBytes > 0 else { return 0 }
        let padded = ((fileBytes - 1) / 16 + 1) * 16
        let dataBlocks = (padded - 1) / 90 + 1
        let groupSize = Int64(recoveryGroupSize)
        let groups = (dataBlocks - 1) / groupSize + 1
        // No single-page grid can contain more than 100 × 100 cells.
        // Return a sentinel rather than overflowing the multiplication.
        guard groups <= 10000 / (groupSize + 1) else { return 10001 }
        return Int((groups + 1) * (groupSize + 1))
    }

    // The single-page encrypted encoder accepts a grid with spare cells.
    // Select the smallest total cell count at or above the encrypted block
    // requirement that has usable factor pairs, then offer every orientation.
    private var encryptedGridCells: Int {
        let required = encryptedRequiredCells
        // Swift traps when a closed range starts above its end (e.g. 12000...10000).
        // Oversized encrypted files should offer Multi-page PNG, not crash.
        guard required > 0 && required <= 10000 else { return 0 }
        for total in required...10000 {
            let pairs = (3...100).filter { x in
                total % x == 0 && total / x >= 3 && total / x <= 100 &&
                (x * 140 + 4) * ((total / x) * 140 + 4) <= 150_000_000
            }
            if pairs.count >= 2 { return total }
        }
        return 0
    }

    private var encryptedChoices: [Int] {
        let total = encryptedGridCells
        guard total > 0 else { return [] }
        return (3...100).filter { x in
            total % x == 0 && total / x >= 3 && total / x <= 100 &&
            (x * 140 + 4) * ((total / x) * 140 + 4) <= 150_000_000
        }
    }

    private func chooseEncryptedGrid() {
        let total = encryptedGridCells
        let options = encryptedChoices
        columns = options.min { a, b in
            let differenceA = abs(a - total / a)
            let differenceB = abs(b - total / b)
            return differenceA < differenceB
        } ?? 0
    }

    private var estimatedDataBlocks: Int {
        guard let source,
              let attributes = try? FileManager.default.attributesOfItem(atPath: source.path),
              let bytes = attributes[.size] as? NSNumber else { return 0 }
        return (bytes.intValue + 89) / 90
    }

    private func convertUnits(to newUnits: String) {
        // The stored values represent the previously selected unit.
        // onChange fires only when the unit selection actually changes.
        let factor = newUnits == "mm" ? 25.4 : 1.0 / 25.4
        paperWidth *= factor
        paperHeight *= factor
        marginLeft *= factor
        marginRight *= factor
        marginTop *= factor
        marginBottom *= factor
    }

    private var choices: [Int] {
        guard count > 0 else { return [] }
        if encryptBackup { return encryptedChoices }
        return (1...count).filter { count % $0 == 0 }
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
            Text("Encode PaperBack PNG Backup").font(.title2.bold())
            Text("Select a file, choose a grid, and save a PNG backup.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            HStack(alignment: .firstTextBaseline, spacing: 12) {
                Button("Choose File…") { chooseFile() }.disabled(busy)
                if let source {
                    Text(source.lastPathComponent)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .textSelection(.enabled)
                }
            }
            if let _ = source {
                Text("\(encryptBackup ? encryptedRequiredCells : count) PaperBack blocks, including metadata and recovery blocks")
            }
            if source != nil && !fourPageMode {
                Picker("Columns × Rows", selection: $columns) {
                    ForEach(choices, id: \.self) { x in
                        let cells = encryptBackup ? encryptedGridCells : count
                        Text("\(x) × \(cells / x)").tag(x)
                    }
                }
                .pickerStyle(.menu)
                .disabled(busy || choices.isEmpty)
                if encryptBackup && choices.isEmpty {
                    Text("This file is too large for a single encrypted PNG. Enable Multi-page PNG to split it into pages.")
                        .foregroundStyle(.orange)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            if fourPageMode {
                Picker("Layout mode", selection: $layoutMode) {
                    Text("Paper Layout (automatic)").tag("paper")
                    Text("Columns and Rows (Manual)").tag("grid")
                }.pickerStyle(.segmented).disabled(busy)
                HStack(spacing: 10) {
                    Text("Maximum pages")
                    TextField("Pages", value: $maximumPages, format: .number)
                        .frame(width: 64)
                        .textFieldStyle(.roundedBorder)
                        .multilineTextAlignment(.trailing)
                        .disabled(busy)
                        .onChange(of: maximumPages) { value in
                            maximumPages = min(100, max(1, value))
                        }
                    Stepper("", value: $maximumPages, in: 1...100)
                        .labelsHidden()
                        .disabled(busy)
                    Spacer()
                }
                Picker("Recovery group size", selection: $recoveryGroupSize) {
                    ForEach(1...15, id: \.self) { Text("\($0)").tag($0) }
                }.disabled(busy)
                if layoutMode == "paper" {
                    Picker("Units", selection: $units) {
                        Text("Inches").tag("in")
                        Text("Millimeters").tag("mm")
                    }.pickerStyle(.segmented).disabled(busy)
                    .onChange(of: units) { newUnits in
                        convertUnits(to: newUnits)
                    }
                    HStack {
                        LabeledContent("Paper width") { TextField("Width", value: $paperWidth, format: .number).frame(width: 90) }
                        LabeledContent("Paper height") { TextField("Height", value: $paperHeight, format: .number).frame(width: 90) }
                    }
                    HStack {
                        LabeledContent("Left margin") { TextField("Left", value: $marginLeft, format: .number).frame(width: 75) }
                        LabeledContent("Right margin") { TextField("Right", value: $marginRight, format: .number).frame(width: 75) }
                    }
                    HStack {
                        LabeledContent("Top margin") { TextField("Top", value: $marginTop, format: .number).frame(width: 75) }
                        LabeledContent("Bottom margin") { TextField("Bottom", value: $marginBottom, format: .number).frame(width: 75) }
                    }
                    Text("All dimensions use the selected units. PNG files remain borderless; margins determine the usable paper area.")
                        .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                } else {
                    NumberStepperField(label: "Columns", value: $manualColumns, range: 3...100, disabled: busy)
                    NumberStepperField(label: "Rows", value: $manualRows, range: 3...100, disabled: busy)
                }
                if source != nil {
                    if let g = proposedGrid {
                        Text("Calculated layout: \(g.x) × \(g.y), \(g.pages) page(s)")
                            .font(.headline)
                        if layoutMode == "paper" {
                            Text(String(format: "Square size on paper: %.2f mm (%.3f in)", g.blockSize, g.blockSize / 25.4))
                                .foregroundStyle(.secondary)
                        }
                        LayoutPreview(columns: g.x, rows: g.y, pages: g.pages,
                                      dataBlockCount: estimatedDataBlocks,
                                      recoveryGroupSize: recoveryGroupSize,
                                      paperMode: layoutMode == "paper",
                                      paperWidth: paperWidth, paperHeight: paperHeight,
                                      left: marginLeft, right: marginRight,
                                      top: marginTop, bottom: marginBottom)
                            .frame(height: 230)
                    } else {
                        Text("No grid fits these settings. Increase the page limit, reduce margins, or change the grid.")
                            .foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            if source != nil {
                Picker("Dot size", selection: $dotPercent) {
                    ForEach([100, 90, 80, 70, 60, 50], id: \.self) { percent in
                        Text("\(percent)%").tag(percent)
                    }
                }
                .pickerStyle(.menu)
                .disabled(busy)
                Text("Smaller dots may require a higher-quality printer or scan.")
                    .font(.caption).foregroundStyle(.secondary)
                Picker("Compression", selection: $compressionMethod) {
                    Text("None (Uncompressed)").tag("none")
                    Text("Original PaperBak (Windows compatible)").tag("original")
                    Text("7-Zip / LZMA2").tag("lzma2")
                }
                .pickerStyle(.menu)
                .disabled(busy)
                .onChange(of: compressionMethod) { _ in refreshCompressionEstimate() }
                if compressionMethod == "lzma2" {
                    Picker("Compression level", selection: $lzmaLevel) {
                        Text("Fast").tag(1)
                        Text("Normal").tag(5)
                        Text("Ultra").tag(9)
                    }
                    .pickerStyle(.menu)
                    .disabled(busy)
                    .onChange(of: lzmaLevel) { _ in refreshCompressionEstimate() }
                    Text("LZMA2 backups require PaperBack for macOS to restore; original Windows PaperBak cannot read them.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                Toggle("Encrypt backup with password", isOn: $encryptBackup).disabled(busy)
                    .onChange(of: encryptBackup) { enabled in
                        if !fourPageMode {
                            if enabled { chooseEncryptedGrid() }
                            else { chooseUnencryptedGrid() }
                        }
                    }
                if encryptBackup {
                    SecureField("Password", text: $backupPassword).textFieldStyle(.roundedBorder).frame(maxWidth: 320)
                    SecureField("Confirm password", text: $confirmPassword).textFieldStyle(.roundedBorder).frame(maxWidth: 320)
                    if !backupPassword.isEmpty && backupPassword != confirmPassword {
                        Text("Passwords do not match").foregroundStyle(.orange)
                    }
                }
                Toggle("Multi-page PNG", isOn: $fourPageMode).disabled(busy)
                if fourPageMode { Text("Full pages use the same grid; the last page may be shorter.") }
                Text("Each square is a complete 32 × 32 encoded PaperBack block. Extreme layouts may not be readable by the original decoder.")
                    .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
                }
                .padding(.horizontal, 26)
                .padding(.vertical, 18)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            VStack(alignment: .leading, spacing: 8) {
                Button("Generate PNG…") { generate() }
                    .buttonStyle(.plain)
                    .font(.body.weight(.semibold))
                    .foregroundStyle(.white)
                    .padding(.horizontal, 18)
                    .padding(.vertical, 9)
                    .background(Color.blue, in: RoundedRectangle(cornerRadius: 8))
                    .disabled(source == nil || (!fourPageMode && columns == 0) || (fourPageMode && proposedGrid == nil) || busy || (encryptBackup && (backupPassword.isEmpty || backupPassword != confirmPassword)))
                    .opacity(source == nil || (!fourPageMode && columns == 0) || (fourPageMode && proposedGrid == nil) || busy || (encryptBackup && (backupPassword.isEmpty || backupPassword != confirmPassword)) ? 0.45 : 1)
                if !message.isEmpty {
                    Text(message)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                        .textSelection(.enabled)
                }
            }
            .padding(.horizontal, 26)
            .padding(.vertical, 14)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
    private func chooseUnencryptedGrid() {
        let n = count
        guard n > 0 else { columns = 0; return }
        let options = (1...n).filter { n % $0 == 0 && $0 >= 3 && n / $0 >= 3 }
        columns = options.min { a, b in
            abs(a - n / a) < abs(b - n / b)
        } ?? 0
    }

    private func chooseFile() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            var bridgeError: NSError?
            let n = PaperBakBridge.requiredCells(url.path, error: &bridgeError)
            if let bridgeError { throw bridgeError }
            guard n > 0 else { throw NSError(domain: "PaperBak", code: 1) }
            source = url
            count = n
            encodedSourceBytes = (try? FileManager.default.attributesOfItem(atPath: url.path)[.size] as? NSNumber)?.int64Value ?? 0
            // Recalculate for all modes: BZip2 and LZMA2 change the
            // encoded size, while None uses the original source size.
            refreshCompressionEstimate()
        } catch { message = error.localizedDescription }
    }
    private func refreshCompressionEstimate() {
        guard let source else { return }
        busy = true
        message = "Calculating compressed backup size…"
        let method = compressionMethod
        let level = lzmaLevel
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let result = try PaperBakBridge.sourceLayout(source.path, compression: method, lzmaLevel: level)
                guard let cells = result["cells"]?.intValue,
                      let bytes = result["bytes"] else {
                    throw NSError(domain: "PaperBack", code: 1, userInfo: [NSLocalizedDescriptionKey: "Unable to calculate compressed layout"])
                }
                DispatchQueue.main.async {
                    // Discard stale results when the user changes compression during processing.
                    guard self.source == source && self.compressionMethod == method && self.lzmaLevel == level else { return }
                    count = cells
                    encodedSourceBytes = bytes.int64Value
                    if encryptBackup && !fourPageMode { chooseEncryptedGrid() }
                    else { chooseUnencryptedGrid() }
                    busy = false
                    message = ""
                }
            } catch {
                DispatchQueue.main.async { busy = false; message = error.localizedDescription }
            }
        }
    }
    private func generate() {
        guard let source else { return }
        if fourPageMode { generateFourPages(source); return }
        if encryptBackup && columns == 0 {
            message = "No single-page encrypted grid fits this file. Enable Multi-page PNG."
            return
        }
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.png]
        panel.nameFieldStringValue = source.lastPathComponent + ".png"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        busy = true
        message = "Encoding PaperBack blocks…"
        let x = columns
        let y = encryptBackup && columns > 0 ? encryptedGridCells / columns : 0
        let password = encryptBackup ? backupPassword : ""
        let selectedDotPercent = dotPercent
        let selectedCompression = compressionMethod
        let selectedLzmaLevel = lzmaLevel
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                if password.isEmpty {
                    try PaperBakBridge.createPNG(source.path, destination: url.path, columns: x, dotPercent: selectedDotPercent, compression: selectedCompression, lzmaLevel: selectedLzmaLevel)
                } else {
                    try PaperBakBridge.createEncryptedPNG(source.path, destination: url.path, columns: x, rows: y, recoveryGroupSize: recoveryGroupSize, password: password, dotPercent: selectedDotPercent, compression: selectedCompression, lzmaLevel: selectedLzmaLevel)
                }
                DispatchQueue.main.async { message = "PNG saved: \(url.lastPathComponent)"; busy = false }
            } catch {
                DispatchQueue.main.async { message = error.localizedDescription; busy = false }
            }
        }
    }
    private func generateFourPages(_ source: URL) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        panel.prompt = "Choose Output Folder"
        guard panel.runModal() == .OK, let folder = panel.url else { return }
        busy = true
        message = "Generating PaperBack PNG pages…"
        guard let grid = proposedGrid else { message = "No valid grid for these settings."; return }
        let selectedColumns = grid.x
        let selectedRows = grid.y
        let selectedRecoveryGroupSize = recoveryGroupSize
        let selectedMaximumPages = maximumPages
        let password = encryptBackup ? backupPassword : ""
        let selectedDotPercent = dotPercent
        let selectedCompression = compressionMethod
        let selectedLzmaLevel = lzmaLevel
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try PaperBakBridge.createMultiPagePNGs(source.path, destinationFolder: folder.path, columns: selectedColumns, rows: selectedRows, pages: selectedMaximumPages, recoveryGroupSize: selectedRecoveryGroupSize, password: password, dotPercent: selectedDotPercent, compression: selectedCompression, lzmaLevel: selectedLzmaLevel)
                DispatchQueue.main.async {
                    message = "PNG pages Generated and Saved in \"\(folder.path)\""
                    busy = false
                }
            } catch {
                DispatchQueue.main.async { message = "Generation failed: \(error.localizedDescription)"; busy = false }
            }
        }
    }

}


// Numeric text input commits only when Return is pressed or focus leaves.
// Keeping the draft as String prevents a first keystroke from being clamped
// immediately to the minimum (the old "3" behavior).
private struct NumberStepperField: View {
    let label: String
    @Binding var value: Int
    let range: ClosedRange<Int>
    let disabled: Bool
    @State private var draft: String = ""
    @FocusState private var editing: Bool

    private func commit() {
        if let parsed = Int(draft.trimmingCharacters(in: .whitespacesAndNewlines)) {
            value = min(range.upperBound, max(range.lowerBound, parsed))
        }
        draft = String(value)
    }

    var body: some View {
        HStack(spacing: 10) {
            Text(label)
            TextField(label, text: $draft)
                .frame(width: 64)
                .textFieldStyle(.roundedBorder)
                .multilineTextAlignment(.trailing)
                .focused($editing)
                .onSubmit { commit() }
                .onChange(of: editing) { focused in
                    if !focused { commit() }
                }
                .onChange(of: value) { newValue in
                    if !editing { draft = String(newValue) }
                }
                .disabled(disabled)
            Stepper("", value: $value, in: range)
                .labelsHidden()
                .disabled(disabled)
            Spacer()
        }
        .onAppear { draft = String(value) }
    }
}

// Fast schematic preview: uses the actual chosen grid dimensions, not the
// underlying encoded black/white pattern. The final page may have fewer rows.
private struct LayoutPreview: View {
    let columns: Int
    let rows: Int
    let pages: Int
    let dataBlockCount: Int
    let recoveryGroupSize: Int
    let paperMode: Bool
    let paperWidth: Double
    let paperHeight: Double
    let left: Double
    let right: Double
    let top: Double
    let bottom: Double

    private var estimatedLastRows: Int {
        let groups = (dataBlockCount + recoveryGroupSize - 1) / recoveryGroupSize
        let capacity = max(1, (columns * rows - recoveryGroupSize - 1) / (recoveryGroupSize + 1))
        let groupsOnLast = max(1, groups - max(0, pages - 1) * capacity)
        let blocksOnLast = min(columns * rows, groupsOnLast * (recoveryGroupSize + 1) + recoveryGroupSize + 1)
        return min(rows, max(1, (blocksOnLast + columns - 1) / columns))
    }

    // Draw the paper at its real width/height ratio and fit the encoded grid
    // within the printable area. Show every page in a horizontal scroller.
    var body: some View {
        GeometryReader { geometry in
            let paperW = paperMode ? max(0.01, paperWidth) : Double(columns)
            let paperH = paperMode ? max(0.01, paperHeight) : Double(rows)
            let ratio = paperW / paperH
            let previewHeight: CGFloat = max(80, geometry.size.height - 29)
            let previewWidth = max(25, previewHeight * CGFloat(ratio))
            ScrollView(.horizontal) {
                HStack(alignment: .top, spacing: 14) {
                    ForEach(0..<pages, id: \.self) { index in
                        VStack(spacing: 6) {
                            Canvas { context, size in
                                let outer = CGRect(origin: .zero, size: size)
                                context.fill(Path(outer), with: .color(.white))
                                context.stroke(Path(outer), with: .color(.gray), lineWidth: 1)
                                let mLeft = paperMode ? CGFloat(max(0, left) / paperW) * size.width : 0
                                let mRight = paperMode ? CGFloat(max(0, right) / paperW) * size.width : 0
                                let mTop = paperMode ? CGFloat(max(0, top) / paperH) * size.height : 0
                                let mBottom = paperMode ? CGFloat(max(0, bottom) / paperH) * size.height : 0
                                let areaW = max(0, size.width - mLeft - mRight)
                                let areaH = max(0, size.height - mTop - mBottom)
                                let nRows = index == pages - 1 ? estimatedLastRows : rows
                                let cell = min(areaW / CGFloat(max(1, columns)), areaH / CGFloat(max(1, rows)))
                                let rect = CGRect(x: mLeft, y: mTop,
                                                  width: CGFloat(columns) * cell,
                                                  height: CGFloat(nRows) * cell)
                                context.fill(Path(rect), with: .color(.black.opacity(0.85)))
                                if cell >= 3 {
                                    var lines = Path()
                                    for x in 0...columns {
                                        let px = rect.minX + CGFloat(x) * cell
                                        lines.move(to: CGPoint(x: px, y: rect.minY))
                                        lines.addLine(to: CGPoint(x: px, y: rect.maxY))
                                    }
                                    for y in 0...nRows {
                                        let py = rect.minY + CGFloat(y) * cell
                                        lines.move(to: CGPoint(x: rect.minX, y: py))
                                        lines.addLine(to: CGPoint(x: rect.maxX, y: py))
                                    }
                                    context.stroke(lines, with: .color(.white.opacity(0.7)), lineWidth: 0.45)
                                }
                            }
                            .frame(width: previewWidth, height: previewHeight)
                            Text("Page \(index + 1) · \(columns)×\(index == pages - 1 ? estimatedLastRows : rows)")
                                .font(.caption2)
                                .lineLimit(1)
                                .minimumScaleFactor(0.6)
                                .frame(width: previewWidth)
                        }
                    }
                }
                .padding(.horizontal, 3)
                .padding(.vertical, 2)
            }
        }
        .accessibilityLabel("Schematic preview of all \(pages) pages, \(columns) columns and up to \(rows) rows")
    }
}


private func qualityLegendItem(_ title: String, color: Color) -> some View {
    HStack(spacing: 4) {
        Rectangle()
            .fill(color)
            .frame(width: 10, height: 10)
        Text(title)
    }
}

// Actual scan results, grouped into the same horizontal page layout as Encode.
private struct QualityMapPreview: View {
    let pages: [[String: Any]]

    private func qualityColor(_ value: Int) -> Color {
        guard value >= 0 else { return .black }
        // 0 corrections = clean; 1...8 = corrected; 9...16 = heavily corrected.
        if value == 0 { return Color(red: 124/255, green: 197/255, blue: 118/255) } // #7CC576
        if value <= 8 { return Color(red: 251/255, green: 175/255, blue: 93/255) } // #FBAF5D
        return Color(red: 242/255, green: 108/255, blue: 79/255) // #F26C4F
    }

    var body: some View {
        GeometryReader { geometry in
            let height = max(80, geometry.size.height - 28)
            ScrollView(.horizontal) {
                HStack(alignment: .top, spacing: 14) {
                    ForEach(Array(pages.enumerated()), id: \.offset) { item in
                        let page = item.element
                        let cols = max(1, page["columns"] as? Int ?? 1)
                        let rows = max(1, page["rows"] as? Int ?? 1)
                        let cells = page["cells"] as? [Int] ?? []
                        let width = max(45, height * CGFloat(cols) / CGFloat(rows))
                        VStack(spacing: 6) {
                            Canvas { context, size in
                                context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(.white))
                                let step = min(size.width / CGFloat(cols), size.height / CGFloat(rows))
                                for y in 0..<rows {
                                    for x in 0..<cols {
                                        let index = y * cols + x
                                        let quality = index < cells.count ? cells[index] : -1
                                        let rect = CGRect(x: CGFloat(x)*step, y: CGFloat(y)*step,
                                                          width: step, height: step)
                                        context.fill(Path(rect), with: .color(qualityColor(quality)))
                                        if step >= 4 {
                                            context.stroke(Path(rect), with: .color(.white.opacity(0.6)), lineWidth: 0.35)
                                        }
                                    }
                                }
                            }
                            .frame(width: width, height: height)
                            Text("Page \(page["page"] as? Int ?? 1) · \(cols)×\(rows)")
                                .font(.caption2).frame(width: width)
                        }
                    }
                }.padding(.horizontal, 3)
            }
        }
    }
}
