#include "ui/toolbar.hpp"
#include <algorithm>
#include <memory>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

namespace ortho {
namespace {
wxColour mix(const wxColour& from, const wxColour& to, double share) {
    const auto channel = [share](int a, int b) {
        return static_cast<unsigned char>(a + (b - a) * share + 0.5);
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
    case Symbol::Back: {
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
    case Symbol::Study: {
        // An open book.
        pen(1.5);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        auto path = gc.CreatePath();
        path.MoveToPoint(10, 6.3);
        path.AddCurveToPoint(8, 4.8, 5, 4.4, 2.5, 4.9);
        path.AddLineToPoint(2.5, 15);
        path.AddCurveToPoint(5, 14.5, 8, 14.9, 10, 16.4);
        path.AddCurveToPoint(12, 14.9, 15, 14.5, 17.5, 15);
        path.AddLineToPoint(17.5, 4.9);
        path.AddCurveToPoint(15, 4.4, 12, 4.8, 10, 6.3);
        path.AddLineToPoint(10, 16.4);
        gc.StrokePath(path);
        break;
    }
    }
}
} // namespace

SymbolButton::SymbolButton(wxWindow* parent, std::variant<Symbol, wxString> face, const wxString& label,
                           std::function<void()> action)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), face_(std::move(face)),
      action_(std::move(action)), colors_(palette(Theme::System)) {
    SetLabel(label);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetInitialSize();
    Bind(wxEVT_PAINT, &SymbolButton::paint, this);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) {
        hover_ = true;
        Refresh(false);
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        hover_ = pressed_ = false;
        Refresh(false);
    });
    const auto press = [this](wxMouseEvent&) {
        pressed_ = true;
        Refresh(false);
    };
    Bind(wxEVT_LEFT_DOWN, press);
    Bind(wxEVT_LEFT_DCLICK, press);
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        const bool click = pressed_ && IsEnabled() && GetClientRect().Contains(e.GetPosition());
        pressed_ = false;
        Refresh(false);
        if (click && action_)
            action_();
    });
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
        hover_ = pressed_ = false;
    Refresh(false);
    return true;
}

wxSize SymbolButton::DoGetBestClientSize() const {
    return FromDIP(wxSize(32, 30));
}

void SymbolButton::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors_.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const auto size = GetClientSize();
    const double scale = FromDIP(100) / 100.0;
    const bool enabled = IsEnabled();
    if (enabled && (checked_ || hover_)) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(checked_ ? with_alpha(colors_.accent, pressed_ ? 80
                                                                   : hover_ ? 64
                                                                            : 44)
                                      : with_alpha(colors_.ink, pressed_ ? 36 : 20)));
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

AddressBar::AddressBar(wxWindow* parent, std::function<void()> action)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      action_(std::move(action)), colors_(palette(Theme::System)) {
    SetName(wxString::FromUTF8("Adressfält"));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));
    SetInitialSize();
    Bind(wxEVT_PAINT, &AddressBar::paint, this);
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
        if (action_)
            action_();
    });
}

void AddressBar::show(const wxString& text, bool dropdown) {
    if (GetLabel() == text && dropdown_ == dropdown)
        return;
    SetLabel(text);
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

wxSize AddressBar::DoGetBestClientSize() const {
    return FromDIP(wxSize(160, 30));
}

void AddressBar::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors_.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const auto size = GetClientSize();
    const double scale = FromDIP(100) / 100.0, radius = 8 * scale;
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(mix(colors_.paper, colors_.ink, hover_ ? 0.11 : 0.065)));
    gc->DrawRoundedRectangle(0, 0, size.x, size.y, radius);
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
