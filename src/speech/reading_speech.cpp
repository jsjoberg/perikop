#include "speech/reading_speech.hpp"
#include "core/reading_display.hpp"
#include "storage/database.hpp"
#include <stdexcept>
namespace ortho {
std::vector<SpeechUtterance> reading_speech(const CorpusDb& corpus, const std::vector<Reading>& readings,
                                            const PronunciationLookup& lookup) {
    std::vector<SpeechUtterance> queue;
    for (size_t r = 0; r < readings.size(); ++r) {
        const auto& reading = readings[r];
        const auto localized = corpus.localize(reading);
        const auto passages = localized.segments();
        if (reading.base_language == "sv") {
            // Announced in the numbers shown on screen.
            const Reading announced{reading.kind, displayed_passage(passages.front()),
                                    passage_label(corpus, {passages.front()})};
            auto intro = make_utterance(reading_introduction(announced), "sv", lookup("sv"));
            intro.cue = SpeechCue{
                r, 0, passages.front().book, "", passages.front().first, passages.front().last, true};
            queue.push_back(std::move(intro));
        }
        for (size_t s = 0; s < passages.size(); ++s) {
            const auto& passage = passages[s];
            // Walk the framing verses and read the left pane's text for each,
            // so Hebrew-only verses are not read and merged verses are read once.
            auto frame = reading.source_override.empty() ? frame_source(reading.base_language, passage.book)
                                                         : reading.source_override;
            const auto* canon = canon_book(passage.book, passage.first.chapter);
            auto source = reading.source_override.empty()
                              ? source_for_language(reading.base_language, canon ? canon->code : passage.book)
                              : reading.source_override;
            if (corpus.coordinates(frame, passage.book).empty())
                frame = source;
            const auto language = source_language(source);
            const auto lexicon = lookup(language);
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
                auto utterance = make_utterance(verse->text, language, lexicon);
                utterance.cue = SpeechCue{r, s, passage.book, frame, ref, ref, false};
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
