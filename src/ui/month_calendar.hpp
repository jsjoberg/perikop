#pragma once

#include "ui/theme.hpp"
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <wx/control.h>

namespace ortho {
// A month of civil dates for the start page, drawn in the theme's colours.
// Weeks start on Sunday, as in the parish calendar, and Sundays use the accent
// colour. A click or Return chooses a day; Escape closes without one. The month
// and year titles open grids of months and years in place of the days.
class MonthCalendar final : public wxControl {
public:
    MonthCalendar(wxWindow* parent, CivilDate selected, std::function<DayReadings(CivilDate)> describe,
                  std::function<void(std::optional<CivilDate>)> chosen);
    void apply(const Palette&);
    // The day the keyboard is on; its month is the one shown.
    CivilDate focused() const {
        return focus_;
    }
    // Called when another month needs a different number of week rows.
    void on_resize(std::function<void()> callback) {
        resized_ = std::move(callback);
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    friend class MainFrame;
    void load_month();
    wxString description(CivilDate) const;
    void update_description();
    void paint(wxPaintEvent&);
    void key(wxKeyEvent&);
    void focus(CivilDate);
    void shift_month(int months);
    void choose(std::optional<CivilDate>);
    CivilDate first_shown() const;
    int weeks() const;
    // Resizes when the number of week rows differs from before.
    void fit(int weeks_before);
    wxRect cell(int index) const;
    std::optional<CivilDate> day_at(wxPoint) const;
    // Month arrows return -1/+1; year arrows return -12/+12; otherwise zero.
    int arrow_at(wxPoint) const;
    wxRect arrow(int step) const;
    // The month (0) or year (1) title, which opens a grid of months or years; -1 elsewhere.
    int title_at(wxPoint) const;
    wxRect title(int group) const;
    enum class View { Days, Months, Years };
    void show(View);
    // The date a month or year cell moves to; each keeps the focused day where it can.
    CivilDate grid_date(int index) const;
    wxRect grid_cell(int index) const;
    // Year arrows page twelve years in the year grid.
    int months_for(int step) const;
    CivilDate selected_, focus_;
    struct CalendarDay {
        CalendarStyle calendar;
        std::optional<std::string> fixed_cycle;
        std::map<std::string, int> readings;
        // Orthros, Vespers, and other services beyond the ordinary Epistle/Gospel.
        std::array<bool, 3> marked{};
    };
    std::function<DayReadings(CivilDate)> describe_;
    std::map<CivilDate, CalendarDay> days_;
    std::function<void(std::optional<CivilDate>)> chosen_;
    std::function<void()> resized_;
    Palette colors_;
    std::optional<CivilDate> hover_;
    int arrow_hover_ = 0, title_hover_ = -1;
    View view_ = View::Days;
    // The grids keep the height the days had when one opened.
    int grid_weeks_ = 0;
    // The focus ring shows once the keyboard is used, not after a click.
    bool keyboard_ = false;
};
} // namespace ortho
