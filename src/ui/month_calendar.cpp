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
std::string month_name(CivilDate date) {
    const auto month_and_year = month_swedish(date);
    return month_and_year.substr(0, month_and_year.find_last_of(' '));
}
// The same day in another month, or that month's last day when it is shorter.
CivilDate same_day(year_month month, CivilDate date) {
    return month / std::min(date.day(), (month / last).day());
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
        const int arrow = arrow_at(e.GetPosition()), title = title_at(e.GetPosition());
        if (day != hover_ || arrow != arrow_hover_ || title != title_hover_) {
            hover_ = day;
            arrow_hover_ = arrow;
            title_hover_ = title;
            update_description();
            Refresh(false);
        }
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        hover_.reset();
        arrow_hover_ = 0;
        title_hover_ = -1;
        UnsetToolTip();
        Refresh(false);
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        keyboard_ = false;
        SetFocus();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (const int direction = arrow_at(e.GetPosition()))
            shift_month(months_for(direction));
        else if (const int group = title_at(e.GetPosition()); group >= 0) {
            const auto grid = group == 0 ? View::Months : View::Years;
            show(view_ == grid ? View::Days : grid);
        } else if (const auto day = day_at(e.GetPosition())) {
            if (view_ == View::Days)
                choose(day);
            else {
                focus(*day);
                show(View::Days);
            }
        }
    });
}

void MonthCalendar::apply(const Palette& colors) {
    colors_ = colors;
    Refresh(false);
}

wxSize MonthCalendar::DoGetBestClientSize() const {
    return FromDIP(zoomed(wxSize(7 * cell_width, header + weekdays + weeks() * cell_height + legend)));
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
    if (hover_ && view_ == View::Days)
        tooltip = description(*hover_);
    else if (arrow_hover_)
        tooltip =
            ui::utf8(std::abs(arrow_hover_) != 12 ? (arrow_hover_ < 0 ? "Föregående månad" : "Nästa månad")
                     : view_ == View::Years       ? (arrow_hover_ < 0 ? "Tidigare år" : "Senare år")
                                                  : (arrow_hover_ < 0 ? "Föregående år" : "Nästa år"));
    else if (title_hover_ >= 0)
        tooltip = ui::utf8(title_hover_ == 0 ? "Välj månad" : "Välj år");
    else if (keyboard_ && view_ == View::Days)
        tooltip = description(focus_);
    if (tooltip != GetToolTipText())
        SetToolTip(tooltip);
}

int MonthCalendar::weeks() const {
    if (view_ != View::Days)
        return grid_weeks_;
    const auto first = first_shown();
    const int cells =
        int(weekday{sys_days{first}}.c_encoding() + unsigned((first.year() / first.month() / last).day()));
    return (cells + 6) / 7;
}

CivilDate MonthCalendar::first_shown() const {
    return focus_.year() / focus_.month() / 1;
}

wxRect MonthCalendar::cell(int index) const {
    return {FromDIP(zoomed(index % 7 * cell_width)),
            FromDIP(zoomed(header + weekdays + index / 7 * cell_height)), FromDIP(zoomed(cell_width)),
            FromDIP(zoomed(cell_height))};
}

wxRect MonthCalendar::arrow(int step) const {
    const int side = FromDIP(zoomed(28)), half = GetClientSize().x / 2;
    const int group = std::abs(step) == 12 ? 1 : 0;
    return {group * half + (step < 0 ? FromDIP(zoomed(4)) : half - side - FromDIP(zoomed(4))),
            FromDIP(zoomed(6)), side, side};
}

wxRect MonthCalendar::title(int group) const {
    const int half = GetClientSize().x / 2, inset = FromDIP(zoomed(34));
    return {group * half + inset, FromDIP(zoomed(6)), half - 2 * inset, FromDIP(zoomed(28))};
}

int MonthCalendar::title_at(wxPoint point) const {
    for (const int group : {0, 1})
        if (title(group).Contains(point))
            return group;
    return -1;
}

void MonthCalendar::show(View view) {
    const int before = weeks();
    if (view_ == View::Days)
        grid_weeks_ = before;
    view_ = view;
    hover_.reset();
    update_description();
    fit(before);
    Refresh(false);
}

