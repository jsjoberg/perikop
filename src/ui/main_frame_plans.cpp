#include "core/reading_plan.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <wx/msgdlg.h>
#include <wx/wrapsizer.h>
namespace ortho {
std::string MainFrame::day_key(const Reading& reading) const {
    // The date and the lectionary's own coordinates, which do not change with the language.
    auto key = "day:" + date_iso(selected_.date());
    for (const auto& passage : reading.segments())
        key += ":" + passage.book + " " + std::to_string(passage.first.chapter) + ":" +
               std::to_string(passage.first.verse) + passage.first.suffix + "-" +
               std::to_string(passage.last.chapter) + ":" + std::to_string(passage.last.verse) +
               passage.last.suffix;
    return key;
}
void MainFrame::open_tracked(const Reading& reading, Tracked item) {
    open_reading(reading);
    tracked_ = std::move(item);
}
void MainFrame::offer_completion(const Tracked& item, bool listened) {
    if (user_.completed().contains(item.key))
        return;
    wxMessageDialog question(
        this,
        (listened ? ui::utf8("Du har lyssnat igenom ") : ui::utf8("Du har läst igenom ")) + item.title +
            ui::utf8(". Vill du markera den som läst?"),
        ui::utf8("Markera som läst"), wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
    question.SetYesNoLabels(ui::utf8("Markera som läst"), ui::utf8("Inte nu"));
    if (question.ShowModal() != wxID_YES)
        return;
    try {
        user_.complete(item.key);
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8("Markeringen kunde inte sparas"), wxOK | wxICON_ERROR,
                     this);
        return;
    }
    refresh_day();
}
void MainFrame::add_plans() {
    // Plans are counted in parts, not days, so nobody falls behind.
    const auto completed = user_.completed();
    entries_->AddSpacer(FromDIP(20));
    entries_->Add(ui::label(home_content_, ui::utf8("LÄSPLANER"), 10), 0, wxBOTTOM, FromDIP(14));
    for (const auto& plan : reading_plans()) {
        std::size_t done = 0;
        for (std::size_t i = 0; i < plan.parts.size(); ++i)
            done += completed.contains(plan_key(plan, i));
        auto* header = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
        auto* title = new wxStaticText(home_content_, wxID_ANY, ui::utf8(plan.title));
        title->SetFont(body_font(18));
        header->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        header->Add(
            ui::label(home_content_, std::to_string(done) + " av " + std::to_string(plan.parts.size()), 11),
            0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        auto* reset = ui::button(home_content_, ui::utf8("Börja om"), [this, plan] {
            wxMessageDialog question(
                this, ui::utf8("Vill du ta bort alla markeringar i ") + ui::utf8(plan.title) + "?",
                ui::utf8("Börja om"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
            question.SetYesNoLabels(ui::utf8("Börja om"), ui::utf8("Avbryt"));
            if (question.ShowModal() != wxID_YES)
                return;
            try {
                user_.forget("plan:" + plan.id + ":");
            } catch (const std::exception& e) {
                wxMessageBox(ui::utf8(e.what()), ui::utf8("Markeringarna kunde inte tas bort"),
                             wxOK | wxICON_ERROR, this);
                return;
            }
            // The button belongs to the page that is rebuilt.
            CallAfter([this] {
                refresh_day();
            });
        });
        reset->Enable(done > 0);
        reset->SetToolTip(ui::utf8("Ta bort alla markeringar i planen"));
        header->Add(reset, 0, wxALIGN_CENTER_VERTICAL);
        entries_->Add(header, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
        auto* about = ui::label(home_content_, ui::utf8(plan.description), 11);
        home_labels_.emplace_back(about, about->GetLabel());
        entries_->Add(about, 0, wxBOTTOM, FromDIP(10));
        if (done == plan.parts.size())
            entries_->Add(ui::label(home_content_, ui::utf8("🎉 ✨ Hela planen är läst! 🕊️ 🎊"), 16), 0,
                          wxBOTTOM, FromDIP(10));
        auto* grid = new wxGridSizer(10, FromDIP(4), FromDIP(4));
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const bool read = completed.contains(plan_key(plan, i));
            const wxString number = std::to_string(i + 1);
            const auto label = ui::utf8(plan_label(plan.parts[i]));
            auto* tile = new SymbolButton(
                home_content_, read ? std::variant<Symbol, wxString>{Symbol::Check} : number, "Del " + number,
                [this, plan, i, number, label] {
                    try {
                        open_tracked(plan_reading(corpus_, plan.parts[i], settings_.primary),
                                     {plan_key(plan, i), "del " + number + ui::utf8(" i ") +
                                                             ui::utf8(plan.title) + " (" + label + ")"});
                    } catch (const std::exception& e) {
                        wxMessageBox(ui::utf8(e.what()), ui::utf8(plan.title), wxOK | wxICON_INFORMATION,
                                     this);
                    }
                });
            tile->checked(read);
            tile->SetToolTip("Del " + number + ui::utf8(" · ") + label +
                             (read ? ui::utf8(" · läst") : wxString{}));
            grid->Add(tile);
            plan_tiles_.push_back(tile);
        }
        entries_->Add(grid, 0, wxBOTTOM, FromDIP(30));
    }
}
} // namespace ortho
