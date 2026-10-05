#pragma once
#include "core/model.hpp"
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/font.h>
namespace ortho {
struct Palette { wxColour paper, ink, muted, accent, rule; };
inline Palette palette(Theme theme) {
    const bool dark=theme==Theme::Dark || (theme==Theme::System && wxSystemSettings::GetAppearance().IsDark());
    if(dark)return {{35,33,30},{231,222,204},{167,157,141},{187,158,98},{67,61,51}};
    return {{247,243,233},{49,45,38},{126,116,99},{145,116,59},{220,212,195}};
}
inline wxFont body_font(int points) { return wxFont(wxFontInfo(points).FaceName("Literata")); }
inline wxFont ui_font(int points=11) { return wxFont(wxFontInfo(points).FaceName("IBM Plex Sans")); }
}
