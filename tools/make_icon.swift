// Run on macOS: swift tools/make_icon.swift resources/icons
// One vector design produces SVG, PNG, ICNS, and Windows ICO sizes.
import Cocoa

guard CommandLine.arguments.count == 2 else {
    fatalError("Usage: swift tools/make_icon.swift output-directory")
}
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let iconset = output.appendingPathComponent("OrthodoxReader.iconset", isDirectory: true)
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
let space = CGColorSpace(name: CGColorSpace.sRGB)!
func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: space, components: [CGFloat((hex >> 16) & 255) / 255,
        CGFloat((hex >> 8) & 255) / 255, CGFloat(hex & 255) / 255, alpha])!
}
struct Stop { let position: CGFloat; let hex: UInt32; var alpha: CGFloat = 1 }
let enamel = [Stop(position: 0, hex: 0x278dc5), Stop(position: 0.43, hex: 0x005293), Stop(position: 1, hex: 0x062d55)]
let gold = [Stop(position: 0, hex: 0xfff3b5), Stop(position: 0.23, hex: 0xffd456),
    Stop(position: 0.43, hex: 0xdca024), Stop(position: 0.53, hex: 0xffed96),
    Stop(position: 0.69, hex: 0xf4c444), Stop(position: 1, hex: 0xb9781c)]
