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
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages, bool short_names) {
    std::string label, book;
    for (const auto& p : passages) {
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
