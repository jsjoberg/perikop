#pragma once
#include <functional>
#include <wx/window.h>
namespace ortho {
void* install_native_scroll(wxWindow*, std::function<void(double)>);
void remove_native_scroll(void*);
} // namespace ortho
