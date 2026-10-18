#include "ui/toolbar.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <wx/access.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

namespace ortho {
namespace {
#if wxUSE_ACCESSIBILITY
class ButtonAccessible final : public wxWindowAccessible {
    std::function<void()> action_;
    std::function<bool()> checked_;
    bool toggle_;

public:
    ButtonAccessible(wxWindow* window, std::function<void()> action, std::function<bool()> checked,
                     bool toggle)
        : wxWindowAccessible(window), action_(std::move(action)), checked_(std::move(checked)),
          toggle_(toggle) {}
    wxAccStatus GetName(int child, wxString* name) override {
        if (child != wxACC_SELF)
            return wxACC_INVALID_ARG;
        *name = GetWindow()->GetName();
        return wxACC_OK;
    }
    wxAccStatus GetRole(int child, wxAccRole* role) override {
        if (child != wxACC_SELF)
            return wxACC_INVALID_ARG;
        *role = toggle_ ? wxROLE_SYSTEM_CHECKBUTTON : wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }
    wxAccStatus GetState(int child, long* state) override {
        if (child != wxACC_SELF)
            return wxACC_INVALID_ARG;
        *state = wxACC_STATE_SYSTEM_FOCUSABLE;
        if (!GetWindow()->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!GetWindow()->IsShownOnScreen())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        if (GetWindow()->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (toggle_ && checked_())
            *state |= wxACC_STATE_SYSTEM_CHECKED;
        return wxACC_OK;
    }
    wxAccStatus GetDefaultAction(int child, wxString* name) override {
        if (child != wxACC_SELF)
            return wxACC_INVALID_ARG;
        *name = "Aktivera";
        return wxACC_OK;
    }
    wxAccStatus DoDefaultAction(int child) override {
        if (child != wxACC_SELF || !GetWindow()->IsEnabled())
            return wxACC_INVALID_ARG;
        if (action_)
            action_();
        return wxACC_OK;
    }
};
#endif
wxColour mix(const wxColour& from, const wxColour& to, double share) {
    const auto channel = [share](int a, int b) {
        return static_cast<unsigned char>(std::lround(a + (b - a) * share));
    };
    return {channel(from.Red(), to.Red()), channel(from.Green(), to.Green()),
            channel(from.Blue(), to.Blue())};
}
// Symbols are drawn on a 20 × 20 grid.
void draw_symbol(wxGraphicsContext& gc, Symbol symbol, const wxColour& ink) {
    const auto pen = [&](double width) {
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(ink).Width(width).Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
    };
    gc.SetBrush(wxBrush(ink));
    switch (symbol) {
    case Symbol::Split: {
        pen(1.5);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawRoundedRectangle(2, 3, 16, 14, 2);
        gc.StrokeLine(10, 3, 10, 17);
        break;
    }
    case Symbol::Back:
    case Symbol::Next: {
        if (symbol == Symbol::Next) {
            gc.Translate(20, 0);
            gc.Scale(-1, 1);
        }
        pen(1.7);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        auto path = gc.CreatePath();
        path.MoveToPoint(16, 10);
        path.AddLineToPoint(4.5, 10);
        path.MoveToPoint(9.5, 5);
        path.AddLineToPoint(4.5, 10);
        path.AddLineToPoint(9.5, 15);
        gc.StrokePath(path);
        break;
    }
    case Symbol::Play: {
        pen(1.6);
        auto path = gc.CreatePath();
        path.MoveToPoint(6.5, 4.2);
        path.AddLineToPoint(15.8, 10);
        path.AddLineToPoint(6.5, 15.8);
        path.CloseSubpath();
        gc.DrawPath(path);
        break;
    }
    case Symbol::Pause:
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.DrawRoundedRectangle(5, 4, 3.4, 12, 1);
        gc.DrawRoundedRectangle(11.6, 4, 3.4, 12, 1);
        break;
    case Symbol::Search:
        pen(1.8);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawEllipse(3, 3, 11, 11);
        gc.StrokeLine(12.6, 12.6, 16.8, 16.8);
        break;
    case Symbol::Check: {
        pen(2);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        auto path = gc.CreatePath();
        path.MoveToPoint(4.5, 10.5);
        path.AddLineToPoint(8.3, 14.3);
        path.AddLineToPoint(15.5, 6);
        gc.StrokePath(path);
        break;
    }
    case Symbol::Stop:
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.DrawRoundedRectangle(5, 5, 10, 10, 2);
        break;
    case Symbol::Chevron: {
        pen(1.6);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        auto path = gc.CreatePath();
        path.MoveToPoint(8, 4.5);
        path.AddLineToPoint(13.5, 10);
        path.AddLineToPoint(8, 15.5);
        gc.StrokePath(path);
        break;
    }
    case Symbol::Minus:
    case Symbol::Plus:
        pen(1.7);
        gc.StrokeLine(4.5, 10, 15.5, 10);
        if (symbol == Symbol::Plus)
            gc.StrokeLine(10, 4.5, 10, 15.5);
        break;
    case Symbol::Menu:
        // Chrome's three vertical dots.
        gc.SetPen(*wxTRANSPARENT_PEN);
        for (const double y : {4.0, 10.0, 16.0})
            gc.DrawEllipse(8.4, y - 1.6, 3.2, 3.2);
        break;
    case Symbol::Study: {
        // Text with a magnifying glass for word lookup.
        pen(1.5);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokeLine(3, 3.5, 16, 3.5);
        gc.StrokeLine(3, 7, 8, 7);
        gc.StrokeLine(3, 10.5, 5.5, 10.5);
        gc.StrokeLine(3, 14, 5.5, 14);
        gc.DrawEllipse(8, 8, 7, 7);
        gc.StrokeLine(14, 14, 17, 17);
        break;
    }
    }
}
// Activates on mouse release inside the control, or on Space or Return release, like a native button.
// The state and action belong to the control, so they live as long as these handlers.
void bind_press(wxWindow* window, PressState& state, const std::function<void()>& action) {
    const auto focus = [window, &state](wxFocusEvent& e) {
        state.pressed = false;
        window->Refresh(false);
        e.Skip();
    };
    window->Bind(wxEVT_SET_FOCUS, focus);
    window->Bind(wxEVT_KILL_FOCUS, focus);
    const auto activation_key = [](const wxKeyEvent& e) {
        return e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN ||
               e.GetKeyCode() == WXK_NUMPAD_ENTER;
    };
    window->Bind(wxEVT_KEY_DOWN, [window, &state, activation_key](wxKeyEvent& e) {
        if (window->IsEnabled() && activation_key(e)) {
            state.pressed = true;
            window->Refresh(false);
        } else
            e.Skip();
    });
    window->Bind(wxEVT_KEY_UP, [window, &state, &action, activation_key](wxKeyEvent& e) {
        if (activation_key(e)) {
            const bool activate = state.pressed && window->IsEnabled();
            state.pressed = false;
            window->Refresh(false);
            if (activate && action)
                action();
        } else
            e.Skip();
    });
    window->Bind(wxEVT_ENTER_WINDOW, [window, &state](wxMouseEvent&) {
        state.hover = true;
        window->Refresh(false);
    });
    window->Bind(wxEVT_LEAVE_WINDOW, [window, &state](wxMouseEvent&) {
        state.hover = state.pressed = false;
        window->Refresh(false);
    });
    const auto press = [window, &state](wxMouseEvent&) {
        if (!window->IsEnabled())
            return;
        window->SetFocus();
        state.pressed = true;
        window->Refresh(false);
    };
    window->Bind(wxEVT_LEFT_DOWN, press);
    window->Bind(wxEVT_LEFT_DCLICK, press);
    window->Bind(wxEVT_LEFT_UP, [window, &state, &action](wxMouseEvent& e) {
        const bool click =
            state.pressed && window->IsEnabled() && window->GetClientRect().Contains(e.GetPosition());
        state.pressed = false;
        window->Refresh(false);
        if (click && action)
            action();
    });
}
} // namespace

SymbolButton::SymbolButton(wxWindow* parent, std::variant<Symbol, wxString> face, const wxString& label,
                           std::function<void()> action, bool toggle)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      face_(std::move(face)), action_(std::move(action)), colors_(palette(Theme::System)), toggle_(toggle) {
    SetLabel(label);
    SetName(label + (toggle_ ? ", av" : ""));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetInitialSize();
#if wxUSE_ACCESSIBILITY
    new ButtonAccessible(
        this, action_,
        [this] {
            return checked_;
        },
        toggle_);
#endif
    Bind(wxEVT_PAINT, &SymbolButton::paint, this);
    bind_press(this, press_, action_);
}

void SymbolButton::face(std::variant<Symbol, wxString> face) {
    if (face_ == face)
        return;
    face_ = std::move(face);
    Refresh(false);
}

void SymbolButton::checked(bool checked) {
    if (checked_ == checked)
        return;
    checked_ = checked;
    if (toggle_)
        SetName(GetLabel() + (checked ? ", på" : ", av"));
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    Refresh(false);
}

void SymbolButton::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

bool SymbolButton::Enable(bool enable) {
    if (!wxControl::Enable(enable))
        return false;
    if (!enable)
        press_.hover = press_.pressed = false;
    Refresh(false);
    return true;
}

wxSize SymbolButton::DoGetBestClientSize() const {
    return FromDIP(zoomed(wxSize(32, 30)));
}

void SymbolButton::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors_.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const auto size = GetClientSize();
    const double scale = FromDIP(zoomed(100)) / 100.0;
    const bool enabled = IsEnabled();
    if (enabled && HasFocus()) {
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.accent).Width(2 * scale)));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(scale, scale, size.x - 2 * scale, size.y - 2 * scale, 5 * scale);
    }
    if (enabled && (checked_ || press_.hover)) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(checked_         ? with_alpha(colors_.accent, press_.pressed ? 80
                                                                           : press_.hover ? 64
                                                                                          : 44)
                             : press_.pressed ? colors_.overlay
                                              : colors_.surface));
        gc->DrawRoundedRectangle(0, 0, size.x, size.y, 6 * scale);
    }
    const wxColour ink = !enabled ? with_alpha(colors_.muted, 110) : checked_ ? colors_.accent : colors_.ink;
    if (const auto* text = std::get_if<wxString>(&face_)) {
        auto font = ui_font(9);
        font.SetWeight(wxFONTWEIGHT_MEDIUM);
        gc->SetFont(font, ink);
        double width = 0, height = 0;
        gc->GetTextExtent(*text, &width, &height);
        const double x = (size.x - width) / 2, y = (size.y - height) / 2;
        gc->DrawText(*text, x, y);
        // A rounded frame makes the code read as a symbol, like a language tag.
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(ink).Width(1.1 * scale)));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(x - 4 * scale, y - 0.5 * scale, width + 8 * scale, height + scale,
                                 3 * scale);
        return;
    }
    const double unit = 0.9 * scale;
    gc->Translate((size.x - 20 * unit) / 2, (size.y - 20 * unit) / 2);
    gc->Scale(unit, unit);
    draw_symbol(*gc, std::get<Symbol>(face_), ink);
}

