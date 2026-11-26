// usage: swift Tools/clip.swift out.gif scale secondsPerFrame in1.png in2.png ...
// Puts pictures of one view together as a short clip that plays round and round (a GIF), each drawn at a scale.
import Foundation
import ImageIO
import CoreGraphics
import UniformTypeIdentifiers

let args = CommandLine.arguments
guard args.count >= 5, let scale = Double(args[2]), let delay = Double(args[3]) else {
    print("usage: clip out.gif scale secondsPerFrame in...")
    exit(1)
}
let inputs = Array(args[4...])
guard let out = CGImageDestinationCreateWithURL(URL(fileURLWithPath: args[1]) as CFURL, UTType.gif.identifier as CFString, inputs.count, nil) else {
    print("could not make \(args[1])")
    exit(1)
}
CGImageDestinationSetProperties(out, [kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFLoopCount: 0]] as CFDictionary)
var written = 0
for path in inputs {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil), let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        print("Warning: \(path) not a valid file - skipping")
        continue
    }
    let wide = Int(Double(image.width) * scale), high = Int(Double(image.height) * scale)
    guard let context = CGContext(data: nil, width: wide, height: high, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue) else { continue }
    context.interpolationQuality = .high
    context.draw(image, in: CGRect(x: 0, y: 0, width: wide, height: high))
    if let small = context.makeImage() {
        CGImageDestinationAddImage(out, small, [kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFDelayTime: delay, kCGImagePropertyGIFUnclampedDelayTime: delay]] as CFDictionary)
        written += 1
    }
}
if CGImageDestinationFinalize(out) {
    print("\(written) pictures in \(args[1])")
} else {
    print("could not write \(args[1])")
    exit(1)
}
