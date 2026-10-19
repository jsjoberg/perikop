#include "perikop_version.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
namespace ortho {
void MainFrame::show_updates() {
    if (auto* dialog = update_dialog_.get(); dialog && !dialog->IsBeingDeleted()) {
        dialog->Raise();
        dialog->SetFocus();
        return;
    }
    auto* dialog = new wxDialog(this, wxID_ANY, ui::utf8("Uppdateringar"));
    dialog->SetFont(ui_font());
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* text = ui::label(dialog, {}, 10);
    text->SetMinSize(FromDIP(wxSize(440, 40)));
    layout->Add(text, 0, wxEXPAND | wxALL, FromDIP(20));
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* button = ui::button(dialog, ui::utf8("Sök efter uppdateringar"), [this] {
        update_clicked();
    });
    auto* cancel = ui::button(dialog, "Avbryt", [this] {
        updater_->cancel();
    });
    cancel->SetToolTip(ui::utf8("Sluta vänta. En påbörjad hämtning avslutas först."));
    row->Add(button, 0, wxRIGHT, FromDIP(8));
    row->Add(cancel);
    layout->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    dialog->SetSizer(layout);
    update_dialog_ = dialog;
    update_button_ = button;
    update_cancel_ = cancel;
    update_text_ = text;
    dialog->Bind(wxEVT_CLOSE_WINDOW, [this, dialog](wxCloseEvent&) {
        const auto state = updater_->status().state;
        if (state == UpdateStatus::State::Checking || state == UpdateStatus::State::Downloading)
            updater_->cancel();
        dialog->Destroy();
    });
    ui::recolor(dialog, palette(settings_.theme));
    show_update_status();
    dialog->CenterOnParent();
    dialog->Show();
    const auto state = updater_->status().state;
    if (state == UpdateStatus::State::Idle || state == UpdateStatus::State::Current ||
        state == UpdateStatus::State::Failed)
        updater_->check(settings_.update_channel);
}
void MainFrame::show_update_status() {
    auto* button = update_button_.get();
    auto* cancel = update_cancel_.get();
    auto* text = static_cast<wxStaticText*>(update_text_.get());
    auto* dialog = update_dialog_.get();
    if (!dialog || dialog->IsBeingDeleted() || !button || !cancel || !text)
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
        line = ui::utf8("Den här versionen: ") + PERIKOP_VERSION;
        break;
    case State::Checking:
        label = ui::utf8("Söker…");
        enabled = false;
        line = ui::utf8("Söker efter uppdateringar på GitHub.");
        break;
    case State::Current:
        line = ui::utf8("Du har den senaste versionen, ") + PERIKOP_VERSION + ".";
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
    if (text->GetLabel() != line) {
        text->SetLabelText(line);
        text->Wrap(FromDIP(zoomed(440)));
    }
    dialog->GetSizer()->Fit(dialog);
    dialog->Layout();
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
