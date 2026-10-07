#pragma once

#include "ui/theme.hpp"
#include <functional>
#include <variant>
#include <wx/control.h>

namespace ortho {
enum class Symbol { Back, Play, Pause, Stop, Study, Search, Check };

// A borderless, browser-style toolbar button. It draws a symbol, or short
// text such as a language code, in the theme's colours. The label is its
// accessible name; the tooltip explains it.
class SymbolButton final : public wxControl {
public:
    SymbolButton(wxWindow* parent, std::variant<Symbol, wxString> face, const wxString& label,
                 std::function<void()> action);
    void face(std::variant<Symbol, wxString>);
    void checked(bool);
    void apply(const Palette&);
    bool Enable(bool enable = true) override;
    bool AcceptsFocus() const override {
        return false;
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void paint(wxPaintEvent&);
    std::variant<Symbol, wxString> face_;
    std::function<void()> action_;
    Palette colors_;
    bool checked_ = false, hover_ = false, pressed_ = false;
};

// The address field: what is shown or read, and the way to another passage.
// As a dropdown it lists a reading's parts; otherwise a magnifier marks it
// as the passage search. The label is the displayed text.
class AddressBar final : public wxControl {
public:
    AddressBar(wxWindow* parent, std::function<void()> action);
    void show(const wxString& text, bool dropdown);
    // Spoken share of the current reading; 0 hides the line.
    void progress(double);
    void apply(const Palette&);
    bool AcceptsFocus() const override {
        return false;
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void paint(wxPaintEvent&);
    std::function<void()> action_;
    Palette colors_;
    double progress_ = 0;
    bool dropdown_ = false, hover_ = false;
};
} // namespace ortho
