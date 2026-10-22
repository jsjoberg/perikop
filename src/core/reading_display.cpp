#include "core/reading_display.hpp"
#include "storage/database.hpp"
namespace ortho {
std::vector<Passage> bookmark_passages(const CorpusDb& corpus, const Bookmark& mark) {
    Reading reading{ReadingKind::OldTestament, mark.passage};
    reading.reference = mark.source;
    try {
        return corpus.localize(reading).segments();
    } catch (const std::exception&) {
        return {};
    }
}
std::string bookmark_label(const CorpusDb& corpus, const Bookmark& mark) {
    const auto parts = bookmark_passages(corpus, mark);
    if (!parts.empty())
        return passage_label(corpus, parts);
    return "OSB-mappning saknas · " + mark.source + " · " + mark.passage.book + " " +
           std::to_string(mark.passage.first.chapter) + ":" + std::to_string(mark.passage.first.verse) +
           mark.passage.first.suffix;
}
Passage displayed_passage(const CorpusDb& corpus, Passage passage) {
    // A framing passage in the book and chapter numbers the reader shows.
    if (const auto* canon = corpus.canon_book(passage.book, passage.first.chapter)) {
        passage.book = canon->code;
        passage.first.chapter -= canon->offset();
        passage.last.chapter -= canon->offset();
    }
    return passage;
}
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages, bool short_names) {
    std::string label, book;
    for (const auto& framing : passages) {
        const auto p = displayed_passage(corpus, framing);
        if (!label.empty())
            label += "; ";
        if (p.book != book) {
            label += (short_names ? corpus.short_title(p.book) : corpus.title(p.book)) + " ";
            book = p.book;
        }
        label += std::to_string(p.first.chapter) + ":" + std::to_string(p.first.verse) + p.first.suffix;
        if (p.last != p.first)
            label += "–" + (p.last.chapter != p.first.chapter ? std::to_string(p.last.chapter) + ":" : "") +
                     std::to_string(p.last.verse) + p.last.suffix;
    }
    return label;
}
} // namespace ortho
