#include "core/reading_position.hpp"
#include "storage/database.hpp"

namespace ortho {
std::string legacy_reading_key(const Reading& reading) {
    std::string key = "reading:" + reading.base_language + ":" + reading.source_override;
    for (const auto& part : reading.segments())
        key += ":" + part.book + " " + std::to_string(part.first.chapter) + ":" +
               std::to_string(part.first.verse) + part.first.suffix + "-" +
               std::to_string(part.last.chapter) + ":" + std::to_string(part.last.verse) + part.last.suffix;
    return key;
}
std::string reading_key(const CorpusDb& corpus, const Reading& reading) {
    std::string key = "reading:osb-v1:" + reading.base_language + ":" + reading.source_override;
    for (const auto& part : corpus.localize(reading).segments())
        key += ":" + part.book + " " + std::to_string(part.first.chapter) + ":" +
               std::to_string(part.first.verse) + part.first.suffix + "-" +
               std::to_string(part.last.chapter) + ":" + std::to_string(part.last.verse) + part.last.suffix;
    return key;
}
std::optional<ReadingPosition> reading_position(const CorpusDb& corpus, const Reading& reading,
                                                std::size_t section, VerseRef verse) {
    const auto parts = corpus.localize(reading).segments();
    std::size_t total = 0, before = 0;
    std::optional<ReadingPosition> result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto& part = parts[i];
        std::string source = osb_reference;
        if (corpus.coordinates(source, part.book).empty())
            source = corpus.source_for_language(reading.base_language, part.book);
        for (const auto& ref : corpus.coordinates(source, part.book)) {
            if (!corpus.contains(source, part, ref))
                continue;
            if (i == section && ref == verse) {
                before = total;
                result = ReadingPosition{i, part.book, source, ref};
            }
            ++total;
        }
    }
    if (result && total)
        result->percent = int(100 * before / total);
    return result;
}
} // namespace ortho
