#include "ui/native_icon.hpp"
#import <AppKit/AppKit.h>
namespace ortho {
void set_native_app_icon(std::span<const unsigned char> png) {
    NSData* data = [NSData dataWithBytes:png.data() length:png.size()];
    NSImage* icon = [[NSImage alloc] initWithData:data];
    if (icon)
        [NSApp setApplicationIconImage:icon];
}
} // namespace ortho
