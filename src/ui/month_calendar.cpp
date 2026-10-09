#include "ui/month_calendar.hpp"
#include "ui/controls.hpp"
#include <memory>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

namespace ortho {
namespace {
using namespace std::chrono;
// Layout on a grid of device-independent pixels.
constexpr int cell_width = 40, cell_height = 34, header = 40, weekdays = 24;
bool supported(CivilDate date) {
    return date.ok() && int(date.year()) >= 1 && int(date.year()) <= 9999;
}
} // namespace

MonthCalendar::MonthCalendar(wxWindow* parent, CivilDate selected,
                             std::function<void(std::optional<CivilDate>)> chosen)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      selected_(selected), focus_(selected), chosen_(std::move(chosen)), colors_(palette(Theme::System)) {
    SetLabel(ui::utf8("Välj datum"));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetInitialSize();
    Bind(wxEVT_PAINT, &MonthCalendar::paint, this);
    Bind(wxEVT_KEY_DOWN, &MonthCalendar::key, this);
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) {
        Refresh(false);
        e.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
        Refresh(false);
        e.Skip();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
        const auto day = day_at(e.GetPosition());
        const int arrow = arrow_at(e.GetPosition());
        if (day != hover_ || arrow != arrow_hover_) {
            hover_ = day;
            arrow_hover_ = arrow;
            Refresh(false);
        }
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        hover_.reset();
        arrow_hover_ = 0;
        Refresh(false);
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        keyboard_ = false;
        SetFocus();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (const int direction = arrow_at(e.GetPosition()))
            shift_month(direction);
        else if (const auto day = day_at(e.GetPosition()))
            choose(day);
    });
}

void MonthCalendar::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

wxSize MonthCalendar::DoGetBestClientSize() const {
    return FromDIP(wxSize(7 * cell_width, header + weekdays + weeks() * cell_height));
}

int MonthCalendar::weeks() const {
    const auto first = first_shown();
    const int cells =
        int(weekday{sys_days{first}}.c_encoding() + unsigned((first.year() / first.month() / last).day()));
    return (cells + 6) / 7;
}

CivilDate MonthCalendar::first_shown() const {
    return focus_.year() / focus_.month() / 1;
}

wxRect MonthCalendar::cell(int index) const {
    return {FromDIP(index % 7 * cell_width), FromDIP(header + weekdays + index / 7 * cell_height),
            FromDIP(cell_width), FromDIP(cell_height)};
}

wxRect MonthCalendar::arrow(int direction) const {
    const int side = FromDIP(28);
    return {GetClientSize().x - (direction < 0 ? 2 * side + FromDIP(4) : side), FromDIP(6), side, side};
}

int MonthCalendar::arrow_at(wxPoint point) const {
    for (const int direction : {-1, 1})
        if (arrow(direction).Contains(point))
            return direction;
    return 0;
}

std::optional<CivilDate> MonthCalendar::day_at(wxPoint point) const {
    const auto first = first_shown();
    const int offset = int(weekday{sys_days{first}}.c_encoding());
    const int length = int(unsigned((first.year() / first.month() / last).day()));
    for (int index = 0; index < 42; ++index)
        if (cell(index).Contains(point)) {
            const int day = index - offset + 1;
            if (day < 1 || day > length)
                return std::nullopt;
            return first.year() / first.month() / unsigned(day);
        }
    return std::nullopt;
}

void MonthCalendar::focus(CivilDate date) {
    if (!supported(date))
        return;
    const int before = weeks();
    focus_ = date;
    hover_.reset();
    if (weeks() != before) {
        InvalidateBestSize();
        SetMinSize(GetBestSize());
        if (resized_)
            resized_();
    }
    Refresh(false);
}

void MonthCalendar::shift_month(int months) {
    const auto month = year_month{focus_.year() / focus_.month()} + std::chrono::months{months};
    // The same day of the month, or the month's last day when it is shorter.
    focus(month / std::min(focus_.day(), (month / last).day()));
}

void MonthCalendar::choose(std::optional<CivilDate> date) {
    if (chosen_)
        chosen_(date);
}

