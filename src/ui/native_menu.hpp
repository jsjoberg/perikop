#pragma once
#include <wx/nonownedwnd.h>
namespace ortho {
#ifdef __APPLE__
// wxWidgets shows a popup window on macOS without making it key, so it gets no keys. This does.
// Desktop diagnostics launched from a shell can also request application activation.
void make_key_window(wxNonOwnedWindow&, bool activate = false);
#endif
} // namespace ortho
