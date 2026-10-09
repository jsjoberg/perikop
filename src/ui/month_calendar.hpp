#pragma once

#include "ui/theme.hpp"
#include <functional>
#include <optional>
#include <wx/control.h>

namespace ortho {
// A month of civil dates for the start page, drawn in the theme's colours.
// Weeks start on Sunday, as in the parish calendar, and Sundays use the accent
// colour. A click or Return chooses a day; Escape closes without one.
class MonthCalendar final : public wxControl {
public:
    MonthCalendar(wxWindow* parent, CivilDate selected, std::function<void(std::optional<CivilDate>)> chosen);
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
    void paint(wxPaintEvent&);
    void key(wxKeyEvent&);
    void focus(CivilDate);
    void shift_month(int months);
    void choose(std::optional<CivilDate>);
    CivilDate first_shown() const;
    int weeks() const;
    wxRect cell(int index) const;
    std::optional<CivilDate> day_at(wxPoint) const;
    // -1 or 1 for the previous or next month's arrow, else 0.
    int arrow_at(wxPoint) const;
    wxRect arrow(int direction) const;
    CivilDate selected_, focus_;
    std::function<void(std::optional<CivilDate>)> chosen_;
    std::function<void()> resized_;
    Palette colors_;
    std::optional<CivilDate> hover_;
    int arrow_hover_ = 0;
    // The focus ring shows once the keyboard is used, not after a click.
    bool keyboard_ = false;
};
} // namespace ortho
