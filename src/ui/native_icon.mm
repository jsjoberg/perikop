#include "ui/native_icon.hpp"
#import <AppKit/AppKit.h>
namespace ortho {
void set_native_app_icon(const std::filesystem::path& path) {
    NSString* file = [NSString stringWithUTF8String:path.c_str()];
    if (!file)
        return;
    NSImage* icon = [[NSImage alloc] initWithContentsOfFile:file];
    if (icon)
        [NSApp setApplicationIconImage:icon];
}
} // namespace ortho
