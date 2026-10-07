#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <set>
#include <wx/dcclient.h>
namespace ortho {
std::vector<std::string> ScriptureView::languages() const {
    std::vector<std::string> result{base_source_.starts_with("sv")    ? "sv"
                                    : base_source_.starts_with("en-") ? "en"
                                                                      : "el"};
    const auto add = [&](const std::string& language) {
        if (std::find(result.begin(), result.end(), language) == result.end())
            result.push_back(language);
    };
    if (!settings_.parallel.empty())
        add(settings_.parallel);
    return result;
}
int ScriptureView::columns_count() const {
    return int(languages().size());
}
int ScriptureView::outside_margin() const {
    const int width = GetClientSize().x;
    if (columns_count() == 1)
        return std::max(32, (width - std::min(FromDIP(680), width - 64)) / 2);
    return std::clamp(width / 14, 32, 105);
}
int ScriptureView::column_width() const {
    const int usable = std::max(120, GetClientSize().x - 2 * outside_margin());
    const bool stacked = usable / columns_count() < 270;
    return stacked ? usable - 30 : (usable - 44 * (columns_count() - 1)) / columns_count() - 30;
}
void ScriptureView::open(const Reading& reading) {
    reading_ = reading;
    open_section(0);
}
void ScriptureView::open_section(std::size_t index) {
    const auto segments = reading_.segments();
    if (index >= segments.size())
        return;
    displayed_ = segments[index];
    rows_.clear();
    located_cue_.reset();
    highlighted_.reset();
    speech_row_.reset();
    guide_.reset();
    guide_alpha_ = 0;
    orphans_.clear();
    if (selection_) {
        selection_.reset();
        if (selection_changed_)
            selection_changed_();
    }
    // The Septuagint frames the Old Testament: its books, order and numbers.
    frame_ = reading_.source_override.empty() ? frame_source(reading_.base_language, displayed_.book)
                                              : reading_.source_override;
    if (corpus_.coordinates(frame_, displayed_.book).empty())
        for (const std::string language : {"sv", "el", "en"}) {
            const auto source = source_for_language(language, displayed_.book);
            if (!corpus_.coordinates(source, displayed_.book).empty()) {
                frame_ = source;
                break;
            }
        }
    canon_ = canon_book(displayed_.book, displayed_.first.chapter);
    base_source_ = reading_.source_override.empty()
                       ? source_for_language(reading_.base_language, canon_code())
                       : reading_.source_override;
    // Paragraphs follow the left pane's edition, placed at its framing verses.
    std::set<VerseRef> boundaries;
    if (base_source_ == frame_) {
        for (auto ref : corpus_.paragraph_starts(frame_, displayed_.book))
            boundaries.insert(ref);
    } else
        for (auto ref : corpus_.paragraph_starts(base_source_, canon_code()))
            for (const auto& [book, target] : corpus_.counterparts(base_source_, frame_, canon_code(), ref))
                if (book == displayed_.book) {
                    boundaries.insert(target);
                    break;
                }
    int chapter = 0;
    const bool stanza = displayed_.book == "Ps" || displayed_.book == "Ps151" || displayed_.book == "Prov" ||
                        displayed_.book == "Song" || displayed_.book == "Lam";
    for (auto ref : corpus_.coordinates(frame_, displayed_.book)) {
        if (canon_ && (ref.chapter < canon_->first_chapter || ref.chapter > canon_->last_chapter))
            continue;
        const bool new_chapter = ref.chapter != chapter;
        if (new_chapter) {
            chapter = ref.chapter;
            rows_.push_back({ref, true, {}, ref});
        }
        if (new_chapter || stanza || boundaries.contains(ref))
            rows_.push_back({ref, false, {}, ref});
        rows_.back().verses.push_back(ref);
        const auto verse = corpus_.verse(frame_, displayed_.book, ref);
        rows_.back().last = verse ? verse->last.value_or(ref) : ref;
    }
    cache_.clear();
    order_.clear();
    offset_ = target_ = 0;
    wheel_timer_.Stop();
    heights_.clear();
    for (const auto& row : rows_)
        heights_.push_back(row.heading ? FromDIP(124) : FromDIP(150));
    rebuild_positions();
    center_passage();
    Refresh(false);
}
std::string ScriptureView::source_of(const std::string& language) const {
    return language == languages().front() ? base_source_ : source_for_language(language, canon_code());
}
const ScriptureView::Orphans& ScriptureView::orphans(const std::string& source) const {
    if (auto it = orphans_.find(source); it != orphans_.end())
        return it->second;
    auto& result = orphans_[source];
    if (source == frame_ || frame_ != "grc-lxx")
        return result;
    // The edition's books that this framing book draws on, such as Swedish Nehemiah.
    std::set<std::string> books;
    for (const auto& row : rows_)
        if (!row.heading)
            for (const auto& ref : row.verses)
                for (const auto& [book, target] :
                     corpus_.counterparts(frame_, source, displayed_.book, ref)) {
                    (void)target;
                    books.insert(book);
                }
    for (const auto& book : books) {
        std::optional<VerseRef> anchor;
        std::vector<VerseRef> pending;
        for (const auto& ref : corpus_.coordinates(source, book)) {
            const auto targets = corpus_.counterparts(source, frame_, book, ref);
            if (targets.empty()) {
                if (anchor)
                    result[*anchor].emplace_back(book, ref);
                else
                    pending.push_back(ref);
                continue;
            }
            const auto here = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
                return target.first == displayed_.book &&
                       (!canon_ || (canon_->first_chapter <= target.second.chapter &&
                                    target.second.chapter <= canon_->last_chapter));
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
        return !r.heading && r.ref <= displayed_.first && displayed_.first <= r.last;
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
    const auto verse_it = std::find_if(rows_[index].verses.begin(), rows_[index].verses.end(), [&](auto ref) {
        auto v = corpus_.verse(frame_, displayed_.book, ref);
        return ref <= displayed_.first && v && displayed_.first <= v->last.value_or(ref);
    });
    const int tag = int(verse_it - rows_[index].verses.begin());
    if (!paragraph.columns.empty()) {
        const auto& text = paragraph.columns.front().text;
        for (std::size_t line = 0; line < text.lines.size(); ++line)
            if (std::any_of(text.lines[line].runs.begin(), text.lines[line].runs.end(), [&](const auto& run) {
                    return run.tag == tag;
                })) {
                if (line > 2)
                    position = positions_[index] + (line - 2) * text.line_height;
                break;
            }
    }
    set_position(position);
    target_ = offset_;
}
bool ScriptureView::passage_in_view() const {
    // Rows at the very edges, or under the button, do not count as seen.
    const double top = offset_ + FromDIP(40), bottom = offset_ + GetClientSize().y - FromDIP(90);
    for (auto i = first_visible(); i < rows_.size() && positions_[i] < bottom; ++i)
        if (positions_[i + 1] > top && !rows_[i].heading && rows_[i].ref <= displayed_.last &&
            displayed_.first <= rows_[i].last)
            return true;
    return false;
}
void ScriptureView::apply(const Settings& settings) {
    settings_ = settings;
    const auto colors = palette(settings.theme);
    SetBackgroundColour(colors.paper);
    SetForegroundColour(colors.ink);
    invalidate();
}
void ScriptureView::invalidate() {
    const double marker_screen = guide_y_ - offset_;
    const auto anchor = first_visible();
    const double fraction =
        rows_.empty() ? 0 : (offset_ - positions_[anchor]) / std::max(1, heights_[anchor]);
    cache_.clear();
    order_.clear();
    layout_width_ = GetClientSize().x;
    for (std::size_t i = 0; i < rows_.size(); ++i)
        heights_[i] = rows_[i].heading ? FromDIP(124) : FromDIP(150);
    rebuild_positions();
    if (!rows_.empty()) {
        heights_[anchor] = row_layout(anchor).height;
        rebuild_positions();
        offset_ = positions_[anchor] + fraction * heights_[anchor];
        target_ = offset_;
    }
    prepare_visible();
    scrollbar();
    Refresh(false);
    guide_y_ = offset_ + marker_screen;
    follow_target_ = offset_;
    if (playback_.state == SpeechState::Paused || playback_.state == SpeechState::Buffering) {
        locate_playback();
        if (guide_)
            guide_y_ = guide_->y;
    }
}
const ScriptureView::Layout& ScriptureView::row_layout(std::size_t index) const {
    if (auto it = cache_.find(index); it != cache_.end())
        return it->second;
    wxClientDC dc(const_cast<ScriptureView*>(this));
    dc.SetFont(body_font(settings_.font_size));
    Layout layout;
    if (rows_[index].heading)
        layout.height = FromDIP(124);
    else {
        const auto selected_languages = languages();
        const bool stacked = (GetClientSize().x - 2 * outside_margin()) / columns_count() < 270;
        const int offset = canon_ ? canon_->offset() : 0;
        const auto segments = reading_.segments();
        int height = 0;
        for (const auto& language : selected_languages) {
            const auto source = source_of(language);
            Column column;
            column.language = language;
            column.source = source;
            std::vector<TextFragment> fragments;
            std::optional<VerseRef> previous;
            const auto add = [&](const wxString& text, const wxString& label, VerseRef first, VerseRef last,
                                 bool faint, bool hebrew) {
                column.verses.emplace_back(first, last);
                column.faint.push_back(faint);
                column.hebrew.push_back(hebrew);
                column.prescribed.push_back(
                    !hebrew && std::any_of(segments.begin(), segments.end(), [&](const auto& p) {
                        return p.book == displayed_.book && p.first <= last && first <= p.last;
                    }));
                fragments.push_back({text, label, int(column.verses.size() - 1)});
            };
            for (const auto& ref : rows_[index].verses) {
                const auto framing = corpus_.verse(frame_, displayed_.book, ref);
                const auto last = framing ? framing->last.value_or(ref) : ref;
                auto verse = corpus_.parallel_verse(frame_, source, displayed_.book, ref);
                // One verse of this edition can span several framing verses.
                if ((verse && previous && verse->ref == *previous) ||
                    (!verse && verse.error() == "Ingår i föregående vers" && !column.verses.empty())) {
                    column.verses.back().second = last;
                    continue;
                }
                if (verse)
                    previous = verse->ref;
                // The Septuagint's number, and this edition's own where it differs.
                wxString number = wxString::Format("%d", ref.verse) + ui::utf8(ref.suffix);
                if (verse && source != frame_) {
                    const auto& own = verse->ref;
                    if (own.chapter != ref.chapter - offset || own.verse != ref.verse ||
                        own.suffix != ref.suffix) {
                        wxString mark =
                            (own.chapter != ref.chapter - offset ? wxString::Format("%d:", own.chapter)
                                                                 : wxString()) +
                            wxString::Format("%d", own.verse) + ui::utf8(own.suffix);
                        if (verse->last)
                            mark += "–" + wxString::Format("%d", verse->last->verse) +
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
                                    wxString::Format("hebr. %d:%d", own.chapter, own.verse) +
                                        ui::utf8(own.suffix),
                                    ref, ref, true, true);
            }
            column.text = layout_paragraph(dc, fragments, std::max(80, column_width()), language);
            const int h = column.text.height() + (stacked && selected_languages.size() > 1 ? FromDIP(24) : 0);
            height = stacked ? height + h : std::max(height, h);
            layout.columns.push_back(std::move(column));
        }
        layout.height = height + FromDIP(14);
    }
    while (cache_.size() >= cache_limit) {
        cache_.erase(order_.front());
        order_.pop_front();
    }
    order_.push_back(index);
    return cache_.emplace(index, std::move(layout)).first->second;
}
} // namespace ortho