wxRect MonthCalendar::grid_cell(int index) const {
    // Three columns and four rows in place of the weekday names and the weeks.
    const int width = 7 * cell_width / 3, height = (weekdays + weeks() * cell_height) / 4;
    return {FromDIP(zoomed(index % 3 * width)), FromDIP(zoomed(header + index / 3 * height)),
            FromDIP(zoomed(width)), FromDIP(zoomed(height))};
}

CivilDate MonthCalendar::grid_date(int index) const {
    if (view_ == View::Months)
        return same_day(focus_.year() / month{unsigned(index + 1)}, focus_);
    // Pages of twelve years from the calendar's first.
    const int first = first_calendar_year + (int(focus_.year()) - first_calendar_year) / 12 * 12;
    return same_day(year{first + index} / focus_.month(), focus_);
}

int MonthCalendar::months_for(int step) const {
    return view_ == View::Years && std::abs(step) == 12 ? 12 * step : step;
}

int MonthCalendar::arrow_at(wxPoint point) const {
    for (const int step : {-1, 1, -12, 12})
        if (supported((year_month{focus_.year() / focus_.month()} + months{months_for(step)}) / 1) &&
            arrow(step).Contains(point))
            return step;
    return 0;
}

std::optional<CivilDate> MonthCalendar::day_at(wxPoint point) const {
    if (view_ != View::Days) {
        for (int index = 0; index < 12; ++index)
            if (grid_cell(index).Contains(point) && supported(grid_date(index)))
                return grid_date(index);
        return std::nullopt;
    }
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
    fit(before);
    Refresh(false);
}

void MonthCalendar::fit(int weeks_before) {
    if (weeks() == weeks_before)
        return;
    InvalidateBestSize();
    SetMinSize(GetBestSize());
    if (resized_)
        resized_();
}

void MonthCalendar::shift_month(int months) {
    focus(same_day(year_month{focus_.year() / focus_.month()} + std::chrono::months{months}, focus_));
}

void MonthCalendar::choose(std::optional<CivilDate> date) {
    if (chosen_)
        chosen_(date);
}

