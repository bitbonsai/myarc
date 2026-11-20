// Encodes PNG frames with alpha into an HEVC-with-alpha .mov, for the
// welcome's ribbon video (frames from tools/render_welcome_ribbon.mjs).
//   swift tools/encode_hevc_alpha.swift <frames dir> <out.mov> [fps]
// Frames are read in name order; all must have the first one's size.
import AVFoundation
import CoreGraphics
import Foundation
import ImageIO
import VideoToolbox

let args = CommandLine.arguments
guard args.count == 3 || args.count == 4 else {
  FileHandle.standardError.write("usage: encode_hevc_alpha.swift <frames dir> <out.mov> [fps]\n".data(using: .utf8)!)
  exit(64)
}
let dir = URL(fileURLWithPath: args[1])
let out = URL(fileURLWithPath: args[2])
let fps = Int32(args.count == 4 ? Int(args[3]) ?? 60 : 60)

let names = (try FileManager.default.contentsOfDirectory(atPath: dir.path))
  .filter { $0.hasSuffix(".png") }.sorted()
guard let first = names.first else {
  FileHandle.standardError.write("no PNG frames in \(dir.path)\n".data(using: .utf8)!)
  exit(1)
}

func load(_ name: String) -> CGImage? {
  guard let source = CGImageSourceCreateWithURL(dir.appendingPathComponent(name) as CFURL, nil) else { return nil }
  return CGImageSourceCreateImageAtIndex(source, 0, nil)
}

guard let firstImage = load(first) else { exit(1) }
let width = firstImage.width
let height = firstImage.height

try? FileManager.default.removeItem(at: out)
let writer = try AVAssetWriter(outputURL: out, fileType: .mov)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
  AVVideoCodecKey: AVVideoCodecType.hevcWithAlpha,
  AVVideoWidthKey: width,
  AVVideoHeightKey: height,
  AVVideoCompressionPropertiesKey: [
    AVVideoQualityKey: 0.9,
    kVTCompressionPropertyKey_TargetQualityForAlpha as String: 0.9,
    kVTCompressionPropertyKey_AlphaChannelMode as String: kVTAlphaChannelMode_PremultipliedAlpha,
  ] as [String: Any],
])
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
  kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
  kCVPixelBufferWidthKey as String: width,
  kCVPixelBufferHeightKey as String: height,
])
writer.add(input)
guard writer.startWriting() else {
  FileHandle.standardError.write("cannot write: \(String(describing: writer.error))\n".data(using: .utf8)!)
  exit(1)
}
writer.startSession(atSourceTime: .zero)

for (index, name) in names.enumerated() {
  guard let image = load(name), image.width == width, image.height == height,
        let pool = adaptor.pixelBufferPool else {
    FileHandle.standardError.write("bad frame \(name)\n".data(using: .utf8)!)
    exit(1)
  }
  var buffer: CVPixelBuffer?
  CVPixelBufferPoolCreatePixelBuffer(nil, pool, &buffer)
  guard let pixels = buffer else { exit(1) }
  CVPixelBufferLockBaseAddress(pixels, [])
  // Premultiplied BGRA, as the encoder's alpha mode says.
  let context = CGContext(
    data: CVPixelBufferGetBaseAddress(pixels), width: width, height: height, bitsPerComponent: 8,
    bytesPerRow: CVPixelBufferGetBytesPerRow(pixels), space: CGColorSpace(name: CGColorSpace.sRGB)!,
    bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)!
  context.clear(CGRect(x: 0, y: 0, width: width, height: height))
  context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
  CVPixelBufferUnlockBaseAddress(pixels, [])
  while !input.isReadyForMoreMediaData { usleep(1000) }
  adaptor.append(pixels, withPresentationTime: CMTime(value: CMTimeValue(index), timescale: fps))
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
guard writer.status == .completed else {
  FileHandle.standardError.write("encode failed: \(String(describing: writer.error))\n".data(using: .utf8)!)
  exit(1)
}
print("frames=\(names.count) size=\(width)x\(height) fps=\(fps) out=\(out.path)")
