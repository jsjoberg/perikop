#include "core/reading_plan.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <ranges>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
namespace ortho {
namespace {
wxString plans_title() {
    return ui::utf8("Läsplaner");
}
} // namespace
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
void MainFrame::open_plan_part(const ReadingPlan& plan, std::size_t part) {
    try {
        auto reading = plan_reading(corpus_, plan.parts[part], "sv");
        Tracked item{plan_key(plan, part), "del " + std::to_string(part + 1) + ui::utf8(" i ") +
                                               ui::utf8(plan.title) + " (" +
                                               ui::utf8(plan_label(corpus_, plan.parts[part])) + ")"};
        // Closing a plans page destroys the control that asked, so open the reading after its event.
        CallAfter([this, reading = std::move(reading), item = std::move(item)] {
            close_pages();
            open_tracked(reading, item);
        });
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8(plan.title), wxOK | wxICON_INFORMATION, this);
    }
}
void MainFrame::add_next_plan() {
    const auto completed = user_.completed();
    const auto& plans = reading_plans();
    const auto done = [&](const ReadingPlan& plan) {
        return std::ranges::count_if(std::views::iota(std::size_t{0}, plan.parts.size()), [&](std::size_t i) {
            return completed.contains(plan_key(plan, i));
        });
    };
    // The plan in progress: the first one started and not finished, else the first one not finished.
    auto plan = std::ranges::find_if(plans, [&](const ReadingPlan& candidate) {
        const auto read = done(candidate);
        return read > 0 && std::size_t(read) < candidate.parts.size();
    });
    if (plan == plans.end())
        plan = std::ranges::find_if(plans, [&](const ReadingPlan& candidate) {
            return std::size_t(done(candidate)) < candidate.parts.size();
        });
    entries_->AddSpacer(FromDIP(28));
    entries_->Add(ui::label(home_content_, ui::utf8("LÄSPLAN"), 10), 0, wxBOTTOM, FromDIP(6));
    const auto add_row = [this](ReadingRow* row) {
        home_rows_.push_back(row);
        entries_->Add(row, 0, wxEXPAND);
    };
    if (plan != plans.end()) {
        std::size_t part = 0;
        while (completed.contains(plan_key(*plan, part)))
            ++part;
        add_row(new ReadingRow(home_content_,
                               ui::utf8(plan->title + ", del " + std::to_string(part + 1) + " av " +
                                        std::to_string(plan->parts.size())),
                               ui::utf8(plan_label(corpus_, plan->parts[part])), [this, &plan = *plan, part] {
                                   open_plan_part(plan, part);
                               }));
    }
    add_row(new ReadingRow(home_content_, {}, ui::utf8("Alla läsplaner"), [this] {
        CallAfter([this] {
            show_plans();
        });
    }));
    entries_->AddSpacer(FromDIP(30));
}
void MainFrame::show_plans() {
    if (page_ && page_title_ == plans_title())
        return;
    auto* page =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetScrollRate(0, FromDIP(12));
    fill_plans(page);
    show_page(page, plans_title());
}
void MainFrame::fill_plans(wxScrolledWindow* page) {
    // Plans are counted in parts, not days, so nobody falls behind.
    page->DestroyChildren();
    auto* content = new wxPanel(page);
    auto* entries = new wxBoxSizer(wxVERTICAL);
    content->SetSizer(entries);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(content, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, FromDIP(40));
    page->SetSizer(outer);
    const auto completed = user_.completed();
    for (const auto& plan : reading_plans()) {
        std::size_t done = 0;
        for (std::size_t i = 0; i < plan.parts.size(); ++i)
            done += completed.contains(plan_key(plan, i));
        auto* header = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
        auto* title = new wxStaticText(content, wxID_ANY, ui::utf8(plan.title));
        title->SetFont(body_font(20));
        header->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        header->Add(ui::label(content, std::to_string(done) + " av " + std::to_string(plan.parts.size()), 11),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        auto* reset = ui::button(content, ui::utf8("Börja om"), [this, plan] {
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
                if (auto* plans = dynamic_cast<wxScrolledWindow*>(page_);
                    plans && page_title_ == plans_title())
                    fill_plans(plans);
            });
        });
        reset->Enable(done > 0);
        reset->SetToolTip(ui::utf8("Ta bort alla markeringar i planen"));
        header->Add(reset, 0, wxALIGN_CENTER_VERTICAL);
        entries->Add(header, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
        if (!plan.description.empty())
            entries->Add(ui::label(content, ui::utf8(plan.description), 11), 0, wxBOTTOM, FromDIP(12));
        else
            entries->AddSpacer(FromDIP(12));
        if (done == plan.parts.size())
            entries->Add(ui::label(content, ui::utf8("🎉 ✨ Hela planen är läst! 🕊️ 🎊"), 16), 0, wxBOTTOM,
                         FromDIP(10));
        auto* grid = new wxGridSizer(10, FromDIP(6), FromDIP(6));
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const bool read = completed.contains(plan_key(plan, i));
            const wxString number = std::to_string(i + 1);
            const auto label = ui::utf8(plan_label(corpus_, plan.parts[i]));
            auto* tile = new TextButton(content, read ? ui::utf8("✓ ") + number : number);
            tile->SetMinSize(FromDIP(wxSize(44, 34)));
            tile->SetName(ui::utf8(plan.title) + ", Del " + number + ", " + label +
                          (read ? ui::utf8(", läst") : wxString{}));
            tile->Bind(wxEVT_BUTTON, [this, &plan, i](wxCommandEvent&) {
                open_plan_part(plan, i);
            });
            tile->SetToolTip("Del " + number + ui::utf8(", ") + label +
                             (read ? ui::utf8(", läst") : wxString{}));
            grid->Add(tile);
        }
        entries->Add(grid, 0, wxBOTTOM, FromDIP(36));
    }
    ui::recolor(page, palette(settings_.theme));
    page->FitInside();
    page->Layout();
}
} // namespace ortho
