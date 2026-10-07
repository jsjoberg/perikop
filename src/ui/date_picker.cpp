#include "ui/date_picker.hpp"
#include "ui/controls.hpp"
#include <wx/calctrl.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
namespace ortho {
std::optional<CivilDate> pick_civil_date(wxWindow* parent, CivilDate current, Theme theme) {
    wxDialog dialog(parent, wxID_ANY, ui::utf8("Välj civilt datum"), wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* calendar = new wxCalendarCtrl(
        &dialog, wxID_ANY,
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
    sizer->Add(calendar, 0, wxALL, dialog.FromDIP(20));

    sizer->Add(ui::button(&dialog, "Aktuellt datum",
                          [calendar] {
                              calendar->SetDate(wxDateTime::Today());
                          }),
               0, wxALIGN_CENTER | wxBOTTOM, dialog.FromDIP(12));
    auto* buttons = dialog.CreateButtonSizer(wxOK | wxCANCEL);
    if (auto* cancel = wxDynamicCast(dialog.FindWindow(wxID_CANCEL), wxButton))
        cancel->SetLabel("Avbryt");
    sizer->Add(buttons, 0, wxALIGN_RIGHT | wxALL, dialog.FromDIP(12));
    dialog.SetSizerAndFit(sizer);
    ui::recolor(&dialog, palette(theme));
    if (dialog.ShowModal() != wxID_OK)
        return std::nullopt;
    const auto value = calendar->GetDate();
    return std::chrono::year{value.GetYear()} / (int(value.GetMonth()) + 1) / value.GetDay();
}
} // namespace ortho