TextButton::TextButton(wxWindow* parent, const wxString& label)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      click_([this] {
          wxCommandEvent event(wxEVT_BUTTON, GetId());
          event.SetEventObject(this);
          ProcessWindowEvent(event);
      }) {
    SetLabel(label);
    SetName(label);
    SetFont(ui_font());
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetInitialSize();
#if wxUSE_ACCESSIBILITY
    new ButtonAccessible(
        this, click_,
        [] {
            return false;
        },
        false);
#endif
    Bind(wxEVT_PAINT, &TextButton::paint, this);
    bind_press(this, press_, click_);
}

bool TextButton::Enable(bool enable) {
    if (!wxControl::Enable(enable))
        return false;
    if (!enable)
        press_.hover = press_.pressed = false;
    Refresh(false);
    return true;
}

wxSize TextButton::DoGetBestClientSize() const {
    int width = 0, height = 0;
    GetTextExtent(GetLabel(), &width, &height);
    return {width + FromDIP(zoomed(28)), height + FromDIP(zoomed(14))};
}

void TextButton::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const auto paper = GetBackgroundColour(), ink = GetForegroundColour();
    dc.SetBackground(wxBrush(paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const auto size = GetClientSize();
    const double scale = FromDIP(zoomed(100)) / 100.0, radius = 6 * scale;
    const bool enabled = IsEnabled();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(mix(paper, ink,
                             !enabled         ? 0.04
                             : press_.pressed ? 0.2
                             : press_.hover   ? 0.14
                                              : 0.08)));
    gc->DrawRoundedRectangle(0, 0, size.x, size.y, radius);
    if (enabled && HasFocus()) {
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(mix(paper, ink, 0.6)).Width(1.5 * scale)));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(scale, scale, size.x - 2 * scale, size.y - 2 * scale, radius);
    }
    gc->SetFont(GetFont(), enabled ? ink : mix(paper, ink, 0.4));
    double width = 0, height = 0;
    gc->GetTextExtent(GetLabel(), &width, &height);
    gc->DrawText(GetLabel(), (size.x - width) / 2, (size.y - height) / 2);
}

