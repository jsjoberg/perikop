#include "core/online_bibles.hpp"
#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/utils.h>
namespace ortho {
void ScriptureView::rebuild_verse_links() {
    const auto marked = marked_reading();
    if (!marked || settings_.word_study || (selections_.empty() && primary_language() != "sv")) {
        if (verse_links_) {
            if (verse_links_->IsDescendant(wxWindow::FindFocus()))
                SetFocus();
            verse_links_->Hide();
        }
        return;
    }
    if (!verse_links_) {
        verse_links_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        verse_links_->SetName(ui::utf8("Läs den markerade versen online"));
    }
    verse_links_->Hide();
    verse_links_->DestroyChildren();
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* bookmark = new SymbolButton(verse_links_, Symbol::Bookmark, ui::utf8("Lägg till bokmärke"),
                                      [this, reading = *marked] {
                                          CallAfter([this, reading] {
                                              if (bookmarked_)
                                                  bookmarked_(reading);
                                          });
                                      });
    bookmark->SetToolTip(ui::utf8("Spara den markerade texten som bokmärke"));
    bookmark->apply(palette(settings_.theme));
    actions->Add(bookmark, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(zoomed(3)));
    if (primary_language() == "sv") {
        auto* listen = ui::button(verse_links_, "Lyssna", [this, reading = *marked] {
            CallAfter([this, reading] {
                if (listen_)
                    listen_(reading);
            });
        });
        listen->SetFont(ui_font(10));
        listen->SetToolTip(ui::utf8("Läs upp den markerade texten"));
        actions->Add(listen, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(zoomed(3)));
    }
    sizer->Add(actions, 0);
    auto* resources = new wxBoxSizer(wxHORIZONTAL);
    const auto links = primary_language() == "sv"
                           ? online_bible_links(corpus_, frame_, displayed_.book, marked->passage.first)
                           : std::vector<OnlineBibleLink>{};
    for (const auto& link : links) {
        auto* button = ui::button(verse_links_, ui::utf8(link.label), [link] {
            if (link.url.empty())
                return;
            const auto form = online_bible_form(link);
            if (form.empty()) {
                wxLaunchDefaultBrowser(ui::utf8(link.url));
                return;
            }
            // Keep the POST handoff page until the browser has read it.
            wxFile file;
            const auto temporary = wxFileName::CreateTempFileName("perikop-bible-", &file);
            if (temporary.empty()) {
                wxLogError(ui::utf8("Kunde inte skapa verslänken."));
                return;
            }
            const bool written = file.Write(ui::utf8(form));
            const bool closed = file.Close();
            const auto page = temporary + ".html";
            if (!written || !closed || !wxRenameFile(temporary, page, false)) {
                wxRemoveFile(temporary);
                wxLogError(ui::utf8("Kunde inte skapa verslänken."));
                return;
            }
            wxLaunchDefaultBrowser(wxFileSystem::FileNameToURL(wxFileName(page)));
        });
        button->SetFont(ui_font(10));
        button->Enable(!link.url.empty());
        button->SetName(ui::utf8(link.label + ", " + link.reference));
        button->SetToolTip(
            ui::utf8(link.url.empty() ? "Ingen verifierad verslänk för detta bibelställe."
                                      : "Öppna " + link.reference + " i " + link.label + " i webbläsaren."));
        resources->Add(button, 0, wxALL, FromDIP(zoomed(3)));
    }
    if (links.empty())
        delete resources;
    else
        sizer->Add(resources, 0);
    verse_links_->SetSizerAndFit(sizer);
    // Rebuilding at the same size does not send a size event to lay out new children.
    verse_links_->Layout();
    ui::recolor(verse_links_, palette(settings_.theme));
    position_verse_links();
}
void ScriptureView::position_verse_links() {
    if (!verse_links_)
        return;
    const auto marked = marked_reading();
    if (!marked || settings_.word_study || rows_.empty() ||
        (selections_.empty() && primary_language() != "sv")) {
        verse_links_->Hide();
        return;
    }
    const auto passages = marked->segments();
    const auto row =
        std::find_if(rows_.begin() + std::ptrdiff_t(first_visible()), rows_.end(), [&](const auto& row) {
            return !row.heading && std::ranges::any_of(passages, [&](const Passage& passage) {
                return row.ref <= passage.last && passage.first <= row.last;
            });
        });
    if (row == rows_.end()) {
        verse_links_->Hide();
        return;
    }
    const auto index = std::size_t(row - rows_.begin());
    if (positions_[index + 1] <= offset_ || positions_[index] >= offset_ + GetClientSize().y) {
        verse_links_->Hide();
        return;
    }
    const auto& layout = row_layout(index);
    if (layout.columns.empty()) {
        verse_links_->Hide();
        return;
    }
    const auto c = selections_.empty() ? std::min(highlighted_column_, layout.columns.size() - 1) : 0;
    const auto& column = layout.columns[c];
    double top = positions_[index] - offset_ + FromDIP(zoomed(12));
    const bool stacked = stacked_columns();
    if (stacked && layout.columns.size() > 1) {
        for (std::size_t before = 0; before < c; ++before)
            top += layout.columns[before].text.height() + FromDIP(zoomed(24));
        top += FromDIP(zoomed(24));
    }
    for (std::size_t line = 0; line < column.text.lines.size(); ++line)
        for (const auto& run : column.text.lines[line].runs) {
            if (!column.main_text(run.tag) || !std::ranges::any_of(passages, [&](const Passage& passage) {
                    return column.verses[run.tag].first <= passage.last &&
                           passage.first <= column.verses[run.tag].second;
                }))
                continue;
            const double verse_top = top + line * column.text.line_height;
            if (verse_top + column.text.line_height <= 0)
                continue;
            if (verse_top >= GetClientSize().y) {
                verse_links_->Hide();
                return;
            }
            const auto size = verse_links_->GetSize();
            const int x = outside_margin() + (stacked ? 0 : int(c) * column_stride()) + FromDIP(zoomed(30)) +
                          int(run.x);
            const int y = std::max(FromDIP(zoomed(6)), int(verse_top) - size.y - FromDIP(zoomed(6)));
            const wxPoint position(
                std::clamp(x, FromDIP(zoomed(6)),
                           std::max(FromDIP(zoomed(6)), GetClientSize().x - size.x - FromDIP(zoomed(6)))),
                y);
            if (verse_links_->GetPosition() != position)
                verse_links_->Move(position);
            verse_links_->Show();
            verse_links_->Raise();
            return;
        }
    verse_links_->Hide();
}
} // namespace ortho
