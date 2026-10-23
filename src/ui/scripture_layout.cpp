#include "core/source_preferences.hpp"
#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <set>
#include <wx/dcclient.h>
namespace ortho {
int ScriptureView::columns_count() const {
    return settings_.split_view && !settings_.word_study ? 2 : 1;
}
double ScriptureView::alphabet_width() const {
    wxClientDC dc(const_cast<ScriptureView*>(this));
    dc.SetFont(body_font(reading_points));
    return dc.GetTextExtent("abcdefghijklmnopqrstuvwxyz").x;
}
int ScriptureView::outside_margin() const {
    const int width = GetClientSize().x;
    if (columns_count() == 1) {
        // Two and a half alphabets, about 65 characters, is Bringhurst's comfortable line;
        // the extra 30 DIP is the column's gutter for verse bars.
        const int limit = int(2.5 * alphabet_width()) + FromDIP(zoomed(30));
        return std::max(FromDIP(zoomed(32)), (width - std::min(limit, width - FromDIP(zoomed(64)))) / 2);
    }
    return std::clamp(width / 14, FromDIP(zoomed(32)), FromDIP(zoomed(105)));
}
bool ScriptureView::stacked_columns() const {
    return (GetClientSize().x - 2 * outside_margin()) / columns_count() < FromDIP(zoomed(270));
}
int ScriptureView::column_stride() const {
    const int usable = GetClientSize().x - 2 * outside_margin();
    const int gap = FromDIP(zoomed(44));
    return (usable - gap * (columns_count() - 1)) / columns_count() + gap;
}
int ScriptureView::estimated_height(const Row& row) const {
    return row.heading ? FromDIP(zoomed(stacked_columns() && columns_count() == 2 ? 158 : 130))
                       : FromDIP(zoomed(150));
}
int ScriptureView::column_width() const {
    const int usable = std::max(FromDIP(zoomed(120)), GetClientSize().x - 2 * outside_margin());
    return stacked_columns() ? usable - FromDIP(zoomed(30))
                             : (usable - FromDIP(zoomed(44)) * (columns_count() - 1)) / columns_count() -
                                   FromDIP(zoomed(30));
}
void ScriptureView::open(const Reading& reading) {
    reading_ = reading;
    chapter_overrides_.clear();
    open_section(0);
}
void ScriptureView::open_section(std::size_t index) {
    const auto segments = reading_.segments();
    if (index >= segments.size())
        return;
    displayed_ = segments[index];
    end_seen_ = false;
    rows_.clear();
    located_cue_.reset();
    highlighted_.reset();
    highlighted_verse_.reset();
    selection_anchor_.reset();
    drag_anchor_.reset();
    drag_ranges_.clear();
    if (HasCapture())
        ReleaseMouse();
    if (verse_links_)
        verse_links_->Hide();
    speech_row_.reset();
    guide_.reset();
    guide_alpha_ = 0;
    orphans_.clear();
    swedish_editions_.clear();
    available_sources_.clear();
    for (auto& [key, selector] : source_selectors_) {
        (void)key;
        selector->Destroy();
    }
    source_selectors_.clear();
    if (!selections_.empty()) {
        selections_.clear();
        if (selection_changed_)
            selection_changed_();
    }
    // OSB defines the shared book order and printed coordinates for every edition.
    frame_ = osb_reference;
    if (corpus_.coordinates(frame_, displayed_.book).empty())
        for (const std::string language : {"sv", "el", "en"}) {
            const auto source = corpus_.source_for_language(language, displayed_.book);
            if (!corpus_.coordinates(source, displayed_.book).empty()) {
                frame_ = source;
                break;
            }
        }
    base_source_ =
        reading_.source_override.empty()
            ? preferred_source(settings_.primary_sources, sources_for_chapter(displayed_.first.chapter))
            : reading_.source_override;
    // Paragraphs follow the left pane's edition, placed at its framing verses.
    std::set<VerseRef> boundaries;
    if (base_source_ == frame_) {
        for (auto ref : corpus_.paragraph_starts(frame_, displayed_.book))
            boundaries.insert(ref);
    } else {
        std::set<std::string> source_books;
        for (const auto& ref : corpus_.coordinates(frame_, displayed_.book))
            for (const auto& [book, target] :
                 corpus_.counterparts(frame_, base_source_, displayed_.book, ref)) {
                (void)target;
                source_books.insert(book);
            }
        for (const auto& source_book : source_books)
            for (auto ref : corpus_.paragraph_starts(base_source_, source_book))
                for (const auto& [book, target] :
                     corpus_.counterparts(base_source_, frame_, source_book, ref))
                    if (book == displayed_.book) {
                        boundaries.insert(target);
                        break;
                    }
    }
    // A book without source paragraph metadata starts each OSB chapter separately.
    if (boundaries.empty())
        for (auto ref : corpus_.paragraph_starts(frame_, displayed_.book))
            boundaries.insert(ref);
    int chapter = 0;
    for (auto ref : corpus_.coordinates(frame_, displayed_.book)) {
        const bool new_chapter = ref.chapter != chapter;
        if (new_chapter) {
            chapter = ref.chapter;
            rows_.push_back({ref, true, {}, ref});
        }
        if (new_chapter || corpus_.stanza_book(displayed_.book, ref.chapter) || boundaries.contains(ref))
            rows_.push_back({ref, false, {}, ref});
        rows_.back().verses.push_back(ref);
        const auto verse = corpus_.verse(frame_, displayed_.book, ref);
        rows_.back().last = verse ? verse->last.value_or(ref) : ref;
    }
    cache_.clear();
    order_.clear();
    ++layout_epoch_;
    clear_tiles();
    offset_ = target_ = 0;
    wheel_timer_.Stop();
    heights_.clear();
    for (const auto& row : rows_)
        heights_.push_back(estimated_height(row));
    rebuild_positions();
    center_passage();
    schedule_prefetch();
    request_repaint();
}
std::string ScriptureView::base_source() const {
    if (rows_.empty())
        return base_source_;
    const auto sources = shown_sources(first_visible());
    return sources.empty() ? std::string{} : sources.front();
}
std::vector<std::string> ScriptureView::sources_for_chapter(int chapter) const {
    if (auto found = available_sources_.find(chapter); found != available_sources_.end())
        return found->second;
    return available_sources_[chapter] = chapter_sources(corpus_, frame_, displayed_.book, chapter);
}
std::vector<std::string> ScriptureView::shown_sources(std::size_t row) const {
    const int chapter = rows_[row].ref.chapter;
    const auto available = sources_for_chapter(chapter);
    const auto found = chapter_overrides_.find({displayed_.book, chapter});
    const std::array<std::string, 2> overrides =
        found == chapter_overrides_.end() ? std::array<std::string, 2>{} : found->second;
    auto primary = overrides[0];
    if (primary.empty())
        primary = reading_.source_override.empty() ? preferred_source(settings_.primary_sources, available)
                                                   : reading_.source_override;
    std::vector<std::string> result{primary};
    if (columns_count() == 2) {
        auto secondary = overrides[1];
        if (secondary.empty())
            secondary = preferred_source(settings_.secondary_sources, available, primary);
        if (!secondary.empty())
            result.push_back(secondary);
    }
    return result;
}
wxString ScriptureView::swedish_edition(std::size_t heading) const {
    if (auto it = swedish_editions_.find(heading); it != swedish_editions_.end())
        return it->second;
    // The Swedish deuterocanonical books are the 1921 apocrypha.
    bool bible = false, apocrypha = false;
    for (auto index = heading + 1; index < rows_.size() && !rows_[index].heading; ++index)
        for (const auto& ref : rows_[index].verses)
            for (const auto& [book, target] : corpus_.counterparts(frame_, "sv1917", displayed_.book, ref)) {
                (void)target;
                (corpus_.deuterocanonical_book(book) ? apocrypha : bible) = true;
            }
    return swedish_editions_[heading] = bible && apocrypha ? ui::utf8("SVENSKA 1917 OCH 1921")
                                        : apocrypha        ? ui::utf8("SVENSKA 1921")
                                        : bible            ? ui::utf8("SVENSKA 1917")
                                                           : wxString{};
}
const ScriptureView::Orphans& ScriptureView::orphans(const std::string& source) const {
    if (auto it = orphans_.find(source); it != orphans_.end())
        return it->second;
    auto& result = orphans_[source];
    if (source == frame_ || frame_ != osb_reference)
        return result;
    // The edition's books that this framing book draws on, such as Swedish Nehemiah.
    std::map<std::string, std::pair<VerseRef, VerseRef>> books;
    for (const auto& row : rows_)
        if (!row.heading)
            for (const auto& ref : row.verses)
                for (const auto& [book, target] :
                     corpus_.counterparts(frame_, source, displayed_.book, ref)) {
                    auto [it, added] = books.try_emplace(book, std::pair{target, target});
                    if (!added) {
                        it->second.first = std::min(it->second.first, target);
                        it->second.second = std::max(it->second.second, target);
                    }
                }
    for (const auto& [book, bounds] : books) {
        std::optional<VerseRef> anchor;
        std::vector<VerseRef> pending;
        for (const auto& ref : corpus_.coordinates(source, book)) {
            // Combined source books, such as Greek Ezra/Nehemiah, must not leak another
            // OSB book's unmapped material into this book's margins.
            if (ref < bounds.first || ref > bounds.second)
                continue;
            const auto targets = corpus_.counterparts(source, frame_, book, ref);
            if (targets.empty()) {
                if (anchor)
                    result[*anchor].emplace_back(book, ref);
                else
                    pending.push_back(ref);
                continue;
            }
            const auto here = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
                return target.first == displayed_.book;
            });
            if (here == targets.end())
                continue;
            anchor = here->second;
            for (const auto& waiting : pending)
                result[*anchor].emplace_back(book, waiting);
            pending.clear();
        }
    }
    return result;
}
void ScriptureView::center_passage() {
    prepare_visible();
    auto it = std::find_if(rows_.begin(), rows_.end(), [this](auto& r) {
        return !r.heading && within_refs(displayed_.first, r.ref, r.last);
    });
    if (it == rows_.end())
        return;
    wheel_timer_.Stop();
    const auto index = std::size_t(it - rows_.begin());
    const auto begin = index > 2 ? index - 2 : 0;
    for (auto i = begin; i <= index; ++i)
        heights_[i] = row_layout(i).height;
    rebuild_positions();
    const bool fits = positions_[index + 1] - positions_[begin] < GetClientSize().y;
    const auto start = fits ? begin : (index && rows_[index - 1].heading ? index - 1 : index);
    double position = positions_[start];
    const auto& paragraph = row_layout(index);
    if (!paragraph.columns.empty()) {
        // Run tags index the column's fragments, which merged and source-only verses offset from
        // the row's verses; find the passage's first verse among them, as show_verse() does.
        const auto& column = paragraph.columns.front();
        const auto& text = column.text;
        for (std::size_t line = 0; line < text.lines.size(); ++line)
            if (std::any_of(text.lines[line].runs.begin(), text.lines[line].runs.end(), [&](const auto& run) {
                    return column.main_text(run.tag) &&
                           within_refs(displayed_.first, column.verses[run.tag].first,
                                       column.verses[run.tag].second);
                })) {
                if (line > 2)
                    position = positions_[index] + (line - 2) * text.line_height;
                break;
            }
    }
    set_position(position);
    target_ = offset_;
}
VerseRef ScriptureView::current_verse() const {
    // The same reading line that spoken text follows. Look at text runs, not page pixels.
    const double line_y = offset_ + GetClientSize().y * 0.38;
    // Clamp in printed order: OSB Esther 1:1a–1:1r precede 1:1.
    const auto clamp = [this](const VerseRef& verse) {
        if (compare_refs(verse, displayed_.first) < 0)
            return displayed_.first;
        if (compare_refs(displayed_.last, verse) < 0)
            return displayed_.last;
        return verse;
    };
    VerseRef result = displayed_.first;
    for (std::size_t row = first_visible(); row < rows_.size(); ++row) {
        if (rows_[row].heading)
            continue;
        const auto& columns = row_layout(row).columns;
        if (columns.empty())
            continue;
        const auto& column = columns.front();
        const double top = positions_[row] + FromDIP(zoomed(12)) +
                           (stacked_columns() && columns.size() > 1 ? FromDIP(zoomed(24)) : 0);
        for (std::size_t line = 0; line < column.text.lines.size(); ++line) {
            for (const auto& run : column.text.lines[line].runs)
                if (column.main_text(run.tag)) {
                    const auto verse = column.verses[run.tag].first;
                    if (top + (line + 1) * column.text.line_height >= line_y)
                        return clamp(verse);
                    result = verse;
                }
        }
    }
    return clamp(result);
}
void ScriptureView::show_verse(VerseRef verse) {
    prepare_visible();
    for (std::size_t row = 0; row < rows_.size(); ++row) {
        if (rows_[row].heading || !within_refs(verse, rows_[row].ref, rows_[row].last))
            continue;
        for (std::size_t i = row > 2 ? row - 2 : 0; i <= row; ++i)
            heights_[i] = row_layout(i).height;
        rebuild_positions();
        const auto& columns = row_layout(row).columns;
        if (columns.empty())
            return;
        const auto& column = columns.front();
        for (std::size_t line = 0; line < column.text.lines.size(); ++line)
            for (const auto& run : column.text.lines[line].runs)
                if (column.main_text(run.tag) &&
                    within_refs(verse, column.verses[run.tag].first, column.verses[run.tag].second)) {
                    restore_scroll(positions_[row] + FromDIP(zoomed(12)) +
                                   (stacked_columns() && columns.size() > 1 ? FromDIP(zoomed(24)) : 0) +
                                   line * column.text.line_height - GetClientSize().y * 0.38);
                    return;
                }
        return;
    }
}
bool ScriptureView::passage_in_view() const {
    // Rows at the very edges, or under the button, do not count as seen.
    const double top = offset_ + FromDIP(zoomed(40)),
                 bottom = offset_ + GetClientSize().y - FromDIP(zoomed(90));
    for (auto i = first_visible(); i < rows_.size() && positions_[i] < bottom; ++i)
        if (positions_[i + 1] > top && !rows_[i].heading &&
            overlap_refs(rows_[i].ref, rows_[i].last, displayed_.first, displayed_.last))
            return true;
    return false;
}
void ScriptureView::apply(const Settings& settings) {
    source_catalog_ = corpus_.sources();
    if (settings_.primary_sources != settings.primary_sources ||
        settings_.secondary_sources != settings.secondary_sources)
        chapter_overrides_.clear();
    const bool metrics_changed = settings_.text_zoom != settings.text_zoom ||
                                 settings_.split_view != settings.split_view ||
                                 settings_.primary_sources != settings.primary_sources ||
                                 settings_.secondary_sources != settings.secondary_sources;
    if (settings_.word_study != settings.word_study) {
        highlighted_.reset();
        highlighted_verse_.reset();
        if (verse_links_)
            verse_links_->Hide();
        if (word_clicked_)
            word_clicked_(std::nullopt);
    }
    settings_ = settings;
    const auto colors = palette(settings.theme);
    SetBackgroundColour(colors.paper);
    SetForegroundColour(colors.ink);
    if (verse_links_)
        ui::recolor(verse_links_, colors);
    if (metrics_changed)
        invalidate();
    else
        request_repaint();
    if (highlighted_verse_ || !selections_.empty())
        rebuild_verse_links();
}
void ScriptureView::invalidate() {
    const double marker_screen = guide_y_ - offset_;
    const auto anchor = first_visible();
    const double fraction =
        rows_.empty() ? 0 : (offset_ - positions_[anchor]) / std::max(1, heights_[anchor]);
    cache_.clear();
    order_.clear();
    ++layout_epoch_;
    clear_tiles();
    layout_width_ = GetClientSize().x;
    for (std::size_t i = 0; i < rows_.size(); ++i)
        heights_[i] = estimated_height(rows_[i]);
    rebuild_positions();
    if (!rows_.empty()) {
        heights_[anchor] = row_layout(anchor).height;
        rebuild_positions();
        offset_ = positions_[anchor] + fraction * heights_[anchor];
        target_ = offset_;
    }
    prepare_visible();
    scrollbar();
    schedule_prefetch();
    request_repaint();
    guide_y_ = offset_ + marker_screen;
    follow_target_ = offset_;
    if (speech_active(playback_.state)) {
        locate_playback();
        if (guide_ && playback_.state != SpeechState::Playing)
            guide_y_ = guide_->y;
        if (guide_)
            start_animation();
    }
}
const ScriptureView::Layout& ScriptureView::row_layout(std::size_t index) const {
    if (auto it = cache_.find(index); it != cache_.end()) {
        order_.splice(order_.end(), order_, it->second.recency);
        return it->second;
    }
    ++render_stats_.layout_builds;
    wxClientDC dc(const_cast<ScriptureView*>(this));
    dc.SetFont(body_font(reading_points));
    Layout layout;
    if (rows_[index].heading)
        layout.height = estimated_height(rows_[index]);
    else {
        const auto selected_sources = shown_sources(index);
        const bool stacked = stacked_columns();
        const bool stanza = corpus_.stanza_book(displayed_.book, rows_[index].ref.chapter);
        const auto segments = reading_.segments();
        int height = 0;
        for (const auto& source : selected_sources) {
            const auto language = language_of(source);
            const auto fetch_begin = std::chrono::steady_clock::now();
            Column column;
            column.language = language;
            column.source = source;
            std::vector<TextFragment> fragments;
            std::optional<std::pair<std::string, VerseRef>> previous;
            const auto add = [&](const wxString& text, const wxString& label, VerseRef first, VerseRef last,
                                 bool faint, bool hebrew) {
                column.verses.emplace_back(first, last);
                column.faint.push_back(faint);
                column.hebrew.push_back(hebrew);
                column.prescribed.push_back(
                    !hebrew && std::any_of(segments.begin(), segments.end(), [&](const auto& p) {
                        return p.book == displayed_.book && overlap_refs(first, last, p.first, p.last);
                    }));
                fragments.push_back({text, label, int(column.verses.size() - 1)});
            };
            for (const auto& ref : rows_[index].verses) {
                const auto framing = corpus_.verse(frame_, displayed_.book, ref);
                const auto last = framing ? framing->last.value_or(ref) : ref;
                auto verse = source.empty() ? std::expected<Verse, std::string>{std::unexpected(
                                                  "Ingen aktiverad källa för kapitlet")}
                                            : corpus_.parallel_verse(frame_, source, displayed_.book, ref);
                // One verse of this edition can span several framing verses.
                if ((verse && previous && std::pair{verse->book, verse->ref} == *previous) ||
                    (!verse && verse.error() == merged_verse && !column.verses.empty())) {
                    column.verses.back().second = last;
                    continue;
                }
                if (verse)
                    previous = std::pair{verse->book, verse->ref};
                else
                    previous.reset();
                // The Septuagint's number, and this edition's own where it differs.
                wxString number = wxString::Format("%d", ref.verse) + ui::utf8(ref.suffix);
                if (verse && source != frame_) {
                    const auto& own = verse->ref;
                    if (verse->book != displayed_.book || own.chapter != ref.chapter ||
                        own.verse != ref.verse || own.suffix != ref.suffix) {
                        wxString mark =
                            (verse->book != displayed_.book
                                 ? ui::utf8(corpus_.book_abbreviation(verse->book)) + " "
                                 : wxString()) +
                            (own.chapter != ref.chapter ? wxString::Format("%d:", own.chapter) : wxString()) +
                            wxString::Format("%d", own.verse) + ui::utf8(own.suffix);
                        if (verse->last)
                            mark += ui::utf8("–") +
                                    (verse->last->chapter != own.chapter
                                         ? wxString::Format("%d:", verse->last->chapter)
                                         : wxString()) +
                                    wxString::Format("%d", verse->last->verse) +
                                    ui::utf8(verse->last->suffix);
                        number += " (" + mark + ")";
                    }
                }
                add(ui::utf8(verse ? verse->text : verse.error()), number, ref, last, !verse, false);
                if (source != frame_)
                    if (auto extra = orphans(source).find(ref); extra != orphans(source).end())
                        for (const auto& [book, own] : extra->second)
                            if (const auto text = corpus_.verse(source, book, own))
                                add(ui::utf8(text->text),
                                    ui::utf8("Utan OSB-ref · " + corpus_.book_abbreviation(book) + " ") +
                                        wxString::Format("%d:%d", own.chapter, own.verse) +
                                        ui::utf8(own.suffix),
                                    ref, ref, true, true);
            }
            const auto typeset_begin = std::chrono::steady_clock::now();
            render_stats_.fragment_ms +=
                std::chrono::duration<double, std::milli>(typeset_begin - fetch_begin).count();
            // Poetry, and columns of under about 40 characters, are set ragged right.
            const double letter = alphabet_width() / 26;
            ParagraphStyle style;
            style.ragged = stanza || column_width() < 40 * letter;
            style.hanging_indent = stanza ? 3 * letter : 0;
            column.text = layout_paragraph(dc, fragments, std::max(80, column_width()), language, style);
            render_stats_.typesetting_ms +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - typeset_begin)
                    .count();
            const int h =
                column.text.height() + (stacked && selected_sources.size() > 1 ? FromDIP(zoomed(24)) : 0);
            height = stacked ? height + h : std::max(height, h);
            layout.columns.push_back(std::move(column));
        }
        layout.height = height + FromDIP(zoomed(14));
    }
    while (cache_.size() >= cache_limit) {
        cache_.erase(order_.front());
        order_.pop_front();
    }
    order_.push_back(index);
    layout.recency = std::prev(order_.end());
    return cache_.emplace(index, std::move(layout)).first->second;
}
} // namespace ortho
