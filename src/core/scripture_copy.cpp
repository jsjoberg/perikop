#include "core/scripture_copy.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace ortho {
std::string scripture_copy(const CorpusDb& corpus, const std::string& frame,
                           const std::vector<std::pair<std::string, Passage>>& passages) {
    const auto sources = corpus.sources();
    std::string result, text, source, edition;
    std::vector<Passage> references;
    std::set<std::tuple<std::string, std::string, VerseRef>> copied;
    const auto flush = [&] {
        if (text.empty())
            return;
        if (!result.empty())
            result += "\n\n";
        result += text + "\n\n— ";
        std::string book;
        for (const auto& p : references) {
            if (!book.empty())
                result += "; ";
            if (book != p.book)
                result += corpus.book_name(p.book) + " ";
            book = p.book;
            const auto ref = [](VerseRef v) {
                return std::to_string(v.verse) + v.suffix;
            };
            result += std::to_string(p.first.chapter) + ":" + ref(p.first);
            if (p.first != p.last)
                result += "–" +
                          (p.first.chapter == p.last.chapter ? "" : std::to_string(p.last.chapter) + ":") +
                          ref(p.last);
        }
        result += " (" + edition + ")";
        text.clear();
        references.clear();
    };
    for (const auto& [shown_source, passage] : passages) {
        bool starts_passage = true;
        for (const auto& framing : corpus.coordinates(frame, passage.book)) {
            if (!corpus.contains(frame, passage, framing))
                continue;
            for (const auto& [book, ref] : corpus.counterparts(frame, shown_source, passage.book, framing)) {
                const auto verse = corpus.verse(shown_source, book, ref);
                if (!verse || verse->text.empty() || !copied.emplace(shown_source, book, verse->ref).second)
                    continue;
                const auto catalog = std::ranges::find(sources, shown_source, &Source::code);
                const auto name = shown_source == "sv1917" && corpus.deuterocanonical_book(book)
                                      ? std::string{"Svenska 1921"}
                                  : catalog == sources.end() ? shown_source
                                                             : catalog->name;
                if (source != shown_source || edition != name) {
                    flush();
                    source = shown_source;
                    edition = name;
                }
                if (!text.empty())
                    text += starts_passage ? "\n\n" : " ";
                text += verse->text;
                starts_passage = false;
                const auto& coordinates = corpus.coordinates(shown_source, book);
                const auto at = std::ranges::find(coordinates, verse->ref);
                if (!references.empty() && references.back().book == book && at != coordinates.begin() &&
                    at != coordinates.end() && *(at - 1) == references.back().last)
                    references.back().last = verse->last.value_or(verse->ref);
                else
                    references.push_back({book, verse->ref, verse->last.value_or(verse->ref)});
            }
        }
    }
    flush();
    return result;
}
} // namespace ortho
