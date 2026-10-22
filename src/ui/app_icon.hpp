#pragma once
#include <span>
#include <wx/image.h>
namespace ortho {
std::span<const unsigned char> app_icon_png();
wxImage app_icon_image();
} // namespace ortho
