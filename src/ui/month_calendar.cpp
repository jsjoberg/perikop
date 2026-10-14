#include "ui/month_calendar.hpp"
#include "ui/controls.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

namespace ortho {
namespace {
using namespace std::chrono;
// Layout on a grid of device-independent pixels.
constexpr int cell_width = 40, cell_height = 34, header = 40, weekdays = 24, legend = 38;
struct CalendarColors {
    wxColour today;
    std::array<wxColour, 3> services;
};
CalendarColors calendar_colors(const Palette& colors) {
    if (colors.paper.Red() < 128)
        return {{120, 179, 222}, {{{188, 160, 223}, {215, 180, 105}, {124, 185, 149}}}};
    return {{42, 112, 160}, {{{133, 98, 171}, {174, 129, 41}, {71, 132, 101}}}};
}
void service_marker(wxGraphicsContext& gc, double x, double y, double size, const wxColour& color,
                    std::size_t service) {
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(color));
    if (service == 0)
        gc.DrawEllipse(x - size, y - size, 2 * size, 2 * size);
    else if (service == 1) {
        auto diamond = gc.CreatePath();
        diamond.MoveToPoint(x, y - size);
        diamond.AddLineToPoint(x + size, y);
        diamond.AddLineToPoint(x, y + size);
        diamond.AddLineToPoint(x - size, y);
        diamond.CloseSubpath();
        gc.FillPath(diamond);
    } else
        gc.DrawRectangle(x - size, y - size, 2 * size, 2 * size);
}
bool supported(CivilDate date) {
    return selectable_date(date);
}
} // namespace

MonthCalendar::MonthCalendar(wxWindow* parent, CivilDate selected,
                             std::function<DayReadings(CivilDate)> describe,
                             std::function<void(std::optional<CivilDate>)> chosen)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      selected_(selected), focus_(selected), describe_(std::move(describe)), chosen_(std::move(chosen)),
      colors_(palette(Theme::System)) {
    SetLabel(ui::utf8("Välj datum"));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    load_month();
    update_description();
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
            update_description();
            Refresh(false);
        }
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        hover_.reset();
        arrow_hover_ = 0;
        UnsetToolTip();
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
    return FromDIP(wxSize(7 * cell_width, header + weekdays + weeks() * cell_height + legend));
}

void MonthCalendar::load_month() {
    days_.clear();
    const auto first = first_shown();
    const auto length = unsigned((first.year() / first.month() / last).day());
    for (unsigned day = 1; day <= length; ++day) {
        const CivilDate date = first.year() / first.month() / day;
        const auto assigned = describe_(date);
        CalendarDay info{assigned.day.calendar, assigned.day.fixed_cycle, {}, {}};
        const auto add = [&](const Reading& reading, const std::string& prefix = "") {
            const auto label = reading.kind == ReadingKind::Matins    ? "Ortros"
                               : reading.kind == ReadingKind::Vespers ? "Vesper"
                               : reading.service.empty()              ? "Läsningar"
                                                                      : service_label(reading.service);
            ++info.readings[prefix + label];
            if (reading.kind == ReadingKind::Matins)
                info.marked[0] = true;
            else if (reading.kind == ReadingKind::Vespers)
                info.marked[1] = true;
            else if (reading.kind != ReadingKind::Epistle && reading.kind != ReadingKind::Gospel)
                info.marked[2] = true;
        };
        for (const auto& reading : assigned.readings)
            add(reading);
        for (const auto& variant : assigned.variants)
            if (variant.reading)
                add(*variant.reading, "Antiokisk, ");
        days_.emplace(date, std::move(info));
    }
}

wxString MonthCalendar::description(CivilDate date) const {
    const auto& day = days_.at(date);
    std::string text = date_swedish(date);
    if (date == local_civil_date())
        text += " — Idag";
    if (day.calendar == CalendarStyle::Old && day.fixed_cycle)
        text += "\nGamla stilen: " + *day.fixed_cycle;
    for (const auto& [service, count] : day.readings)
        text += "\n" + service + ": " + std::to_string(count) + (count == 1 ? " läsning" : " läsningar");
    if (day.readings.empty())
        text += "\nInga föreskrivna läsningar";
    return ui::utf8(text);
}

