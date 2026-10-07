#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <limits>
namespace ortho {
std::optional<Passage> ScriptureView::selection() const {
    if (!selection_)
        return std::nullopt;
    const auto last = corpus_.verse(frame_, displayed_.book, selection_->second);
    return Passage{displayed_.book, selection_->first,
                   last ? last->last.value_or(selection_->second) : selection_->second};
}
void ScriptureView::select_verses(VerseRef first, VerseRef last) {
    if (last < first)
        std::swap(first, last);
    if (selection_ == std::pair{first, last})
        return;
    selection_ = std::pair{first, last};
    Refresh(false);
    if (selection_changed_)
        selection_changed_();
}
void ScriptureView::clear_selection() {
    if (!selection_)
        return;
    selection_.reset();
    Refresh(false);
    if (selection_changed_)
        selection_changed_();
}
std::optional<VerseRef> ScriptureView::verse_at(wxPoint point) const {
    const auto found = hit(point);
    if (!found || found->run < 0)
        return std::nullopt;
    const auto& column = *found->column;
    return column.verses[column.text.lines[found->line].runs[found->run].tag].first;
}
namespace {
// Letters of a run, without punctuation, quotes, digits or a hyphenation mark.
wxString word_letters(const wxString& text) {
    wxString result;
    for (const auto c : text) {
        const auto code = c.GetValue();
        const bool ascii_letter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
        const bool other = code >= 0xc0 && code != 0xd7 && code != 0xf7 && code != 0x2013 && code != 0x2014 &&
                           code != 0x2018 && code != 0x2019 && code != 0x201c && code != 0x201d &&
                           code != 0x2026 && code != 0x0387 && code != 0x037e && code != 0x00ab &&
                           code != 0x00bb;
        if (ascii_letter || other)
            result += c;
    }
    return result;
}
} // namespace
std::vector<ScriptureView::WordRuns> ScriptureView::words(const Column& column) {
    std::vector<WordRuns> result;
    bool joining = false;
    for (std::size_t line = 0; line < column.text.lines.size(); ++line) {
        const auto& runs = column.text.lines[line].runs;
        for (std::size_t run = 0; run < runs.size(); ++run) {
            if (runs[run].marker || runs[run].tag < 0)
                continue;
            const auto letters = word_letters(runs[run].text);
            if (joining && !result.empty()) {
                result.back().text += letters;
                result.back().runs.emplace_back(line, run);
                joining = false;
                continue;
            }
            joining = false;
            if (letters.empty())
                continue;
            result.push_back({runs[run].tag, letters, {{line, run}}});
        }
        joining = column.text.lines[line].hyphenated && !result.empty();
    }
    return result;
}
std::optional<std::pair<ScriptureView::WordRuns, int>>
ScriptureView::word_of(const Column& column, int tag, std::size_t line, int run) const {
    std::map<wxString, int> seen;
    for (auto& word : words(column)) {
        if (word.tag != tag)
            continue;
        const int occurrence = seen[word.text.Lower()]++;
        for (const auto& [l, r] : word.runs)
            if (l == line && int(r) == run)
                return std::pair{std::move(word), occurrence};
    }
    return std::nullopt;
}
std::optional<ScriptureView::Word> ScriptureView::word_at(wxPoint point) const {
    const auto found = hit(point);
    if (!found || found->run < 0 || found->distance > FromDIP(4))
        return std::nullopt;
    const auto& column = *found->column;
    const int tag = column.text.lines[found->line].runs[found->run].tag;
    const auto word = word_of(column, tag, found->line, found->run);
    if (!word)
        return std::nullopt;
    const auto utf8 = word->first.text.ToUTF8();
    return Word{displayed_.book, std::string(utf8.data(), utf8.length()), column.verses[tag].first,
                word->second};
}
void ScriptureView::highlight_word(std::optional<Word> word) {
    highlighted_ = std::move(word);
    Refresh(false);
}
std::optional<ScriptureView::Hit> ScriptureView::hit(wxPoint point) const {
    if (rows_.empty() || positions_.empty())
        return std::nullopt;
    // Rows are drawn from positions_ shifted by the scroll offset and a top inset.
    const double y = point.y + offset_ - FromDIP(12);
    auto it = std::upper_bound(positions_.begin(), positions_.end(), y);
    if (it == positions_.begin())
        return std::nullopt;
    const auto index = std::size_t(it - positions_.begin() - 1);
    if (index >= rows_.size() || rows_[index].heading)
        return std::nullopt;
    const auto& layout = row_layout(index);
    if (layout.columns.empty())
        return std::nullopt;
    const int margin = outside_margin(), stride = column_stride();
    const bool stacked = stacked_columns();
    if (!stacked && columns_count() > 1 && point.x >= margin + stride - FromDIP(24))
        return std::nullopt;
    const auto& column = layout.columns.front();
    double top = positions_[index];
    if (stacked && layout.columns.size() > 1)
        top += FromDIP(24);
    if (y < top || y >= top + column.text.height())
        return std::nullopt;
    const auto line =
        std::min(column.text.lines.size() - 1, std::size_t((y - top) / column.text.line_height));
    const double x = point.x - margin - FromDIP(30);
    // The run under the pointer, or the nearest one on that line.
    int nearest = -1;
    double best = std::numeric_limits<double>::max();
    const auto& runs = column.text.lines[line].runs;
    for (std::size_t i = 0; i < runs.size(); ++i) {
        const auto& run = runs[i];
        if (!column.main_text(run.tag))
            continue;
        const double distance = x < run.x ? run.x - x : x > run.x + run.width ? x - run.x - run.width : 0;
        if (distance < best) {
            best = distance;
            nearest = int(i);
        }
    }
    return Hit{index, &column, line, nearest, best};
}
} // namespace ortho
