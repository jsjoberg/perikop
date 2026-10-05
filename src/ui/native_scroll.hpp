#pragma once
#include <wx/window.h>
#include <functional>
namespace ortho {
void* install_native_scroll(wxWindow*,std::function<void(double)>);
void remove_native_scroll(void*);
}
