#pragma once

#include "ui/theme.hpp"
#include <functional>
#include <variant>
#include <vector>
#include <wx/control.h>

namespace ortho {
enum class Symbol { Back, Next, Play, Pause, Stop, Study, Search, Check, Menu, Chevron, Minus, Plus };
// Hover and press state of a drawn button, which acts on release like a native one.
struct PressState {
    bool hover = false, pressed = false;
};

// A borderless, browser-style toolbar button. It draws a symbol, or short
// text such as a language code, in the theme's colours. The label is its
// accessible name; the tooltip explains it.
class SymbolButton final : public wxControl {
public:
    SymbolButton(wxWindow* parent, std::variant<Symbol, wxString> face, const wxString& label,
                 std::function<void()> action, bool toggle = false);
    void face(std::variant<Symbol, wxString>);
    void checked(bool);
    void apply(const Palette&);
    bool Enable(bool enable = true) override;
    bool AcceptsFocus() const override {
        return IsShown() && IsEnabled();
    }
    bool AcceptsFocusFromKeyboard() const override {
        return AcceptsFocus();
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void paint(wxPaintEvent&);
    std::variant<Symbol, wxString> face_;
    std::function<void()> action_;
    Palette colors_;
    PressState press_;
    bool checked_ = false, toggle_ = false;
};

// A drawn text button, the same on every platform. It paints in the background and
// foreground colours its page sets, and sends wxEVT_BUTTON like a native button.
class TextButton final : public wxControl {
public:
    TextButton(wxWindow* parent, const wxString& label);
    bool Enable(bool enable = true) override;
    bool AcceptsFocus() const override {
        return IsShown() && IsEnabled();
    }
    bool AcceptsFocusFromKeyboard() const override {
        return AcceptsFocus();
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void paint(wxPaintEvent&);
    std::function<void()> click_;
    PressState press_;
};

// A start-page row that opens what it names: a quiet label above the wrapped text,
// with a chevron, or a check once read. Without an action it only shows the text.
class ReadingRow final : public wxControl {
public:
    // full is the text with whole book names, shown instead when it fits on one line.
    ReadingRow(wxWindow* parent, const wxString& label, const wxString& text, std::function<void()> action,
               bool done = false, const wxString& full = {});
    // Wraps the text to the width, after semicolons and plus signs where it can, and sets the height.
    void wrap(int width);
    void apply(const Palette&);
    bool opens() const {
        return bool(action_);
    }
    bool AcceptsFocus() const override {
        return IsShown() && IsEnabled() && opens();
    }
    bool AcceptsFocusFromKeyboard() const override {
        return AcceptsFocus();
    }

private:
    void paint(wxPaintEvent&);
    wxString label_, text_, full_;
    std::vector<wxString> lines_;
    int label_height_ = 0, line_height_ = 0;
    std::function<void()> action_;
    Palette colors_;
    PressState press_;
    bool done_;
};

// A row of the ⋮ menu: the command, with its shortcut at the right. Like a native menu item,
// the row under the pointer takes the focus, so one highlight shows the row Return chooses.
class MenuRow final : public wxControl {
public:
    MenuRow(wxWindow* parent, const wxString& label, const wxString& shortcut, std::function<void()> action);
    void apply(const Palette&);
    bool Enable(bool enable = true) override;
    bool AcceptsFocus() const override {
        return IsShown() && IsEnabled();
    }
    bool AcceptsFocusFromKeyboard() const override {
        return AcceptsFocus();
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void paint(wxPaintEvent&);
    wxString shortcut_;
    std::function<void()> action_;
    Palette colors_;
    PressState press_;
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
        return IsShown() && IsEnabled();
    }
    bool AcceptsFocusFromKeyboard() const override {
        return AcceptsFocus();
    }
    // The narrowest the field gets in a narrow window, at the current text size.
    wxSize GetMinSize() const override;

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
