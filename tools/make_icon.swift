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
 NSColor(red:0,green:106/255,blue:167/255,alpha:1).setFill()
 NSBezierPath(roundedRect:NSRect(x:72,y:72,width:880,height:880),xRadius:200,yRadius:200).fill()
 NSColor(red:254/255,green:204/255,blue:0,alpha:1).setFill()
 for r in [NSRect(x:472,y:205,width:80,height:625),NSRect(x:362,y:273,width:300,height:62),NSRect(x:258,y:410,width:508,height:76)] {
  NSBezierPath(roundedRect:r,xRadius:12,yRadius:12).fill()
 }
 let bar=NSBezierPath();bar.move(to:NSPoint(x:365,y:630));bar.line(to:NSPoint(x:337,y:680));bar.line(to:NSPoint(x:659,y:824));bar.line(to:NSPoint(x:688,y:774));bar.close();bar.fill()
 NSGraphicsContext.restoreGraphicsState()
 return rep.representation(using:.png,properties:[:])!
}
for size in [16,32,128,256,512] {
 try render(size).write(to:URL(fileURLWithPath:iconset+"/icon_\(size)x\(size).png"))
 try render(size*2).write(to:URL(fileURLWithPath:iconset+"/icon_\(size)x\(size)@2x.png"))
}
try render(1024).write(to:URL(fileURLWithPath:output+"/orthodox-cross.png"))
