#include "core/online_bibles.hpp"
#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/utils.h>
namespace ortho {
void ScriptureView::rebuild_verse_links() {
    if (!highlighted_verse_ || settings_.word_study) {
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
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    const auto links = online_bible_links(corpus_, frame_, displayed_.book, highlighted_verse_->first);
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
        button->SetName(ui::utf8(link.label + " · " + link.reference));
        button->SetToolTip(
            ui::utf8(link.url.empty() ? "Ingen verifierad verslänk för detta bibelställe."
                                      : "Öppna " + link.reference + " i " + link.label + " i webbläsaren."));
        sizer->Add(button, 0, wxALL, FromDIP(3));
    }
    verse_links_->SetSizerAndFit(sizer);
    // Rebuilding at the same size does not send a size event to lay out new children.
    verse_links_->Layout();
    ui::recolor(verse_links_, palette(settings_.theme));
    position_verse_links();
}
void ScriptureView::position_verse_links() {
    if (!verse_links_)
        return;
    if (!highlighted_verse_ || settings_.word_study || rows_.empty()) {
        verse_links_->Hide();
        return;
    }
    const auto range = *highlighted_verse_;
    const auto row = std::find_if(rows_.begin(), rows_.end(), [&](const auto& row) {
        return !row.heading && row.ref <= range.second && range.first <= row.last;
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
    const auto c = std::min(highlighted_column_, layout.columns.size() - 1);
    const auto& column = layout.columns[c];
    double top = positions_[index] - offset_ + FromDIP(12);
    const bool stacked = stacked_columns();
    if (stacked && layout.columns.size() > 1) {
        for (std::size_t before = 0; before < c; ++before)
            top += layout.columns[before].text.height() + FromDIP(24);
        top += FromDIP(24);
    }
    for (std::size_t line = 0; line < column.text.lines.size(); ++line)
        for (const auto& run : column.text.lines[line].runs) {
            if (!column.main_text(run.tag) || column.verses[run.tag].first > range.second ||
                column.verses[run.tag].second < range.first)
                continue;
            const double verse_top = top + line * column.text.line_height;
            if (verse_top + column.text.line_height <= 0)
                continue;
            if (verse_top >= GetClientSize().y) {
                verse_links_->Hide();
                return;
            }
            const auto size = verse_links_->GetSize();
            const int x =
                outside_margin() + (stacked ? 0 : int(c) * column_stride()) + FromDIP(30) + int(run.x);
            const int y = std::max(FromDIP(6), int(verse_top) - size.y - FromDIP(6));
            const wxPoint position(
                std::clamp(x, FromDIP(6), std::max(FromDIP(6), GetClientSize().x - size.x - FromDIP(6))), y);
            if (verse_links_->GetPosition() != position)
                verse_links_->Move(position);
            verse_links_->Show();
            verse_links_->Raise();
            return;
        }
    verse_links_->Hide();
}
} // namespace ortho
