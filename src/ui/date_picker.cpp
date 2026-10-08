#include "ui/date_picker.hpp"
#include "ui/controls.hpp"
#include <wx/calctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
namespace ortho {
wxWindow* make_date_page(wxWindow* parent, CivilDate current, Theme theme,
                         const std::function<void(CivilDate)>& selected) {
    auto* page = new wxPanel(parent);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* calendar = new wxCalendarCtrl(
        page, wxID_ANY,
        wxDateTime(unsigned(current.day()), static_cast<wxDateTime::Month>(unsigned(current.month()) - 1),
                   int(current.year())),
        wxDefaultPosition, wxDefaultSize, wxCAL_SUNDAY_FIRST);
    // Weeks start on Sunday, as in the parish calendar; Sundays are red.
    const auto mark_sundays = [calendar] {
        const auto shown = calendar->GetDate();
        for (unsigned day = 1; day <= wxDateTime::GetNumberOfDays(shown.GetMonth(), shown.GetYear()); ++day) {
            if (wxDateTime(day, shown.GetMonth(), shown.GetYear()).GetWeekDay() == wxDateTime::Sun)
                calendar->SetAttr(day, new wxCalendarDateAttr(*wxRED));
            else
                calendar->ResetAttr(day);
        }
        calendar->Refresh();
    };
    calendar->Bind(wxEVT_CALENDAR_PAGE_CHANGED, [mark_sundays](wxCalendarEvent& event) {
        mark_sundays();
        event.Skip();
    });
    mark_sundays();
    sizer->AddSpacer(page->FromDIP(48));
    sizer->Add(ui::label(page, ui::utf8("Välj datum"), 18), 0, wxALIGN_CENTER | wxBOTTOM, page->FromDIP(20));
    sizer->Add(calendar, 0, wxALIGN_CENTER | wxALL, page->FromDIP(20));

    sizer->Add(ui::button(page, "Idag",
                          [calendar, mark_sundays] {
                              calendar->SetDate(wxDateTime::Today());
                              mark_sundays();
                          }),
               0, wxALIGN_CENTER | wxBOTTOM, page->FromDIP(12));
    const auto choose = [calendar, selected] {
        const auto value = calendar->GetDate();
        selected(std::chrono::year{value.GetYear()} / (int(value.GetMonth()) + 1) / value.GetDay());
    };
    sizer->Add(ui::button(page, ui::utf8("Visa dagens läsningar"),
                          [page, choose] {
                              page->CallAfter(choose);
                          }),
               0, wxALIGN_CENTER | wxBOTTOM, page->FromDIP(20));
    calendar->Bind(wxEVT_CALENDAR_DOUBLECLICKED, [page, choose](wxCalendarEvent&) {
        page->CallAfter(choose);
    });
    page->SetSizer(sizer);
    ui::recolor(page, palette(theme));
    return page;
}
} // namespace ortho