ReadingRow::ReadingRow(wxWindow* parent, const wxString& label, const wxString& text,
                       std::function<void()> action, bool done, const wxString& full)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      label_(label), text_(text), full_(full), action_(std::move(action)), colors_(palette(Theme::System)),
      done_(done) {
    const auto& spoken = full.empty() ? text : full;
    SetLabel(label.empty() ? spoken
                           : label + ", " + spoken + (done ? wxString::FromUTF8(", läst") : wxString{}));
    SetName(GetLabel());
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &ReadingRow::paint, this);
    if (!opens())
        return;
    SetCursor(wxCursor(wxCURSOR_HAND));
#if wxUSE_ACCESSIBILITY
    new ButtonAccessible(
        this, action_,
        [] {
            return false;
        },
        false);
#endif
    bind_press(this, press_, action_);
}

void ReadingRow::wrap(int width) {
    const auto label_font = ui_font(10), body = body_font(18);
    int unused = 0;
    if (!label_.empty())
        GetTextExtent(label_, &unused, &label_height_, nullptr, nullptr, &label_font);
    GetTextExtent("Ag", &unused, &line_height_, nullptr, nullptr, &body);
    const auto measure = [&](const wxString& text) {
        int extent = 0;
        GetTextExtent(text, &extent, &unused, nullptr, nullptr, &body);
        return extent;
    };
    // Room for the left margin and the chevron, so the longest line never runs into it.
    const int available = std::max(FromDIP(zoomed(80)), width - FromDIP(zoomed(64)));
    lines_.clear();
    if (!full_.empty() && measure(full_) <= available)
        lines_.push_back(full_);
    else {
        // References break after a semicolon or plus sign; one that is still too long breaks at its spaces.
        std::vector<wxString> chunks;
        wxString chunk;
        for (size_t i = 0; i < text_.length(); ++i) {
            if (text_[i] == ' ' && i && (text_[i - 1] == ';' || text_[i - 1] == '+')) {
                chunks.push_back(chunk);
                chunk.clear();
            } else
                chunk += text_[i];
        }
        chunks.push_back(chunk);
        wxString line;
        const auto add = [&](const wxString& piece) {
            const auto joined = line.empty() ? piece : line + " " + piece;
            if (!line.empty() && measure(joined) > available) {
                lines_.push_back(line);
                line = piece;
            } else
                line = joined;
        };
        for (const auto& piece : chunks)
            if (measure(piece) > available)
                for (const auto& word : wxSplit(piece, ' ', '\0'))
                    add(word);
            else
                add(piece);
        if (!line.empty())
            lines_.push_back(line);
    }
    const int height = FromDIP(zoomed(22)) + (label_.empty() ? 0 : label_height_ + FromDIP(zoomed(2))) +
                       int(lines_.size()) * line_height_;
    if (GetMinSize().y != height) {
        SetMinSize({-1, height});
        InvalidateBestSize();
    }
    Refresh(false);
}

