#pragma once
#include "core/model.hpp"
#include <cmath>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/settings.h>
namespace ortho {
struct Palette {
    wxColour paper, ink, muted, accent, rule;
};
inline Palette palette(Theme theme) {
    const bool dark =
        theme == Theme::Dark || (theme == Theme::System && wxSystemSettings::GetAppearance().IsDark());
    if (dark)
        return {{35, 33, 30}, {231, 222, 204}, {167, 157, 141}, {187, 158, 98}, {67, 61, 51}};
    return {{247, 243, 233}, {49, 45, 38}, {126, 116, 99}, {145, 116, 59}, {220, 212, 195}};
}
// The colour with an opacity from 0 (transparent) to 255 (opaque).
inline wxColour with_alpha(const wxColour& colour, int alpha) {
    return {colour.Red(), colour.Green(), colour.Blue(), static_cast<unsigned char>(alpha)};
}
// The text size, in percent of normal. Every font follows it, and so does the space around text.
inline int text_zoom = 100;
// A length in device-independent pixels at the current text size.
inline int zoomed(int dip) {
    return int(std::lround(dip * text_zoom / 100.0));
}
inline wxSize zoomed(wxSize dip) {
    return {dip.x < 0 ? dip.x : zoomed(dip.x), dip.y < 0 ? dip.y : zoomed(dip.y)};
}
// The scripture text in points at normal size.
constexpr int reading_points = 19;
inline wxFont body_font(double points) {
    return wxFont(wxFontInfo(points * text_zoom / 100).FaceName("Literata"));
}
inline wxFont ui_font(double points = 11) {
    return wxFont(wxFontInfo(points * text_zoom / 100).FaceName("IBM Plex Sans"));
}
} // namespace ortho
