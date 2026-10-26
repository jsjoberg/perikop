#include "perikop_version.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <algorithm>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
namespace ortho {
void MainFrame::show_updates() {
    if (voice_preview_)
        stop_speech(false);
    settings_category_ = 5;
    if (navigation_.current().screen == Screen::Settings)
        fill_settings(static_cast<wxScrolledWindow*>(current_page()));
    else
        show_settings();
    const auto state = updater_->status().state;
    if (state == UpdateStatus::State::Idle || state == UpdateStatus::State::Current ||
        state == UpdateStatus::State::Failed)
        updater_->check(settings_.update_channel);
}
void MainFrame::add_update_controls(wxWindow* parent, wxBoxSizer* layout) {
    layout->Add(ui::label(parent, ui::utf8("Installerad version: ") + PERIKOP_VERSION), 0, wxBOTTOM,
                FromDIP(zoomed(12)));
    auto* text = ui::label(parent, {}, 10);
    layout->Add(text, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(12)));
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* button = ui::button(parent, ui::utf8("Sök efter uppdateringar"), [this] {
        update_clicked();
    });
    auto* cancel = ui::button(parent, "Avbryt", [this] {
        updater_->cancel();
    });
    cancel->SetToolTip(ui::utf8("Sluta vänta. En påbörjad hämtning avslutas först."));
    row->Add(button, 0, wxRIGHT, FromDIP(zoomed(8)));
    row->Add(cancel);
    layout->Add(row, 0, wxBOTTOM, FromDIP(zoomed(18)));
    update_button_ = button;
    update_cancel_ = cancel;
    update_text_ = text;
    show_update_status();
}
void MainFrame::show_update_status() {
    auto* button = update_button_.get();
    auto* cancel = update_cancel_.get();
    auto* text = static_cast<wxStaticText*>(update_text_.get());
    if (!button || !cancel || !text)
        return;
    using State = UpdateStatus::State;
    const auto& status = updater_->status();
    const auto version = ui::utf8(status.offer.version);
    auto label = ui::utf8("Sök efter uppdateringar");
    wxString line;
    bool enabled = true;
    switch (status.state) {
    case State::Unavailable:
        enabled = false;
        line = ui::utf8(status.message);
        break;
    case State::Idle:
        break;
    case State::Current:
        line = ui::utf8("Du har den senaste versionen.");
        break;
    case State::Checking:
        label = ui::utf8("Söker…");
        enabled = false;
        line = ui::utf8("Söker efter uppdateringar på GitHub.");
        break;
    case State::Available:
        label = ui::utf8("Hämta version ") + version;
        line = ui::utf8("Version ") + version + ui::utf8(" finns att hämta");
        if (status.offer.size)
            line += ", " + std::to_string((status.offer.size + 999'999) / 1'000'000) + " MB";
        line += status.offer.downgrade ? ui::utf8(". Den är äldre än den du har.") : wxString(".");
        break;
    case State::Downloading:
        label = ui::utf8("Hämtar…");
        enabled = false;
        line = ui::utf8("Hämtar version ") + version + ": " + std::to_string(status.progress) + " %";
        break;
    case State::Cancelling:
        label = ui::utf8("Avbryter…");
        enabled = false;
        line = ui::utf8("Avbryter. Det som redan har börjat hämtas avslutas först.");
        break;
    case State::Ready:
        label = ui::utf8("Starta om och installera");
        line =
            ui::utf8("Version ") + version + ui::utf8(" är hämtad. Perikop startar om när den installeras.");
        break;
    case State::Failed:
        label = ui::utf8("Försök igen");
        line = ui::utf8("Det gick inte: ") + ui::utf8(status.message);
        break;
    }
    if (button->GetLabel() != label) {
        button->SetLabel(label);
        button->InvalidateBestSize();
    }
    button->Enable(enabled);
    cancel->Show(status.state == State::Checking || status.state == State::Downloading);
    text->SetLabelText(line);
    auto* parent = button->GetParent();
    text->Wrap(std::max(1, std::min(parent->GetClientSize().x, FromDIP(zoomed(440)))));
    parent->Layout();
    static_cast<wxScrolledWindow*>(parent->GetParent())->FitInside();
}
void MainFrame::update_clicked() {
    using State = UpdateStatus::State;
    const auto& status = updater_->status();
    if (status.state == State::Available) {
        if (status.offer.downgrade &&
            wxMessageBox(ui::utf8("Version " + status.offer.version +
                                  " är äldre än den du har. Vill du byta till den?"),
                         ui::utf8("Äldre version"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
            return;
        updater_->download();
    } else if (status.state == State::Ready) {
        // Velopack installs once the program has closed, then starts it again.
        if (updater_->apply())
            Close();
    } else
        updater_->check(settings_.update_channel);
}
} // namespace ortho
