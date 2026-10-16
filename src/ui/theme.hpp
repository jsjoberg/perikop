#pragma once
#include "core/model.hpp"
#include <array>
#include <cmath>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/settings.h>
namespace ortho {
// The colours of a scheme, by role. Backgrounds rise from paper to surface (a resting control) to
// overlay (a hovered control). Verse numbers take the scheme's red, like rubrics.
struct Palette {
    wxColour paper, surface, overlay, ink, muted, rule, accent, selection, verse, today;
    std::array<wxColour, 3> services; // Orthros, Vespers, and other services in the calendar
};
inline wxColour rgb(unsigned hex) {
    return {static_cast<unsigned char>(hex >> 16), static_cast<unsigned char>(hex >> 8),
            static_cast<unsigned char>(hex)};
}
inline Palette palette(unsigned paper, unsigned surface, unsigned overlay, unsigned ink, unsigned muted,
                       unsigned rule, unsigned accent, unsigned selection, unsigned verse, unsigned today,
                       unsigned orthros, unsigned vespers, unsigned other) {
    return {rgb(paper),
            rgb(surface),
            rgb(overlay),
            rgb(ink),
            rgb(muted),
            rgb(rule),
            rgb(accent),
            rgb(selection),
            rgb(verse),
            rgb(today),
            {rgb(orthros), rgb(vespers), rgb(other)}};
}
// The names the upstream themes use, in ColorScheme order: the scheme, then its light and dark variants.
struct SchemeName {
    const char *name, *light, *dark;
};
constexpr std::array<SchemeName, 8> scheme_names{{{"Perikop", "Ljus", "Mörk"},
                                                  {"Rosé Pine", "Dawn", "Main"},
                                                  {"Gruvbox", "Light", "Dark"},
                                                  {"Nord", "Snow Storm", "Polar Night"},
                                                  {"Everforest", "Light", "Dark"},
                                                  {"Catppuccin", "Latte", "Mocha"},
                                                  {"Tokyo Night", "Day", "Night"},
                                                  {"Solarized", "Light", "Dark"}}};
// Values are the upstream palettes' own; comments name their roles.
inline Palette palette(ColorScheme scheme, bool dark) {
    switch (scheme) {
    case ColorScheme::Perikop:
        // The original colours. Surface, overlay, and selection are its earlier tints, made opaque.
        if (dark)
            return palette(0x23211e, 0x302d29, 0x393631, 0xe7decc, 0xa79d8d, 0x433d33, 0xbb9e62, 0x443c2d,
                           0xe7decc, 0x78b3de, 0xbca0df, 0xd7b469, 0x7cb995);
        return palette(0xf7f3e9, 0xeae7dd, 0xe1ddd4, 0x312d26, 0x7e7463, 0xdcd4c3, 0x91743b, 0xe1d7c3,
                       0x312d26, 0x2a70a0, 0x8562ab, 0xae8129, 0x478465);
    case ColorScheme::RosePine:
        // base, overlay, highlight med, text, subtle, highlight high, iris, highlight med, love; foam, iris,
        // gold, pine. Dawn's surface is lighter than its base, so controls rest on overlay.
        if (dark)
            return palette(0x191724, 0x26233a, 0x403d52, 0xe0def4, 0x908caa, 0x524f67, 0xc4a7e7, 0x403d52,
                           0xeb6f92, 0x9ccfd8, 0xc4a7e7, 0xf6c177, 0x31748f);
        return palette(0xfaf4ed, 0xf2e9e1, 0xdfdad9, 0x464261, 0x797593, 0xcecacd, 0x907aa9, 0xdfdad9,
                       0xb4637a, 0x56949f, 0x907aa9, 0xea9d34, 0x286983);
    case ColorScheme::Gruvbox:
        // bg0, bg1, bg2, fg1, fg4, bg2, yellow, bg3 (Visual), red; blue, purple, orange, aqua.
        if (dark)
            return palette(0x282828, 0x3c3836, 0x504945, 0xebdbb2, 0xa89984, 0x504945, 0xfabd2f, 0x665c54,
                           0xfb4934, 0x83a598, 0xd3869b, 0xfe8019, 0x8ec07c);
        return palette(0xfbf1c7, 0xebdbb2, 0xd5c4a1, 0x3c3836, 0x7c6f64, 0xd5c4a1, 0xb57614, 0xbdae93,
                       0x9d0006, 0x076678, 0x8f3f71, 0xaf3a03, 0x427b58);
    case ColorScheme::Nord:
        // Polar Night nord0–3, nord4 text, nord4 at 60 % for secondary text as in Nord's own editor
        // themes, nord8 accent, nord2 selection, nord11 red; nord9, nord15, nord13, nord14.
        if (dark)
            return palette(0x2e3440, 0x3b4252, 0x434c5e, 0xd8dee9, 0x949aa5, 0x4c566a, 0x88c0d0, 0x434c5e,
                           0xbf616a, 0x81a1c1, 0xb48ead, 0xebcb8b, 0xa3be8c);
        // Nord has no light variant; its guide sets bright ambiance on Snow Storm with Polar Night text.
        // The darker Frost nord10 and Aurora orange nord12 replace colours too pale on Snow Storm.
        return palette(0xeceff4, 0xe5e9f0, 0xd8dee9, 0x2e3440, 0x4c566a, 0xd8dee9, 0x5e81ac, 0xd8dee9,
                       0xbf616a, 0x5e81ac, 0xb48ead, 0xd08770, 0xa3be8c);
    case ColorScheme::Everforest:
        // Medium contrast: bg0, bg1, bg2, fg, grey2, bg4, green, bg_visual, red; blue, purple, yellow, aqua.
        if (dark)
            return palette(0x2d353b, 0x343f44, 0x3d484d, 0xd3c6aa, 0x9da9a0, 0x4f585e, 0xa7c080, 0x543a48,
                           0xe67e80, 0x7fbbb3, 0xd699b6, 0xdbbc7f, 0x83c092);
        return palette(0xfdf6e3, 0xf4f0d9, 0xefebd4, 0x5c6a72, 0x829181, 0xe0dcc7, 0x8da101, 0xeaedc8,
                       0xf85552, 0x3a94c5, 0xdf69ba, 0xdfa000, 0x35a77c);
    case ColorScheme::Catppuccin:
        // base, surface0, surface1, text, subtext0, surface1, mauve, overlay2 at 30 % (the style guide's
        // selection), red; blue, mauve, yellow, green.
        if (dark)
            return palette(0x1e1e2e, 0x313244, 0x45475a, 0xcdd6f4, 0xa6adc8, 0x45475a, 0xcba6f7, 0x414356,
                           0xf38ba8, 0x89b4fa, 0xcba6f7, 0xf9e2af, 0xa6e3a1);
        return palette(0xeff1f5, 0xccd0da, 0xbcc0cc, 0x4c4f69, 0x6c6f85, 0xbcc0cc, 0x8839ef, 0xcccfd8,
                       0xd20f39, 0x1e66f5, 0x8839ef, 0xdf8e1d, 0x40a02b);
    case ColorScheme::TokyoNight:
        // bg, bg_highlight, fg_gutter, fg, dark5, fg_gutter, blue, bg_visual, red; cyan, magenta, yellow,
        // green.
        if (dark)
            return palette(0x1a1b26, 0x292e42, 0x3b4261, 0xc0caf5, 0x737aa2, 0x3b4261, 0x7aa2f7, 0x283457,
                           0xf7768e, 0x7dcfff, 0xbb9af7, 0xe0af68, 0x9ece6a);
        return palette(0xe1e2e7, 0xc4c8da, 0xa8aecb, 0x3760bf, 0x68709a, 0xa8aecb, 0x2e7de9, 0xb7c1e3,
                       0xf52a65, 0x007197, 0x9854f1, 0x8c6c3e, 0x587539);
    case ColorScheme::Solarized:
        // Solarized has two background tones: base03 and base02 when dark, base3 and base2 when light.
        // Body text base0/base00, secondary base01/base1, blue accent, red; cyan, violet, yellow, green.
        if (dark)
            return palette(0x002b36, 0x073642, 0x073642, 0x839496, 0x586e75, 0x073642, 0x268bd2, 0x073642,
                           0xdc322f, 0x2aa198, 0x6c71c4, 0xb58900, 0x859900);
        return palette(0xfdf6e3, 0xeee8d5, 0xeee8d5, 0x657b83, 0x93a1a1, 0xeee8d5, 0x268bd2, 0xeee8d5,
                       0xdc322f, 0x2aa198, 0x6c71c4, 0xb58900, 0x859900);
    }
    return palette(ColorScheme::Perikop, dark);
}
// The chosen scheme. Like text_zoom, every palette follows it.
inline ColorScheme color_scheme = ColorScheme::Perikop;
inline Palette palette(Theme theme) {
    return palette(color_scheme, theme == Theme::Dark ||
                                     (theme == Theme::System && wxSystemSettings::GetAppearance().IsDark()));
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
