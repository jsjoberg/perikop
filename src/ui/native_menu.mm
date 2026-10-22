#include "ui/native_menu.hpp"
#import <AppKit/AppKit.h>
namespace ortho {
void make_key_window(wxNonOwnedWindow& window, bool activate) {
    if (activate)
        [NSApp activateIgnoringOtherApps:YES];
    [static_cast<NSWindow*>(window.GetWXWindow()) makeKeyWindow];
}
} // namespace ortho
