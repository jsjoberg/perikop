#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <wx/dcclient.h>
#include <wx/menu.h>

namespace ortho {
std::string ScriptureView::language_of(const std::string& source) const {
    const auto found = std::ranges::find(source_catalog_, source, &Source::code);
    return found == source_catalog_.end() ? std::string{} : found->language;
}
std::string ScriptureView::primary_language() const {
    return language_of(base_source());
}
wxString ScriptureView::source_label(const std::string& source, std::size_t heading) const {
    if (source.empty())
        return ui::utf8("Välj källa");
    if (source == "sv1917")
        return swedish_edition(heading);
    const auto found = std::ranges::find(source_catalog_, source, &Source::code);
    return ui::utf8(found == source_catalog_.end() ? source : found->name);
}
wxRect ScriptureView::source_selector_rect(std::size_t heading, std::size_t pane) const {
    const bool stacked = stacked_columns();
    const int available = stacked ? GetClientSize().x - 2 * outside_margin() : column_stride();
    const auto sources = shown_sources(heading);
    const auto label = source_label(pane < sources.size() ? sources[pane] : "", heading);
    wxClientDC dc(const_cast<ScriptureView*>(this));
    dc.SetFont(ui_font(9));
    const int width = std::clamp(dc.GetTextExtent(label).x + FromDIP(zoomed(42)), FromDIP(zoomed(80)),
                                 std::max(FromDIP(zoomed(80)), available - FromDIP(zoomed(16))));
    const int center = columns_count() == 1 || stacked ? GetClientSize().x / 2
                                                       : outside_margin() + int(pane) * column_stride() +
                                                             column_width() / 2 + FromDIP(zoomed(15));
    return {center - width / 2,
            int(std::lround(positions_[heading] - offset_)) +
                FromDIP(zoomed(92 + (stacked ? 30 * int(pane) : 0))),
            width, FromDIP(zoomed(26))};
}
void ScriptureView::position_source_selectors() {
    std::set<std::pair<std::size_t, std::size_t>> visible;
    const auto colors = palette(settings_.theme);
    for (auto row = first_visible(); row < rows_.size() && positions_[row] - offset_ < GetClientSize().y;
         ++row) {
        if (!rows_[row].heading)
            continue;
        const auto sources = shown_sources(row);
        for (int pane = 0; pane < columns_count(); ++pane) {
            const auto key = std::pair{row, std::size_t(pane)};
            const auto rectangle = source_selector_rect(row, std::size_t(pane));
            if (!GetClientRect().Contains(rectangle))
                continue;
            visible.insert(key);
            auto*& button = source_selectors_[key];
            if (!button) {
                button = ui::button(this, {}, [this, row, pane] {
                    const auto found = source_selectors_.find({row, std::size_t(pane)});
                    if (found != source_selectors_.end())
                        source_menu(row, std::size_t(pane), found->second);
                });
                button->SetFont(ui_font(9));
            }
            const auto full = source_label(std::size_t(pane) < sources.size() ? sources[pane] : "", row);
            wxClientDC dc(this);
            dc.SetFont(ui_font(9));
            const auto label =
                wxControl::Ellipsize(full, dc, wxELLIPSIZE_END, rectangle.width - FromDIP(zoomed(34))) +
                ui::utf8(" ▾");
            button->SetFont(ui_font(9));
            if (button->GetLabel() != label) {
                button->SetLabel(label);
                button->Refresh();
            }
            button->SetName(
                ui::utf8(pane ? "Andra texten, " : "Huvudtext, ") + full +
                wxString::Format(", kapitel %d", rows_[row].ref.chapter - (canon_ ? canon_->offset() : 0)));
            button->SetToolTip(
                ui::utf8("Byt källa bara för detta kapitel. Prioriteringen i Inställningar behålls."));
            button->SetBackgroundColour(colors.paper);
            button->SetForegroundColour(colors.muted);
            button->SetSize(rectangle);
            button->Show();
        }
    }
    for (auto it = source_selectors_.begin(); it != source_selectors_.end();) {
        if (!visible.contains(it->first)) {
            it->second->Destroy();
            it = source_selectors_.erase(it);
        } else
            ++it;
    }
}
void ScriptureView::source_menu(std::size_t heading, std::size_t pane, wxWindow* button) {
    const auto available = sources_for_chapter(rows_[heading].ref.chapter);
    const auto shown = shown_sources(heading);
    const auto found = chapter_overrides_.find({displayed_.book, rows_[heading].ref.chapter});
    const bool overridden = found != chapter_overrides_.end() && !found->second[pane].empty();
    const int first = wxID_HIGHEST + 2000;
    wxMenu menu;
    menu.AppendRadioItem(first, ui::utf8("Automatiskt enligt Inställningar"));
    menu.Check(first, !overridden);
    for (std::size_t i = 0; i < available.size(); ++i) {
        const int id = first + 1 + int(i);
        menu.AppendRadioItem(id, source_label(available[i], heading));
        if (overridden && pane < shown.size() && shown[pane] == available[i])
            menu.Check(id, true);
    }
    const int chosen = button->GetPopupMenuSelectionFromUser(menu, 0, button->GetSize().y);
    if (chosen == wxID_NONE)
        return;
    const auto source = chosen == first ? std::string{} : available[std::size_t(chosen - first - 1)];
    const int chapter = rows_[heading].ref.chapter;
    CallAfter([this, chapter, pane, source] {
        chapter_source(chapter, pane, source);
    });
}
std::string ScriptureView::chapter_source(int chapter, std::size_t pane) const {
    const auto row = std::ranges::find_if(rows_, [chapter](const Row& row) {
        return row.heading && row.ref.chapter == chapter;
    });
    if (row == rows_.end())
        return {};
    const auto sources = shown_sources(std::size_t(row - rows_.begin()));
    return pane < sources.size() ? sources[pane] : std::string{};
}
void ScriptureView::chapter_source(int chapter, std::size_t pane, const std::string& source) {
    if (pane > 1)
        return;
    const auto available = sources_for_chapter(chapter);
    if (!source.empty() && std::ranges::find(available, source) == available.end())
        return;
    chapter_overrides_[{displayed_.book, chapter}][pane] = source;
    clear_selection();
    highlighted_.reset();
    highlighted_verse_.reset();
    swedish_editions_.clear();
    invalidate();
    if (source_changed_)
        source_changed_(pane == 0);
}
} // namespace ortho