void ReadingRow::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

void ReadingRow::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const auto size = GetClientSize();
    const bool active = opens() && IsEnabled();
    dc.SetBackground(wxBrush(active && press_.hover ? (press_.pressed ? colors_.overlay : colors_.surface)
                                                    : colors_.paper));
    dc.Clear();
    dc.SetPen(wxPen(colors_.rule));
    dc.DrawLine(0, size.y - 1, size.x, size.y - 1);
    const int x = FromDIP(zoomed(12));
    int y = FromDIP(zoomed(11));
    if (!label_.empty()) {
        dc.SetFont(ui_font(10));
        dc.SetTextForeground(colors_.muted);
        dc.DrawText(label_, x, y);
        y += label_height_ + FromDIP(zoomed(2));
    }
    dc.SetFont(body_font(18));
    dc.SetTextForeground(opens() ? colors_.ink : colors_.muted);
    for (const auto& line : lines_) {
        dc.DrawText(line, x, y);
        y += line_height_;
    }
    if (!opens())
        return;
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const double scale = FromDIP(zoomed(100)) / 100.0;
    if (HasFocus()) {
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.accent).Width(2 * scale)));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(scale, scale, size.x - 2 * scale, size.y - 3 * scale, 5 * scale);
    }
    const double unit = 0.9 * scale;
    gc->Translate(size.x - FromDIP(zoomed(34)), (size.y - 20 * unit) / 2);
    gc->Scale(unit, unit);
    draw_symbol(*gc, done_ ? Symbol::Check : Symbol::Chevron, done_ ? colors_.accent : colors_.muted);
}

