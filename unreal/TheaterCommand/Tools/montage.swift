// Puts several pictures of the same size into one sheet, in a grid.
// usage: swift Tools/montage.swift out.png columns cropTop cropBottom scale in1.png in2.png ...
// cropTop and cropBottom are rows of pixels cut from the top and bottom of every picture first.
import AppKit

let args = CommandLine.arguments
guard args.count >= 7, let columns = Int(args[2]), let cropTop = Int(args[3]), let cropBottom = Int(args[4]), let scale = Double(args[5]) else {
    print("usage: montage out.png columns cropTop cropBottom scale in...")
    exit(1)
}
let images: [CGImage] = args[6...].compactMap { path in
    guard let data = NSData(contentsOfFile: path), let rep = NSBitmapImageRep(data: data as Data) else { return nil }
    return rep.cgImage
}
guard let first = images.first else { exit(1) }
let cellW = first.width, cellH = first.height - cropTop - cropBottom
let rows = (images.count + columns - 1) / columns
let outW = Int(Double(cellW * columns) * scale), outH = Int(Double(cellH * rows) * scale)
let space = CGColorSpaceCreateDeviceRGB()
let context = CGContext(data: nil, width: outW, height: outH, bitsPerComponent: 8, bytesPerRow: 0, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
context.interpolationQuality = .high
for (index, image) in images.enumerated() {
    guard let cut = image.cropping(to: CGRect(x: 0, y: cropTop, width: cellW, height: cellH)) else { continue }
    let column = index % columns, row = index / columns
    let w = Double(cellW) * scale, h = Double(cellH) * scale
    context.draw(cut, in: CGRect(x: Double(column) * w, y: Double(outH) - Double(row + 1) * h, width: w, height: h))
}
let out = NSBitmapImageRep(cgImage: context.makeImage()!)
try! out.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: args[1]))
