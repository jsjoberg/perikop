#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace ortho {
double resume_speech(std::vector<SpeechUtterance>& queue, const ReadingPosition& position,
                     const CorpusDb* corpus) {
    const auto start = std::find_if(queue.begin(), queue.end(), [&](const auto& item) {
        return item.cue && item.cue->reading == 0 && item.cue->section == position.section &&
               item.cue->book == position.book && item.cue->source == position.source &&
               (corpus ? corpus->contains(item.cue->source, {item.cue->book, item.cue->verse, item.cue->last},
                                          position.verse)
                       : item.cue->verse <= position.verse && position.verse <= item.cue->last);
    });
    if (start == queue.end())
        return 0;
    const double skipped = double(start - queue.begin()) / double(queue.size());
    queue.erase(queue.begin(), start);
    return skipped;
}
std::vector<SpeechUtterance> reading_speech(const CorpusDb& corpus, const std::vector<Reading>& readings,
                                            const PronunciationLookup& lookup,
                                            const SpeechSourceLookup& source_lookup) {
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
            std::string frame = osb_reference;
            auto source = reading.source_override.empty()
                              ? corpus.source_for_language(reading.base_language, passage.book)
                              : reading.source_override;
            if (corpus.coordinates(frame, passage.book).empty())
                frame = source;
            bool found = false;
            bool source_read = false;
            std::map<int, std::string> sources;
            std::optional<std::pair<std::string, VerseRef>> previous;
            for (auto ref : corpus.coordinates(frame, passage.book)) {
                if (!corpus.contains(frame, passage, ref))
                    continue;
                if (source_lookup) {
                    auto [entry, added] = sources.try_emplace(ref.chapter);
                    if (added)
                        entry->second = source_lookup(reading, passage.book, ref.chapter);
                    if (source != entry->second) {
                        source = entry->second;
                        previous.reset();
                        source_read = false;
                    }
                }
                auto verse = corpus.parallel_verse(frame, source, passage.book, ref);
                // A selection may begin inside one indivisible translation verse.
                // Read that complete source verse rather than dropping the selected OSB point.
                if (!verse && verse.error() == merged_verse && !source_read)
                    if (const auto group = corpus.alignment(frame, source, {passage.book, ref, ref}))
                        verse = corpus.parallel_verse(frame, source, passage.book, group->from.first);
                const bool continues =
                    (verse && previous && std::pair{verse->book, verse->ref} == *previous) ||
                    (!verse && verse.error() == merged_verse);
                if (continues && source_read) {
                    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): queued verses have cues.
                    queue.back().cue->last = ref;
                    continue;
                }
                if (!verse) {
                    previous.reset();
                    continue;
                }
                previous = std::pair{verse->book, verse->ref};
                auto utterance = prepare(verse->text, source_language(source));
                utterance.cue = SpeechCue{r, s, passage.book, frame, ref, ref};
                queue.push_back(std::move(utterance));
                found = true;
                source_read = true;
            }
            if (!found)
                throw std::runtime_error("Ingen text finns att läsa upp.");
        }
    }
    return queue;
}
} // namespace ortho