void MonthCalendar::update_description() {
    SetLabel(ui::utf8("Välj datum: ") + description(focus_));
    wxString tooltip;
    if (hover_)
        tooltip = description(*hover_);
    else if (arrow_hover_)
        tooltip =
            ui::utf8(std::abs(arrow_hover_) == 12 ? (arrow_hover_ < 0 ? "Föregående år" : "Nästa år")
                                                  : (arrow_hover_ < 0 ? "Föregående månad" : "Nästa månad"));
    else if (keyboard_)
        tooltip = description(focus_);
    if (tooltip != GetToolTipText())
        SetToolTip(tooltip);
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

wxRect MonthCalendar::arrow(int step) const {
    const int side = FromDIP(28), half = GetClientSize().x / 2;
    const int group = std::abs(step) == 12 ? 1 : 0;
    return {group * half + (step < 0 ? FromDIP(4) : half - side - FromDIP(4)), FromDIP(6), side, side};
}

int MonthCalendar::arrow_at(wxPoint point) const {
    for (const int step : {-1, 1, -12, 12})
        if (supported((year_month{focus_.year() / focus_.month()} + months{step}) / 1) &&
            arrow(step).Contains(point))
            return step;
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
    const auto month = first_shown();
    focus_ = date;
    hover_.reset();
    if (first_shown() != month)
        load_month();
    update_description();
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
        shift_month(e.ShiftDown() ? -12 : -1);
        return;
    case WXK_PAGEDOWN:
        shift_month(e.ShiftDown() ? 12 : 1);
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
    // Independent month and year groups, each with arrows around its label.
    const auto month_and_year = month_swedish(focus_);
    const std::array labels{ui::utf8(month_and_year.substr(0, month_and_year.find_last_of(' '))),
                            wxString::Format("%d", int(focus_.year()))};
    const int half = GetClientSize().x / 2;
    for (std::size_t group = 0; group < labels.size(); ++group) {
        int points = 12;
        gc->SetFont(ui_font(points), colors_.ink);
        gc->GetTextExtent(labels[group], &width, &height);
        const int available = half - 2 * FromDIP(34);
        while (width > available && points > 9) {
            gc->SetFont(ui_font(--points), colors_.ink);
            gc->GetTextExtent(labels[group], &width, &height);
        }
        gc->DrawText(labels[group], group * half + (half - width) / 2, (FromDIP(header) - height) / 2);
    }
    for (const int step : {-1, 1, -12, 12}) {
        const auto area = arrow(step);
        const bool enabled = supported((year_month{focus_.year() / focus_.month()} + months{step}) / 1);
        if (enabled && arrow_hover_ == step) {
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(with_alpha(colors_.ink, 20)));
            gc->DrawRoundedRectangle(area.x, area.y, area.width, area.height, 6 * scale);
        }
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(enabled ? colors_.ink : colors_.muted)
                                     .Width(1.6 * scale)
                                     .Cap(wxCAP_ROUND)
                                     .Join(wxJOIN_ROUND)));
        const int direction = step < 0 ? -1 : 1;
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
    const auto marks = calendar_colors(colors_);
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
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(marks.today).Width(1.7 * scale)));
            gc->DrawRoundedRectangle(x + 2 * scale, y + 2 * scale, w - 4 * scale, h - 4 * scale,
                                     radius - scale);
        }
        if (date == focus_ && keyboard_ && HasFocus()) {
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.ink).Width(1.6 * scale)));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        auto font = ui_font(11);
        if (selected || date == today)
            font.SetWeight(wxFONTWEIGHT_MEDIUM);
        gc->SetFont(font, selected || index % 7 == 0 ? colors_.accent : colors_.ink);
        const auto number = wxString::Format("%d", day);
        gc->GetTextExtent(number, &width, &height);
        gc->DrawText(number, area.x + (area.width - width) / 2,
                     area.y + (area.height - height) / 2 - 2 * scale);
        const double center = area.x + area.width / 2.0, bottom = area.GetBottom() - 6 * scale;
        const auto& services = days_.at(date).marked;
        const int count = std::ranges::count(services, true);
        int position = 0;
        for (std::size_t service = 0; service < services.size(); ++service)
            if (services[service])
                service_marker(*gc, center + (position++ - (count - 1) / 2.0) * 8 * scale, bottom,
                               2.3 * scale, marks.services[service], service);
    }
    // Shapes keep the meaning visible without relying on colour alone.
    const double bottom = FromDIP(header + weekdays + weeks() * cell_height);
    const auto key = [&](double x, double y, const wxColour& color, const wxString& label, int shape) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(color));
        if (shape == 0) {
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(color).Width(1.4 * scale)));
            gc->DrawRoundedRectangle(x, y - 4 * scale, 8 * scale, 8 * scale, 2 * scale);
        } else
            service_marker(*gc, x + 4 * scale, y, 3 * scale, color, std::size_t(shape - 1));
        gc->SetFont(ui_font(8), colors_.muted);
        gc->GetTextExtent(label, &width, &height);
        gc->DrawText(label, x + 13 * scale, y - height / 2);
    };
    key(8 * scale, bottom + 10 * scale, marks.today, "Idag", 0);
    key(144 * scale, bottom + 10 * scale, marks.services[0], "Ortros", 1);
    key(8 * scale, bottom + 27 * scale, marks.services[1], "Vesper", 2);
    key(144 * scale, bottom + 27 * scale, marks.services[2], ui::utf8("Övriga gudstjänster"), 3);
}
} // namespace ortho
