#include "ui/controls.hpp"
#include "ui/toolbar.hpp"

namespace ortho::ui {
wxString utf8(const std::string& text) {
    return wxString::FromUTF8(text);
}

std::string to_utf8(const wxString& text) {
    const auto bytes = text.ToUTF8();
    return {bytes.data(), bytes.length()};
}

std::filesystem::path filesystem_path(const wxString& text) {
    // ToUTF8 returns a scoped view; text remains alive until the path is copied.
    const auto bytes = text.ToUTF8();
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(bytes.data()), bytes.length()));
}

wxControl* button(wxWindow* parent, const wxString& text, const std::function<void()>& action) {
    auto* control = new TextButton(parent, text);
    control->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) {
        action();
    });
    return control;
}

wxStaticText* label(wxWindow* parent, const wxString& text, int points) {
    auto* control = new wxStaticText(parent, wxID_ANY, text);
    control->SetFont(ui_font(points));
    return control;
}

void recolor(wxWindow* window, const Palette& colors) {
    window->SetBackgroundColour(colors.paper);
    window->SetForegroundColour(colors.ink);
    for (auto* child : window->GetChildren()) {
        recolor(child, colors);
    }
    window->Refresh();
}
} // namespace ortho::ui
