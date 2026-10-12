#include "ui/native_menu.hpp"
#import <AppKit/AppKit.h>
#include <cmath>
namespace ortho {
int native_menu_width(wxMenu& menu) {
    NSMenu* native = menu.GetHMenu();
    return native ? int(std::ceil(native.size.width)) : 0;
}
} // namespace ortho
