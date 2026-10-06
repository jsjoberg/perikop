#include "core/reading_display.hpp"
#include "storage/database.hpp"
namespace ortho {
Passage displayed_passage(Passage passage) {
    // A framing passage in the book and chapter numbers the reader shows.
    if (const auto* canon = canon_book(passage.book, passage.first.chapter)) {
        passage.book = canon->code;
        passage.first.chapter -= canon->offset();
        passage.last.chapter -= canon->offset();
    }
    return passage;
}
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages) {
    std::string label, book;
    for (const auto& framing : passages) {
        const auto p = displayed_passage(framing);
        if (!label.empty())
            label += "; ";
        if (p.book != book) {
            label += corpus.book_name(p.book) + " ";
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
