#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/reading_tooltips.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <ranges>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <wx/wupdlock.h>
namespace ortho {
namespace {
class PlansPage final : public wxScrolledWindow {
public:
    using wxScrolledWindow::wxScrolledWindow;
    struct Part {
        TextButton* button;
        std::string key, passage_key;
        wxString name;
    };
    struct Section {
        wxStaticText* count;
        wxControl* reset;
        wxStaticText* celebration;
        std::vector<Part> parts;
    };
    std::vector<Section> sections;
    int zoom = 0;
};
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
    restore_reading_position();
    save_navigation();
}
void MainFrame::complete_reading(const Tracked& item) {
    if (user_.completed().contains(item.key))
        return;
    try {
        user_.complete(item.key);
        user_.reset_position(item.key);
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
        // Finish the tile's event before changing views; Back retains the plans page.
        CallAfter([this, reading = std::move(reading), item = std::move(item)] {
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
    entries_->AddSpacer(FromDIP(zoomed(28)));
    entries_->Add(ui::label(home_content_, ui::utf8("LÄSPLAN"), 10), 0, wxBOTTOM, FromDIP(zoomed(6)));
    const auto add_row = [this](ReadingRow* row) {
        home_rows_.push_back(row);
        entries_->Add(row, 0, wxEXPAND);
    };
    if (plan != plans.end()) {
        std::size_t part = 0;
        while (completed.contains(plan_key(*plan, part)))
            ++part;
        const auto progress =
            reading_progress(plan_reading(corpus_, plan->parts[part], "sv"), plan_key(*plan, part));
        auto* row = new ReadingRow(
            home_content_,
            ui::utf8(plan->title + ", del " + std::to_string(part + 1) + " av " +
                     std::to_string(plan->parts.size()) + progress),
            ui::utf8(plan_label(corpus_, plan->parts[part])),
            [this, &plan = *plan, part] {
                open_plan_part(plan, part);
            },
            false, ui::utf8(plan_label(corpus_, plan->parts[part], false)));
        row->SetToolTip(ui::plan_parts_tooltip(corpus_, plan->parts[part]));
        add_row(row);
    }
    auto* all = new ReadingRow(home_content_, {}, ui::utf8("Alla läsplaner"), [this] {
        CallAfter([this] {
            show_plans();
        });
    });
    all->SetToolTip(ui::utf8("Visa alla läsplaner, se vad du har läst och välj en del att läsa"));
    add_row(all);
    entries_->AddSpacer(FromDIP(zoomed(30)));
}
void MainFrame::show_plans() {
    if (navigation_.current().screen == Screen::Plans)
        return;
    auto* page = new PlansPage(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->Hide();
    page->SetScrollRate(0, FromDIP(zoomed(12)));
    page->Bind(wxEVT_SIZE, [this, page](wxSizeEvent& event) {
        layout_plans(page);
        event.Skip();
    });
    fill_plans(page);
    show_page(page, Screen::Plans, plans_title());
}
void MainFrame::fill_plans(wxScrolledWindow* page) {
    const wxWindowUpdateLocker redraw(page);
    const auto scroll = page->GetViewStart();
    auto* plans = static_cast<PlansPage*>(page);
    // Plans are counted in parts, not days, so nobody falls behind.
    if (plans->zoom != text_zoom) {
        plans->sections.clear();
        page->DestroyChildren();
        auto* content = new wxPanel(page);
        auto* entries = new wxBoxSizer(wxVERTICAL);
        content->SetSizer(entries);
        auto* outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(content, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, FromDIP(zoomed(40)));
        page->SetSizer(outer);
        for (const auto& plan : reading_plans()) {
            PlansPage::Section section{nullptr, nullptr, nullptr, {}};
            auto* header = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
            auto* title = new wxStaticText(content, wxID_ANY, ui::utf8(plan.title));
            title->SetFont(body_font(20));
            header->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(12)));
            section.count = ui::label(content, {}, 11);
            header->Add(section.count, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(12)));
            auto* reset = ui::button(content, ui::utf8("Börja om"), [this, plan] {
                wxMessageDialog question(
                    this, ui::utf8("Vill du ta bort alla markeringar i ") + ui::utf8(plan.title) + "?",
                    ui::utf8("Börja om"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
                question.SetYesNoLabels(ui::utf8("Börja om"), ui::utf8("Avbryt"));
                if (question.ShowModal() != wxID_YES)
                    return;
                try {
                    user_.forget("plan:" + plan.id + ":");
                    for (const auto& part : plan.parts)
                        user_.reset_position(reading_key(corpus_, plan_reading(corpus_, part, "sv")));
                } catch (const std::exception& e) {
                    wxMessageBox(ui::utf8(e.what()), ui::utf8("Markeringarna kunde inte tas bort"),
                                 wxOK | wxICON_ERROR, this);
                    return;
                }
                // Refresh after the button's event finishes.
                CallAfter([this] {
                    if (auto* plans = dynamic_cast<wxScrolledWindow*>(current_page());
                        plans && navigation_.current().screen == Screen::Plans)
                        fill_plans(plans);
                });
            });
            section.reset = reset;
            reset->SetToolTip(ui::utf8("Ta bort alla markeringar i planen"));
            header->Add(reset, 0, wxALIGN_CENTER_VERTICAL);
            entries->Add(header, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(4)));
            if (!plan.description.empty())
                entries->Add(ui::label(content, ui::utf8(plan.description), 11), 0, wxBOTTOM,
                             FromDIP(zoomed(12)));
            else
                entries->AddSpacer(FromDIP(zoomed(12)));
            section.celebration = ui::label(content, ui::utf8("🎉 ✨ Hela planen är läst! 🕊️ 🎊"), 16);
            entries->Add(section.celebration, 0, wxBOTTOM, FromDIP(zoomed(10)));
            auto* grid = new wxGridSizer(1, FromDIP(zoomed(6)), FromDIP(zoomed(6)));
            for (std::size_t i = 0; i < plan.parts.size(); ++i) {
                const wxString number = std::to_string(i + 1);
                const auto label = ui::utf8(plan_label(corpus_, plan.parts[i]));
                auto* tile = new TextButton(content, number);
                tile->SetMinSize({std::max(FromDIP(zoomed(44)), tile->GetBestSize().x), FromDIP(zoomed(34))});
                const auto name = ui::utf8(plan.title) + ", Del " + number + ", " + label;
                tile->SetName(name);
                section.parts.push_back({tile, plan_key(plan, i),
                                         reading_key(corpus_, plan_reading(corpus_, plan.parts[i], "sv")),
                                         name});
                tile->Bind(wxEVT_BUTTON, [this, &plan, i](wxCommandEvent&) {
                    open_plan_part(plan, i);
                });
                tile->SetToolTip(ui::plan_parts_tooltip(corpus_, plan.parts[i]));
                grid->Add(tile, 0, wxEXPAND);
            }
            entries->Add(grid, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(36)));
            plans->sections.push_back(std::move(section));
        }
        ui::recolor(page, palette(settings_.theme));
        plans->zoom = text_zoom;
    }
    const auto completed = user_.completed();
    for (const auto& section : plans->sections) {
        std::size_t done = 0;
        bool partial = false;
        for (std::size_t i = 0; i < section.parts.size(); ++i) {
            const auto& part = section.parts[i];
            const bool read = completed.contains(part.key);
            done += read;
            std::string progress;
            if (!read) {
                auto position = user_.position(part.key);
                if (!position)
                    position = user_.position(part.passage_key);
                if (position) {
                    progress = " · " + std::to_string(position->percent) + " %";
                    partial = true;
                }
            }
            const wxString number = std::to_string(i + 1);
            const auto label = read ? ui::utf8("✓ ") + number : number + ui::utf8(progress);
            if (part.button->GetLabel() != label) {
                part.button->SetLabel(label);
                part.button->SetMinSize(wxDefaultSize);
                part.button->InvalidateBestSize();
                part.button->SetMinSize(
                    {std::max(FromDIP(zoomed(44)), part.button->GetBestSize().x), FromDIP(zoomed(34))});
                part.button->Refresh(false);
            }
            const auto name = part.name + (read ? ui::utf8(", läst") : ui::utf8(progress));
            if (part.button->GetName() != name)
                part.button->SetName(name);
        }
        section.count->SetLabel(std::to_string(done) + " av " + std::to_string(section.parts.size()));
        section.reset->Enable(done > 0 || partial);
        section.celebration->Show(done == section.parts.size());
    }
    layout_plans(page);
    page->Scroll(scroll);
}
void MainFrame::layout_plans(wxScrolledWindow* page) {
    if (page->GetChildren().empty())
        return;
    auto* content = page->GetChildren().front();
    // Include saved-progress labels when measuring the space each column needs.
    int cell_width = FromDIP(zoomed(44));
    std::vector<wxGridSizer*> grids;
    for (auto* item : content->GetSizer()->GetChildren())
        if (auto* grid = dynamic_cast<wxGridSizer*>(item->GetSizer())) {
            grids.push_back(grid);
            for (auto* cell : grid->GetChildren())
                cell_width = std::max(cell_width, cell->GetMinSize().x);
        }
    const int gap = FromDIP(zoomed(6));
    const int available = std::max(1, page->GetClientSize().x - FromDIP(zoomed(64)));
    const int columns = std::clamp((available + gap) / (cell_width + gap), 1, 10);
    for (auto* grid : grids)
        grid->SetCols(columns);
    const int width = std::min(available, std::max(FromDIP(zoomed(400)), 10 * (cell_width + gap) - gap));
    content->SetMinSize({width, -1});
    content->SetSize({width, content->GetSize().y});
    content->InvalidateBestSize();
    page->FitInside();
    page->Layout();
}
} // namespace ortho
