#pragma once
#include <wx/menu.h>
namespace ortho {
#ifdef __APPLE__
// The width AppKit lays the menu out at, in points.
int native_menu_width(wxMenu&);
#endif
} // namespace ortho
