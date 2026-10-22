#include "ui/reading_tooltips.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include <algorithm>
#include <wx/wxcrt.h>

namespace ortho::ui {
wxString text_preview(const std::string& text, bool continues) {
    wxString result;
    bool space = false;
    for (const auto c : utf8(text)) {
        if (wxIsspace(c)) {
            space = !result.empty();
            continue;
        }
        if (space)
            result += ' ';
        result += c;
        space = false;
    }
    if (result.length() > reading_preview_limit - (continues ? 1 : 0)) {
        result = result.Left(reading_preview_limit - 1);
        const auto boundary = result.find_last_of(' ');
        if (boundary != wxString::npos)
            result = result.Left(boundary);
        result.Trim();
        continues = true;
    }
    if (continues && !result.empty())
        result += utf8("…");
    return result;
}
wxString verse_tooltip(const CorpusDb& corpus, const std::string& frame, const std::string& book,
                       VerseRef ref, bool preview, bool continues) {
    const auto* canon = corpus.canon_book(book, ref.chapter);
    for (const auto* language : {"sv", "en", "el"}) {
        const auto source = corpus.source_for_language(language, canon ? canon->code : book);
        if (source.empty())
            continue;
        auto verse = corpus.parallel_verse(frame, source, book, ref);
        // A merged translation covers this verse too, even though the reader prints it only once.
        if (!verse && verse.error() == merged_verse)
            if (const auto map = corpus.alignment(frame, source, {book, ref, ref}))
                verse = corpus.parallel_verse(frame, source, book, map->from.first);
        if (!verse || verse->text.empty())
            continue;
        const auto text = preview ? text_preview(verse->text, continues) : utf8(verse->text);
        return source_language(source) == "sv"   ? text
               : source_language(source) == "en" ? utf8("Engelska:\n") + text
                                                 : utf8("Grekiska:\n") + text;
    }
    return {};
}
wxString reading_tooltip(const CorpusDb& corpus, const Reading& reading) {
    if (!reading.can_open())
        return {};
    const auto passages = reading.segments();
    for (std::size_t i = 0; i < passages.size(); ++i) {
        const auto& passage = passages[i];
        const auto frame = corpus.frame_source(reading.base_language, passage.book);
        const auto& refs = corpus.coordinates(frame, passage.book);
        for (const auto& ref : refs) {
            if (!corpus.contains(frame, passage, ref))
                continue;
            const auto text = verse_tooltip(corpus, frame, passage.book, ref, true,
                                            ref != passage.last || i + 1 < passages.size());
            if (!text.empty())
                return text;
        }
    }
    return {};
}
wxString plan_parts_tooltip(const CorpusDb& corpus, const PlanPart& part) {
    wxString result;
    for (const auto& range : part) {
        if (!result.empty())
            result += '\n';
        result += utf8(plan_label(corpus, {range}));
    }
    return result;
}
} // namespace ortho::ui
