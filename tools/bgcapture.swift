// Background helpers for capture scripts: never activates the target app.
//   swift tools/bgcapture.swift key <pid> <combo>   e.g. cmd+t (posted to the PID)
//   swift tools/bgcapture.swift shot <pid> <out.png> [title] (all on-screen windows
//     of the PID, cropped to its largest window; works while other windows cover
//     it. With a title, only the PID's window with that title.)
import CoreGraphics
import Foundation
import ScreenCaptureKit

let args = CommandLine.arguments
guard args.count == 4 || args.count == 5, let pid = Int32(args[2]) else {
  FileHandle.standardError.write("usage: bgcapture.swift key|shot <pid> <arg> [title]\n".data(using: .utf8)!)
  exit(64)
}

let keyCodes: [String: CGKeyCode] = [
  "a": 0, "s": 1, "d": 2, "f": 3, "h": 4, "g": 5, "z": 6, "x": 7, "c": 8, "v": 9,
  "b": 11, "q": 12, "w": 13, "e": 14, "r": 15, "y": 16, "t": 17, "o": 31, "u": 32,
  "i": 34, "p": 35, "l": 37, "j": 38, "k": 40, "n": 45, "m": 46, "comma": 43,
  "return": 36, "tab": 48, "space": 49, "escape": 53, "down": 125, "up": 126,
]

func postKey(_ combo: String) {
  let parts = combo.lowercased().split(separator: "+").map(String.init)
  guard let name = parts.last, let code = keyCodes[name] else {
    FileHandle.standardError.write("unknown key in \(combo)\n".data(using: .utf8)!)
    exit(65)
  }
  var flags: CGEventFlags = []
  for mod in parts.dropLast() {
    switch mod {
    case "cmd": flags.insert(.maskCommand)
    case "ctrl": flags.insert(.maskControl)
    case "shift": flags.insert(.maskShift)
    case "alt": flags.insert(.maskAlternate)
    default: break
    }
  }
  let source = CGEventSource(stateID: .privateState)
  for down in [true, false] {
    let event = CGEvent(keyboardEventSource: source, virtualKey: code, keyDown: down)!
    event.flags = flags
    event.postToPid(pid)
    usleep(30_000)
  }
}

func shot(_ out: String) async throws {
  let content = try await SCShareableContent.excludingDesktopWindows(
    false, onScreenWindowsOnly: true)
  let title = args.count == 5 ? args[4] : nil
  let windows = content.windows.filter {
    $0.owningApplication?.processID == pid && $0.frame.width > 40 && $0.frame.height > 40
      && (title == nil || $0.title == title)
  }
  guard let main = windows.max(by: {
    $0.frame.width * $0.frame.height < $1.frame.width * $1.frame.height
  }) else {
    FileHandle.standardError.write("no on-screen window for pid \(pid)\n".data(using: .utf8)!)
    exit(66)
  }
  let display = content.displays.first { $0.frame.intersects(main.frame) } ?? content.displays[0]
  let filter = SCContentFilter(display: display, including: windows)
  let scale = CGFloat(filter.pointPixelScale)
  let config = SCStreamConfiguration()
  config.sourceRect = main.frame.offsetBy(dx: -display.frame.minX, dy: -display.frame.minY)
  config.width = Int(main.frame.width * scale)
  config.height = Int(main.frame.height * scale)
  config.showsCursor = false
  let image = try await SCScreenshotManager.captureImage(
    contentFilter: filter, configuration: config)
  let url = URL(fileURLWithPath: out) as CFURL
  guard let dest = CGImageDestinationCreateWithURL(url, "public.png" as CFString, 1, nil) else {
    exit(73)
  }
  CGImageDestinationAddImage(dest, image, nil)
  exit(CGImageDestinationFinalize(dest) ? 0 : 74)
}

switch args[1] {
case "key":
  postKey(args[3])
case "shot":
  Task {
    do { try await shot(args[3]) } catch {
      FileHandle.standardError.write("capture failed: \(error)\n".data(using: .utf8)!)
      exit(1)
    }
  }
  dispatchMain()
default:
  exit(64)
}
