// Native Cocoa rendering of the vector geometry in orthodox-cross.svg.
import Cocoa
let output=CommandLine.arguments[1]
let iconset=output+"/OrthodoxReader.iconset"
try FileManager.default.createDirectory(atPath:iconset,withIntermediateDirectories:true)
func render(_ size:Int)->Data {
 let rep=NSBitmapImageRep(bitmapDataPlanes:nil,pixelsWide:size,pixelsHigh:size,bitsPerSample:8,samplesPerPixel:4,hasAlpha:true,isPlanar:false,colorSpaceName:.deviceRGB,bytesPerRow:0,bitsPerPixel:0)!
 NSGraphicsContext.saveGraphicsState();NSGraphicsContext.current=NSGraphicsContext(bitmapImageRep:rep)
 let ctx=NSGraphicsContext.current!.cgContext;ctx.scaleBy(x:CGFloat(size)/1024,y:CGFloat(size)/1024)
 ctx.translateBy(x:0,y:1024);ctx.scaleBy(x:1,y:-1)
 NSColor(red:0,green:82/255,blue:147/255,alpha:1).setFill()
 NSBezierPath(roundedRect:NSRect(x:72,y:72,width:880,height:880),xRadius:200,yRadius:200).fill()
 NSColor(red:254/255,green:203/255,blue:0,alpha:1).setFill()
 for r in [NSRect(x:476,y:160,width:72,height:704),NSRect(x:397,y:236,width:230,height:56),NSRect(x:302,y:348,width:420,height:72)] {
  NSBezierPath(roundedRect:r,xRadius:12,yRadius:12).fill()
 }
 let bar=NSBezierPath();bar.move(to:NSPoint(x:392,y:625));bar.line(to:NSPoint(x:376,y:679));bar.line(to:NSPoint(x:632,y:759));bar.line(to:NSPoint(x:648,y:705));bar.close();bar.fill()
 NSGraphicsContext.restoreGraphicsState()
 return rep.representation(using:.png,properties:[:])!
}
for size in [16,32,128,256,512] {
 try render(size).write(to:URL(fileURLWithPath:iconset+"/icon_\(size)x\(size).png"))
 try render(size*2).write(to:URL(fileURLWithPath:iconset+"/icon_\(size)x\(size)@2x.png"))
}
try render(1024).write(to:URL(fileURLWithPath:output+"/orthodox-cross.png"))
