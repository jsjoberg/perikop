#include "ui/app_icon.hpp"
#include <wx/mstream.h>
namespace ortho {
namespace {
#include "perikop_icon.hpp"
}
std::span<const unsigned char> app_icon_png() {
    return perikop_icon_png;
}
wxImage app_icon_image() {
    const auto png = app_icon_png();
    wxMemoryInputStream input(png.data(), png.size());
    wxImage icon;
    icon.LoadFile(input, wxBITMAP_TYPE_PNG);
    return icon;
}
} // namespace ortho