MenuRow::MenuRow(wxWindow* parent, const wxString& label, const wxString& shortcut,
                 std::function<void()> action)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      shortcut_(shortcut), action_(std::move(action)), colors_(palette(Theme::System)) {
    SetLabel(label);
    SetName(shortcut.empty() ? label : label + ", " + shortcut);
    SetFont(ui_font());
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetInitialSize();
#if wxUSE_ACCESSIBILITY
    new ButtonAccessible(
        this, action_,
        [] {
            return false;
        },
        false);
#endif
    Bind(wxEVT_PAINT, &MenuRow::paint, this);
    bind_press(this, press_, action_);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) {
        if (IsEnabled())
            SetFocus();
        e.Skip();
    });
}

void MenuRow::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

bool MenuRow::Enable(bool enable) {
    if (!wxControl::Enable(enable))
        return false;
    Refresh(false);
    return true;
}

wxSize MenuRow::DoGetBestClientSize() const {
    int label = 0, shortcut = 0, height = 0;
    GetTextExtent(GetLabel(), &label, &height);
    if (!shortcut_.empty())
        GetTextExtent(shortcut_, &shortcut, &height);
    return {FromDIP(zoomed(16)) + label + (shortcut ? FromDIP(zoomed(40)) + shortcut : 0) +
                FromDIP(zoomed(16)),
            height + FromDIP(zoomed(12))};
}

void MenuRow::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const auto size = GetClientSize();
    const bool enabled = IsEnabled();
    dc.SetBackground(wxBrush(enabled && HasFocus() ? (press_.pressed ? colors_.overlay : colors_.surface)
                                                   : colors_.paper));
    dc.Clear();
    dc.SetFont(GetFont());
    const int pad = FromDIP(zoomed(16)), y = (size.y - dc.GetCharHeight()) / 2;
    dc.SetTextForeground(enabled ? colors_.ink : mix(colors_.paper, colors_.muted, 0.6));
    dc.DrawText(GetLabel(), pad, y);
    dc.SetTextForeground(enabled ? colors_.muted : mix(colors_.paper, colors_.muted, 0.6));
    dc.DrawText(shortcut_, size.x - pad - dc.GetTextExtent(shortcut_).x, y);
}