let glow = [Stop(position: 0, hex: 0xa8deff, alpha: 0.4), Stop(position: 1, hex: 0xa8deff, alpha: 0)]
let glint = [Stop(position: 0, hex: 0xfffdeb, alpha: 0.95), Stop(position: 1, hex: 0xfffdeb, alpha: 0)]
func gradient(_ stops: [Stop]) -> CGGradient {
    CGGradient(colorsSpace: space, colors: stops.map { color($0.hex, $0.alpha) } as CFArray,
        locations: stops.map(\.position))!
}
func linear(_ ctx: CGContext, _ stops: [Stop], _ from: CGPoint, _ to: CGPoint) {
    ctx.drawLinearGradient(gradient(stops), start: from, end: to,
        options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
}
// Generate the SVG and native outlines together to keep their geometry identical.
final class Outline {
    let path = CGMutablePath()
    var commands: [String] = []
    func move(_ x: CGFloat, _ y: CGFloat) {
        path.move(to: CGPoint(x: x, y: y)); commands.append("M\(x) \(y)")
    }
    func line(_ x: CGFloat, _ y: CGFloat) {
        path.addLine(to: CGPoint(x: x, y: y)); commands.append("L\(x) \(y)")
    }
    func quad(_ cx: CGFloat, _ cy: CGFloat, _ x: CGFloat, _ y: CGFloat) {
        path.addQuadCurve(to: CGPoint(x: x, y: y), control: CGPoint(x: cx, y: cy))
        commands.append("Q\(cx) \(cy) \(x) \(y)")
    }
    func close() { path.closeSubpath(); commands.append("Z") }
}
let cross = Outline()
cross.move(488, 160); cross.line(536, 160); cross.quad(548, 160, 548, 172)
cross.line(548, 236); cross.line(615, 236); cross.quad(627, 236, 627, 248)
cross.line(627, 280); cross.quad(627, 292, 615, 292); cross.line(548, 292)
cross.line(548, 348); cross.line(710, 348); cross.quad(722, 348, 722, 360)
cross.line(722, 408); cross.quad(722, 420, 710, 420); cross.line(548, 420)
cross.line(548, 673.75); cross.line(648, 705); cross.line(632, 759)
cross.line(548, 732.75); cross.line(548, 852); cross.quad(548, 864, 536, 864)
cross.line(488, 864); cross.quad(476, 864, 476, 852); cross.line(476, 710.25)
cross.line(376, 679); cross.line(392, 625); cross.line(476, 651.25)
cross.line(476, 420); cross.line(314, 420); cross.quad(302, 420, 302, 408)
cross.line(302, 360); cross.quad(302, 348, 314, 348); cross.line(476, 348)
cross.line(476, 292); cross.line(409, 292); cross.quad(397, 292, 397, 280)
cross.line(397, 248); cross.quad(397, 236, 409, 236); cross.line(476, 236)
cross.line(476, 172); cross.quad(476, 160, 488, 160); cross.close()
let tile = CGPath(roundedRect: CGRect(x: 72, y: 72, width: 880, height: 880), cornerWidth: 200, cornerHeight: 200, transform: nil)
let rim = CGPath(roundedRect: CGRect(x: 80, y: 80, width: 864, height: 864), cornerWidth: 192, cornerHeight: 192, transform: nil)
let reflection = Outline()
reflection.move(285, 320); reflection.line(740, 470); reflection.line(740, 500); reflection.line(285, 350); reflection.close()

func render(_ size: Int) -> Data {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let ctx = NSGraphicsContext.current!.cgContext
    ctx.scaleBy(x: CGFloat(size) / 1024, y: CGFloat(size) / 1024)
    ctx.translateBy(x: 0, y: 1024); ctx.scaleBy(x: 1, y: -1)
    ctx.setAllowsAntialiasing(true)
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -12), blur: 18, color: color(0x001b35, 0.32))
    ctx.addPath(tile); ctx.setFillColor(color(0x005293)); ctx.fillPath()
    ctx.restoreGState()
    ctx.saveGState()
    ctx.addPath(tile); ctx.clip()
    linear(ctx, enamel, CGPoint(x: 175, y: 100), CGPoint(x: 805, y: 940))
    ctx.drawRadialGradient(gradient(glow), startCenter: CGPoint(x: 340, y: 210), startRadius: 0,
        endCenter: CGPoint(x: 340, y: 210), endRadius: 640, options: [.drawsAfterEndLocation])
    ctx.restoreGState()
    ctx.addPath(tile); ctx.setLineWidth(3); ctx.setStrokeColor(color(0x002b50, 0.7)); ctx.strokePath()
    ctx.addPath(rim); ctx.setLineWidth(2); ctx.setStrokeColor(color(0xb1e4ff, 0.24)); ctx.strokePath()
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -14), blur: 16, color: color(0x001b32, 0.55))
    ctx.addPath(cross.path); ctx.setFillColor(color(0xc78a21)); ctx.fillPath()
    ctx.restoreGState()
    ctx.addPath(cross.path); ctx.setLineWidth(12); ctx.setLineJoin(.round)
    ctx.setStrokeColor(color(0x946019)); ctx.strokePath()
    ctx.saveGState()
    ctx.addPath(cross.path); ctx.clip()
    linear(ctx, gold, CGPoint(x: 320, y: 175), CGPoint(x: 680, y: 865))
    ctx.addPath(reflection.path); ctx.setFillColor(color(0xfff9d7, 0.18)); ctx.fillPath()
    ctx.drawRadialGradient(gradient(glint), startCenter: CGPoint(x: 488, y: 245), startRadius: 0,
        endCenter: CGPoint(x: 488, y: 245), endRadius: 30, options: [])
    ctx.restoreGState()
    ctx.addPath(cross.path); ctx.setLineWidth(3); ctx.setStrokeColor(color(0xfff0aa, 0.8)); ctx.strokePath()
    ctx.move(to: CGPoint(x: 321, y: 358)); ctx.addLine(to: CGPoint(x: 462, y: 358))
    ctx.move(to: CGPoint(x: 486, y: 179)); ctx.addLine(to: CGPoint(x: 486, y: 224))
    ctx.setLineWidth(3); ctx.setLineCap(.round); ctx.setStrokeColor(color(0xfffde8, 0.64)); ctx.strokePath()
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}
func svgStops(_ stops: [Stop]) -> String {
    stops.map { "<stop offset=\"\($0.position)\" stop-color=\"\(String(format: "#%06x", $0.hex))\" stop-opacity=\"\($0.alpha)\"/>" }.joined(separator: "\n      ")
}
let svg = """
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <!-- Generated by tools/make_icon.swift: polished gold on Swedish blue enamel. -->
  <defs>
    <linearGradient id="enamel" gradientUnits="userSpaceOnUse" x1="175" y1="100" x2="805" y2="940">
      \(svgStops(enamel))
    </linearGradient>
    <linearGradient id="gold" gradientUnits="userSpaceOnUse" x1="320" y1="175" x2="680" y2="865">
      \(svgStops(gold))
    </linearGradient>
    <radialGradient id="glow" gradientUnits="userSpaceOnUse" cx="340" cy="210" r="640">
      \(svgStops(glow))
    </radialGradient>
    <radialGradient id="glint" gradientUnits="userSpaceOnUse" cx="488" cy="245" r="30">
      \(svgStops(glint))
    </radialGradient>
    <path id="cross" d="\(cross.commands.joined(separator: " "))"/>
    <rect id="tile" x="72" y="72" width="880" height="880" rx="200"/>
    <clipPath id="tileClip"><use href="#tile"/></clipPath>
    <clipPath id="crossClip"><use href="#cross"/></clipPath>
    <filter id="tileShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="12" stdDeviation="9" flood-color="#001b35" flood-opacity="0.32"/>
    </filter>
    <filter id="crossShadow" x="-30%" y="-20%" width="160%" height="140%">
      <feDropShadow dx="0" dy="14" stdDeviation="8" flood-color="#001b32" flood-opacity="0.55"/>
    </filter>
  </defs>
  <use href="#tile" fill="#005293" filter="url(#tileShadow)"/>
  <g clip-path="url(#tileClip)">
    <use href="#tile" fill="url(#enamel)"/>
    <use href="#tile" fill="url(#glow)"/>
  </g>
  <use href="#tile" fill="none" stroke="#002b50" stroke-opacity="0.7" stroke-width="3"/>
  <rect x="80" y="80" width="864" height="864" rx="192" fill="none" stroke="#b1e4ff" stroke-opacity="0.24" stroke-width="2"/>
  <use href="#cross" fill="#c78a21" filter="url(#crossShadow)"/>
  <use href="#cross" fill="none" stroke="#946019" stroke-width="12" stroke-linejoin="round"/>
  <g clip-path="url(#crossClip)">
    <use href="#cross" fill="url(#gold)"/>
    <path d="\(reflection.commands.joined(separator: " "))" fill="#fff9d7" fill-opacity="0.18"/>
    <use href="#cross" fill="url(#glint)"/>
  </g>
  <use href="#cross" fill="none" stroke="#fff0aa" stroke-opacity="0.8" stroke-width="3" stroke-linejoin="round"/>
  <path d="M321 358H462 M486 179V224" fill="none" stroke="#fffde8" stroke-opacity="0.64" stroke-width="3" stroke-linecap="round"/>
</svg>

"""
try svg.write(to: output.appendingPathComponent("orthodox-cross.svg"), atomically: true, encoding: .utf8)
var rasters: [Int: Data] = [:]
for size in [16, 32, 48, 64, 128, 256, 512, 1024] { rasters[size] = render(size) }
for size in [16, 32, 128, 256, 512] {
    try rasters[size]!.write(to: iconset.appendingPathComponent("icon_\(size)x\(size).png"))
    try rasters[size * 2]!.write(to: iconset.appendingPathComponent("icon_\(size)x\(size)@2x.png"))
}
try rasters[1024]!.write(to: output.appendingPathComponent("orthodox-cross.png"))
// Windows supports PNG-compressed ICO entries.
func appendLE<T: FixedWidthInteger>(_ value: T, to data: inout Data) {
    var little = value.littleEndian
    withUnsafeBytes(of: &little) { data.append(contentsOf: $0) }
}
let windowsSizes = [16, 32, 48, 64, 128, 256]
var ico = Data()
appendLE(UInt16(0), to: &ico); appendLE(UInt16(1), to: &ico); appendLE(UInt16(windowsSizes.count), to: &ico)
var offset = 6 + windowsSizes.count * 16
for size in windowsSizes {
    ico.append(contentsOf: [UInt8(size == 256 ? 0 : size), UInt8(size == 256 ? 0 : size), 0, 0])
    appendLE(UInt16(1), to: &ico); appendLE(UInt16(32), to: &ico)
    appendLE(UInt32(rasters[size]!.count), to: &ico); appendLE(UInt32(offset), to: &ico)
    offset += rasters[size]!.count
}
for size in windowsSizes { ico.append(rasters[size]!) }
try ico.write(to: output.appendingPathComponent("OrthodoxReader.ico"))
let iconutil = Process()
iconutil.executableURL = URL(fileURLWithPath: "/usr/bin/iconutil")
iconutil.arguments = ["-c", "icns", iconset.path, "-o", output.appendingPathComponent("OrthodoxReader.icns").path]
try iconutil.run(); iconutil.waitUntilExit()
guard iconutil.terminationStatus == 0 else { fatalError("iconutil failed") }
print("Generated SVG, PNG, macOS ICNS, and Windows ICO in \(output.path)")
