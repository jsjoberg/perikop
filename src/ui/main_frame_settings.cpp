#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
namespace ortho {
void MainFrame::show_settings() {
    if (navigation_.current().screen == Screen::Settings)
        return;
    auto* page =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetScrollRate(0, FromDIP(zoomed(12)));
    fill_settings(page);
    show_page(page, Screen::Settings, ui::utf8("Inställningar"));
}
void MainFrame::fill_settings(wxScrolledWindow* page) {
    // Native radio buttons and a check box, so screen readers and arrow keys work as everywhere else.
    page->DestroyChildren();
    auto* content = new wxPanel(page);
    auto* entries = new wxBoxSizer(wxVERTICAL);
    content->SetSizer(entries);
    const auto choices = [&](const char* heading, std::initializer_list<const char*> names, int chosen,
                             const std::function<void(int)>& choose) {
        entries->Add(ui::label(content, ui::utf8(heading), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        int i = 0;
        for (const auto* name : names) {
            auto* radio = new wxRadioButton(content, wxID_ANY, ui::utf8(name), wxDefaultPosition,
                                            wxDefaultSize, i ? 0 : wxRB_GROUP);
            radio->SetFont(ui_font());
            radio->SetValue(i == chosen);
            radio->Bind(wxEVT_RADIOBUTTON, [choose, i](wxCommandEvent&) {
                choose(i);
            });
            entries->Add(radio, 0, wxBOTTOM, FromDIP(zoomed(6)));
            ++i;
        }
        entries->AddSpacer(FromDIP(zoomed(18)));
    };
    choices("TEMA", {"Systemets tema", "Ljust tema", "Mörkt tema"}, int(settings_.theme), [this](int i) {
        settings_.theme = static_cast<Theme>(i);
        apply_settings();
    });
    const auto day_changed = [this] {
        clear_preparation();
        refresh_day();
        apply_settings();
    };
    choices("KALENDER", {"Gregoriansk / reviderad juliansk", "Juliansk"},
            settings_.calendar == CalendarStyle::Old, [this, day_changed](int i) {
                settings_.calendar = i ? CalendarStyle::Old : CalendarStyle::New;
                day_changed();
            });
    choices("LÄSORDNING", {"Grekiska läsordningen", "Slaviska läsordningen"},
            settings_.tradition == Tradition::Slavic, [this, day_changed](int i) {
                settings_.tradition = i ? Tradition::Slavic : Tradition::Greek;
                day_changed();
            });
    // Read-aloud is Swedish only.
    choices("RÖST FÖR UPPLÄSNING", {"Alice", "Björn"}, settings_.speech_voice == "bjorn", [this](int i) {
        clear_preparation();
        settings_.speech_voice = i ? "bjorn" : "alice";
        speech_->set_voice(settings_.speech_voice);
        apply_settings();
    });
    auto* highlight = new wxCheckBox(content, wxID_ANY, ui::utf8("Markera texten som läses upp"));
    highlight->SetFont(ui_font());
    highlight->SetValue(settings_.speech_highlight);
    highlight->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        settings_.speech_highlight = event.IsChecked();
        apply_settings();
    });
    entries->Add(highlight);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(content, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, FromDIP(zoomed(40)));
    page->SetSizer(outer);
    ui::recolor(page, palette(settings_.theme));
    page->FitInside();
    page->Layout();
}
} // namespace ortho