AddressBar::AddressBar(wxWindow* parent, std::function<void()> action)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      action_(std::move(action)), colors_(palette(Theme::System)) {
    SetName(wxString::FromUTF8("Adressfält"));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));
    SetInitialSize();
#if wxUSE_ACCESSIBILITY
    new ButtonAccessible(
        this, action_,
        [] {
            return false;
        },
        false);
#endif
    Bind(wxEVT_PAINT, &AddressBar::paint, this);
    const auto focus = [this](wxFocusEvent& e) {
        Refresh(false);
        e.Skip();
    };
    Bind(wxEVT_SET_FOCUS, focus);
    Bind(wxEVT_KILL_FOCUS, focus);
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
        if (IsEnabled() && (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN ||
                            e.GetKeyCode() == WXK_NUMPAD_ENTER)) {
            if (action_)
                action_();
        } else
            e.Skip();
    });
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) {
        hover_ = true;
        Refresh(false);
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        hover_ = false;
        Refresh(false);
    });
    // Like a menu, the field acts on press.
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        SetFocus();
        if (action_)
            action_();
    });
}

void AddressBar::show(const wxString& text, bool dropdown) {
    if (GetLabel() == text && dropdown_ == dropdown)
        return;
    SetLabel(text);
    SetName(text);
    dropdown_ = dropdown;
    Refresh(false);
}

void AddressBar::progress(double share) {
    if (progress_ == share)
        return;
    progress_ = share;
    Refresh(false);
}

void AddressBar::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

wxSize AddressBar::GetMinSize() const {
    return FromDIP(zoomed(wxSize(80, 30)));
}

wxSize AddressBar::DoGetBestClientSize() const {
    return FromDIP(zoomed(wxSize(160, 30)));
}

void AddressBar::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors_.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const auto size = GetClientSize();
    const double scale = FromDIP(zoomed(100)) / 100.0, radius = 8 * scale;
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(hover_ ? colors_.overlay : colors_.surface));
    gc->DrawRoundedRectangle(0, 0, size.x, size.y, radius);
    if (HasFocus()) {
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.accent).Width(2 * scale)));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(scale, scale, size.x - 2 * scale, size.y - 2 * scale, radius);
    }
    const auto font = ui_font(11);
    dc.SetFont(font);
    // The text is centred together with the magnifier before it or the chevron after it.
    const double icon = 14 * scale, before = dropdown_ ? 0 : icon + 6 * scale, after = dropdown_ ? icon : 0;
    const auto text = wxControl::Ellipsize(
        GetLabel(), dc, wxELLIPSIZE_END, std::max(0, size.x - int(2 * radius + 2 * std::max(before, after))));
    gc->SetFont(font, colors_.ink);
    double width = 0, height = 0;
    gc->GetTextExtent(text, &width, &height);
    const double x = (size.x - width - before - after) / 2 + before, middle = size.y / 2.0;
    gc->DrawText(text, x, middle - height / 2);
    if (dropdown_) {
        gc->SetPen(gc->CreatePen(
            wxGraphicsPenInfo(colors_.ink).Width(1.5 * scale).Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
        const double left = x + width + 7 * scale;
        gc->StrokeLine(left, middle - 1.5 * scale, left + 3.5 * scale, middle + 2 * scale);
        gc->StrokeLine(left + 3.5 * scale, middle + 2 * scale, left + 7 * scale, middle - 1.5 * scale);
    } else {
        gc->PushState();
        gc->Translate(x - before, middle - icon / 2);
        gc->Scale(icon / 20, icon / 20);
        draw_symbol(*gc, Symbol::Search, colors_.muted);
        gc->PopState();
    }
    // Like a browser's loading line, the spoken share runs along the field's lower edge.
    if (progress_ > 0) {
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.accent).Width(2 * scale).Cap(wxCAP_ROUND)));
        const double y = size.y - 1.5 * scale;
        gc->StrokeLine(radius, y, radius + (size.x - 2 * radius) * std::min(1.0, progress_), y);
    }
}
} // namespace ortho
