#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace ortho {
void resume_speech(std::vector<SpeechUtterance>& queue, const ReadingPosition& position) {
    const auto start = std::find_if(queue.begin(), queue.end(), [&](const auto& item) {
        return item.cue && item.cue->reading == 0 && item.cue->section == position.section &&
               item.cue->book == position.book && item.cue->source == position.source &&
               item.cue->verse <= position.verse && position.verse <= item.cue->last;
    });
    if (start != queue.end())
        queue.erase(queue.begin(), start);
}
std::vector<SpeechUtterance> reading_speech(const CorpusDb& corpus, const std::vector<Reading>& readings,
                                            const PronunciationLookup& lookup) {
    std::vector<SpeechUtterance> queue;
    std::map<std::string, PronunciationMatcher> matchers;
    const auto prepare = [&](const std::string& text, const std::string& language) {
        auto found = matchers.find(language);
        if (found == matchers.end())
            found = matchers.try_emplace(language, language, lookup(language)).first;
        return found->second.utterance(text);
    };
    for (size_t r = 0; r < readings.size(); ++r) {
        const auto& reading = readings[r];
        const auto localized = corpus.localize(reading);
        const auto passages = localized.segments();
        for (size_t s = 0; s < passages.size(); ++s) {
            const auto& passage = passages[s];
            // Walk the framing verses and read the left pane's text for each,
            // so Hebrew-only verses are not read and merged verses are read once.
            auto frame = reading.source_override.empty()
                             ? corpus.frame_source(reading.base_language, passage.book)
                             : reading.source_override;
            const auto* canon = corpus.canon_book(passage.book, passage.first.chapter);
            auto source =
                reading.source_override.empty()
                    ? corpus.source_for_language(reading.base_language, canon ? canon->code : passage.book)
                    : reading.source_override;
            if (corpus.coordinates(frame, passage.book).empty())
                frame = source;
            const auto language = source_language(source);
            bool found = false;
            std::optional<VerseRef> previous;
            for (auto ref : corpus.coordinates(frame, passage.book)) {
                if (ref > passage.last)
                    break;
                if (ref < passage.first)
                    continue;
                auto verse = corpus.parallel_verse(frame, source, passage.book, ref);
                const bool continues = (verse && previous && verse->ref == *previous) ||
                                       (!verse && verse.error() == merged_verse);
                if (continues && found) {
                    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): queued verses have cues.
                    queue.back().cue->last = ref;
                    continue;
                }
                if (!verse)
                    continue;
                previous = verse->ref;
                auto utterance = prepare(verse->text, language);
                utterance.cue = SpeechCue{r, s, passage.book, frame, ref, ref};
                queue.push_back(std::move(utterance));
                found = true;
            }
            if (!found)
                throw std::runtime_error("Ingen text finns att läsa upp.");
        }
    }
    return queue;
}
} // namespace ortho
