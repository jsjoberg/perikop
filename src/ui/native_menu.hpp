#pragma once
#include <wx/nonownedwnd.h>
namespace ortho {
#ifdef __APPLE__
// wxWidgets shows a popup window on macOS without making it key, so it gets no keys. This does.
void make_key_window(wxNonOwnedWindow&);
#endif
} // namespace ortho