void MonthCalendar::key(wxKeyEvent& e) {
    keyboard_ = true;
    if (view_ != View::Days) {
        // Arrows move through the grid's three columns; Return or Escape goes back to the days.
        const int unit = view_ == View::Months ? 1 : 12;
        switch (e.GetKeyCode()) {
        case WXK_LEFT:
        case WXK_RIGHT:
        case WXK_UP:
        case WXK_DOWN:
            shift_month(unit * (e.GetKeyCode() == WXK_LEFT    ? -1
                                : e.GetKeyCode() == WXK_RIGHT ? 1
                                : e.GetKeyCode() == WXK_UP    ? -3
                                                              : 3));
            return;
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
        case WXK_SPACE:
        case WXK_ESCAPE:
            show(View::Days);
            return;
        default:
            break;
        }
    }
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
    const double scale = FromDIP(zoomed(100)) / 100.0;
    double width = 0, height = 0;
    // Independent month and year groups, each with arrows around its label.
    const std::array labels{ui::utf8(month_name(focus_) + (view_ == View::Months ? " ▴" : " ▾")),
                            wxString::Format("%d", int(focus_.year())) +
                                ui::utf8(view_ == View::Years ? " ▴" : " ▾")};
    const int half = GetClientSize().x / 2;
    for (std::size_t group = 0; group < labels.size(); ++group) {
        if (title_hover_ == int(group)) {
            const auto area = title(int(group));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(colors_.surface));
            gc->DrawRoundedRectangle(area.x, area.y, area.width, area.height, 6 * scale);
        }
        int points = 12;
        gc->SetFont(ui_font(points), colors_.ink);
        gc->GetTextExtent(labels[group], &width, &height);
        const int available = half - 2 * FromDIP(zoomed(34));
        while (width > available && points > 9) {
            gc->SetFont(ui_font(--points), colors_.ink);
            gc->GetTextExtent(labels[group], &width, &height);
        }
        gc->DrawText(labels[group], group * half + (half - width) / 2,
                     (FromDIP(zoomed(header)) - height) / 2);
    }
    for (const int step : {-1, 1, -12, 12}) {
        const auto area = arrow(step);
        const bool enabled =
            supported((year_month{focus_.year() / focus_.month()} + months{months_for(step)}) / 1);
        if (enabled && arrow_hover_ == step) {
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(colors_.surface));
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
    const auto today = local_civil_date();
    // A day, month, or year: tinted when selected, ringed when current, outlined under the keyboard.
    const auto draw_cell = [&](const wxRect& area, const wxString& text, CivilDate date, bool selected,
                               bool current, bool accent, double lift) {
        const double x = area.x + 3 * scale, y = area.y + 2 * scale, w = area.width - 6 * scale,
                     h = area.height - 4 * scale, radius = 6 * scale;
        gc->SetPen(*wxTRANSPARENT_PEN);
        if (selected || hover_ == date) {
            gc->SetBrush(wxBrush(selected ? with_alpha(colors_.accent, 44) : colors_.surface));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        if (current) {
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.today).Width(1.7 * scale)));
            gc->DrawRoundedRectangle(x + 2 * scale, y + 2 * scale, w - 4 * scale, h - 4 * scale,
                                     radius - scale);
        }
        if (date == focus_ && keyboard_ && HasFocus()) {
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors_.ink).Width(1.6 * scale)));
            gc->DrawRoundedRectangle(x, y, w, h, radius);
        }
        auto font = ui_font(11);
        if (selected || current)
            font.SetWeight(wxFONTWEIGHT_MEDIUM);
        gc->SetFont(font, accent ? colors_.accent : colors_.ink);
        gc->GetTextExtent(text, &width, &height);
        gc->DrawText(text, area.x + (area.width - width) / 2, area.y + (area.height - height) / 2 - lift);
    };
    if (view_ != View::Days) {
        for (int index = 0; index < 12; ++index) {
            const auto date = grid_date(index);
            if (!supported(date))
                continue;
            const bool current = view_ == View::Months
                                     ? date.year() == today.year() && date.month() == today.month()
                                     : date.year() == today.year();
            draw_cell(grid_cell(index),
                      view_ == View::Months ? ui::utf8(month_name(date))
                                            : wxString::Format("%d", int(date.year())),
                      date, date == focus_, current, date == focus_, 0);
        }
    } else {
        // Weekday initials, Sunday first.
        static const char* names[] = {"sö", "må", "ti", "on", "to", "fr", "lö"};
        gc->SetFont(ui_font(9), colors_.muted);
        for (int column = 0; column < 7; ++column) {
            const auto name = ui::utf8(names[column]);
            gc->GetTextExtent(name, &width, &height);
            gc->DrawText(name,
                         FromDIP(zoomed(column * cell_width)) + (FromDIP(zoomed(cell_width)) - width) / 2,
                         FromDIP(zoomed(header)) + (FromDIP(zoomed(weekdays)) - height) / 2);
        }
        const auto first = first_shown();
        const int offset = int(weekday{sys_days{first}}.c_encoding());
        const int days_in_month = int(unsigned((first.year() / first.month() / last).day()));
        for (int day = 1; day <= days_in_month; ++day) {
            const int index = offset + day - 1;
            const CivilDate date = first.year() / first.month() / unsigned(day);
            const auto area = cell(index);
            const bool selected = date == selected_;
            draw_cell(area, wxString::Format("%d", day), date, selected, date == today,
                      selected || index % 7 == 0, 2 * scale);
            const double center = area.x + area.width / 2.0, bottom = area.GetBottom() - 6 * scale;
            const auto& services = days_.at(date).marked;
            const int count = std::ranges::count(services, true);
            int position = 0;
            for (std::size_t service = 0; service < services.size(); ++service)
                if (services[service])
                    service_marker(*gc, center + (position++ - (count - 1) / 2.0) * 8 * scale, bottom,
                                   2.3 * scale, colors_.services[service], service);
        }
    }
    // Shapes keep the meaning visible without relying on colour alone.
    const double bottom = FromDIP(zoomed(header + weekdays + weeks() * cell_height));
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
    key(8 * scale, bottom + 10 * scale, colors_.today, "Idag", 0);
    key(144 * scale, bottom + 10 * scale, colors_.services[0], "Ortros", 1);
    key(8 * scale, bottom + 27 * scale, colors_.services[1], "Vesper", 2);
    key(144 * scale, bottom + 27 * scale, colors_.services[2], ui::utf8("Övriga gudstjänster"), 3);
}
} // namespace ortho
