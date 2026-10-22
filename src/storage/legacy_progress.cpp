#include "storage/legacy_progress.hpp"
#include "storage/database.hpp"
#include <optional>
#include <stdexcept>
namespace ortho {
namespace {
// Where a plan book lay in its pre-OSB frame. Brenton's Greek joins Ezra and Nehemiah as
// 2 Esdras 1–23 and prints Esther with its additions as one book; every other book framed itself.
struct LegacyBook {
    std::string frame_book;
    int offset = 0;
};
LegacyBook legacy_book(const std::string& book) {
    if (book == "Neh")
        return {"Ezra", 10};
    if (book == "Esth")
        return {"EsthGr", 0};
    return {book, 0};
}
} // namespace
std::string legacy_frame_source(const CorpusDb& corpus, const std::string& language,
                                const std::string& book) {
    return corpus.new_testament_book(book) ? corpus.source_for_language(language, book) : "grc-lxx";
}
Reading legacy_plan_reading(const CorpusDb& corpus, const PlanPart& part, const std::string& language) {
    std::vector<Passage> passages;
    std::string reference;
    bool new_testament = false;
    for (const auto& range : part) {
        const auto [frame_book, offset] = legacy_book(range.book);
        const auto frame = legacy_frame_source(corpus, language, frame_book);
        const int first = range.first + offset, last = range.last + offset;
        std::optional<VerseRef> begin, end;
        for (const auto& ref : corpus.coordinates(frame, frame_book))
            if (first <= ref.chapter && ref.chapter <= last) {
                if (!begin)
                    begin = ref;
                end = ref;
            }
        if (!begin || !end)
            continue;
        passages.push_back({frame_book, *begin, *end});
        reference = frame;
        new_testament = corpus.new_testament_book(range.book);
    }
    if (passages.empty())
        throw std::runtime_error("Delen har ingen text i denna utgåva.");
    Reading reading{new_testament ? ReadingKind::Gospel : ReadingKind::OldTestament,
                    passages.front(),
                    {passages.begin() + 1, passages.end()},
                    language};
    reading.reference = reference;
    return reading;
}
std::string legacy_reading_key(const Reading& reading) {
    std::string key = "reading:" + reading.base_language + ":" + reading.source_override;
    for (const auto& part : reading.segments())
        key += ":" + part.book + " " + std::to_string(part.first.chapter) + ":" +
               std::to_string(part.first.verse) + part.first.suffix + "-" +
               std::to_string(part.last.chapter) + ":" + std::to_string(part.last.verse) + part.last.suffix;
    return key;
}
} // namespace ortho