void MonthCalendar::key(wxKeyEvent& e) {
    keyboard_ = true;
    switch (e.GetKeyCode()) {
    case WXK_LEFT:
    case WXK_RIGHT:
    case WXK_UP:
    case WXK_DOWN: {
        const int step = e.GetKeyCode() == WXK_LEFT    ? -1
                         : e.GetKeyCode() == WXK_RIGHT ? 1
                         : e.GetKeyCode() == WXK_UP    ? -7
                                                       : 7;
        focus(CivilDate{sys_days{focus_} + days{step}});
        return;
    }
    case WXK_PAGEUP:
        shift_month(-1);
        return;
    case WXK_PAGEDOWN:
        shift_month(1);
        return;
    case WXK_HOME:
        focus(local_civil_date());
        return;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
    case WXK_SPACE:
        choose(focus_);
        return;
    case WXK_ESCAPE:
        choose(std::nullopt);
        return;
    default:
        e.Skip();
    }
}

void MonthCalendar::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors_.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const double scale = FromDIP(100) / 100.0;
    double width = 0, height = 0;
    // The month, and the arrows to the months before and after it.
    gc->SetFont(body_font(15), colors_.ink);
    const auto title = ui::utf8(month_swedish(focus_));
    gc->GetTextExtent(title, &width, &height);
    gc->DrawText(title, FromDIP(6), (FromDIP(header) - height) / 2);
    for (const int direction : {-1, 1}) {
        const auto area = arrow(direction);
        if (arrow_hover_ == direction) {
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(with_alpha(colors_.ink, 20)));
            gc->DrawRoundedRectangle(area.x, area.y, area.width, area.height, 6 * scale);
        }
        gc->SetPen(gc->CreatePen(
            wxGraphicsPenInfo(colors_.ink).Width(1.6 * scale).Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
        const double x = area.x + area.width / 2.0, y = area.y + area.height / 2.0, arm = 4 * scale;
        auto path = gc->CreatePath();
        path.MoveToPoint(x - direction * arm / 2, y - arm);
        path.AddLineToPoint(x + direction * arm / 2, y);
        path.AddLineToPoint(x - direction * arm / 2, y + arm);
        gc->StrokePath(path);
    }
    // Weekday initials, Sunday first.
    static const char* names[] = {"sö", "må", "ti", "on", "to", "fr", "lö"};
    gc->SetFont(ui_font(9), colors_.muted);
    for (int column = 0; column < 7; ++column) {
        const auto name = ui::utf8(names[column]);
        gc->GetTextExtent(name, &width, &height);
        gc->DrawText(name, FromDIP(column * cell_width) + (FromDIP(cell_width) - width) / 2,
                     FromDIP(header) + (FromDIP(weekdays) - height) / 2);
    }
    const auto first = first_shown();
    const int offset = int(weekday{sys_days{first}}.c_encoding());
    const int days_in_month = int(unsigned((first.year() / first.month() / last).day()));
    const auto today = local_civil_date();
    for (int day = 1; day <= days_in_month; ++day) {
        const int index = offset + day - 1;
        const CivilDate date = first.year() / first.month() / unsigned(day);
        const auto area = cell(index);
        const double x = area.x + 3 * scale, y = area.y + 2 * scale, w = area.width - 6 * scale,
                     h = area.height - 4 * scale, radius = 6 * scale;
        const bool selected = date == selected_;
        gc->SetPen(*wxTRANSPARENT_PEN);
        if (selected || hover_ == date) {
            gc->SetBrush(wxBrush(selected ? with_alpha(colors_.accent, 44) : with_alpha(colors_.ink, 20)));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        if (date == today) {
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.muted).Width(1.1 * scale)));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        if (date == focus_ && keyboard_ && HasFocus()) {
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.ink).Width(1.6 * scale)));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        auto font = ui_font(11);
        if (selected)
            font.SetWeight(wxFONTWEIGHT_MEDIUM);
        gc->SetFont(font, selected || index % 7 == 0 ? colors_.accent : colors_.ink);
        const auto number = wxString::Format("%d", day);
        gc->GetTextExtent(number, &width, &height);
        gc->DrawText(number, area.x + (area.width - width) / 2, area.y + (area.height - height) / 2);
    }
}
} // namespace ortho
