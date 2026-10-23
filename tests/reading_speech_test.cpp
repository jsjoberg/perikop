#include "core/reading_display.hpp"
#include "speech/playback_controller.hpp"
#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    try {
        using namespace ortho;
        check(argc == 2, "Usage: ortho-reading-speech-tests CORPUS");
        const CorpusDb corpus(argv[1]);
        const PronunciationLookup lexicon = [&](const std::string& language) {
            auto result = corpus.pronunciations(language);
            result.insert(result.begin(), {"sv", "HERREN", "Herren-test", "", 1000});
            return result;
        };

        Reading psalm{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}};
        psalm.reference = "sv1917";
        const auto spoken = reading_speech(corpus, {psalm}, lexicon);
        check(spoken.size() == 6, "Psalm 23 must contain only its six Swedish verses");
        check(spoken[0].display_text == corpus.verse("sv1917", "Ps", {23, 1})->text,
              "Playback must start directly with Swedish Scripture in Septuagint framing");
        check(spoken[0].speech_text.find("Herren-test") != std::string::npos &&
                  spoken[0].display_text.find("Herren-test") == std::string::npos,
              "User pronunciation overrides must affect speech only");
        for (std::size_t i = 0; i < spoken.size(); ++i) {
            check(spoken[i].language == "sv" &&
                      spoken[i].cue ==
                          SpeechCue{0, 0, "Ps", osb_reference, {22, int(i + 1)}, {22, int(i + 1)}},
                  "Spoken verses must follow the framing order and cue source");
        }

        Reading merged{ReadingKind::OldTestament, {"1Chr", {12, 4}, {12, 5}}};
        merged.reference = "grc-lxx";
        const auto merged_speech = reading_speech(corpus, {merged}, lexicon);
        check(merged_speech.size() == 1, "A merged Swedish verse must be read only once");
        check(merged_speech[0].display_text == corpus.verse("sv1917", "1Chr", {12, 4})->text &&
                  merged_speech[0].cue == SpeechCue{0, 0, "1Chr", osb_reference, {12, 4}, {12, 5}},
              "One utterance must cover both framing verses in a merged passage");

        Reading nehemiah{ReadingKind::OldTestament, {"Neh", {1, 1}, {1, 2}}};
        nehemiah.reference = "sv1917";
        const auto nehemiah_speech = reading_speech(corpus, {nehemiah}, lexicon);
        check(nehemiah_speech.size() == 2, "Nehemiah playback must contain only the selected verses");
        check(nehemiah_speech[0].cue == SpeechCue{0, 0, "Neh", osb_reference, {1, 1}, {1, 1}} &&
                  nehemiah_speech[0].display_text == corpus.verse("sv1917", "Neh", {1, 1})->text,
              "Nehemiah must retain its framing cue and its Swedish text");
        check(passage_label(corpus, {{"Neh", {1, 1}, {1, 2}}, {"Neh", {1, 4}, {1, 4}}}) ==
                  "Nehemja 1:1–2; 1:4",
              "Passage labels must apply canon offsets and omit repeated book names");

        const Reading sections{ReadingKind::Gospel, {"John", {1, 1}, {1, 1}}, {{"John", {1, 3}, {1, 4}}}};
        const auto batch = reading_speech(corpus, {sections, psalm}, lexicon);
        int lookups = 0;
        const auto reused = reading_speech(corpus, {sections, psalm}, [&](const auto& language) {
            ++lookups;
            return lexicon(language);
        });
        check(lookups == 1 && reused.size() == batch.size(),
              "A speech batch must fetch pronunciation rules once per language");
        for (std::size_t i = 0; i < batch.size(); ++i)
            check(reused[i].speech_text == batch[i].speech_text && reused[i].cue == batch[i].cue,
                  "Reusing pronunciation rules must preserve speech and playback coordinates");
        check(batch.size() == 9 && batch[0].cue->reading == 0 && batch[0].cue->section == 0 &&
                  batch[1].cue->section == 1 && batch[2].cue->section == 1 &&
                  batch[3].cue == SpeechCue{1, 0, "Ps", osb_reference, {22, 1}, {22, 1}},
              "Multiple readings and sections must move directly between Scripture verses");

        // A picker selection can mix LXX and New Testament coordinates without renumbering either.
        Reading custom{ReadingKind::OldTestament,
                       {"Ps", {22, 1}, {22, 2}},
                       {{"John", {1, 3}, {1, 4}}, {"Ps", {22, 6}, {22, 6}}}};
        custom.reference.clear();
        const auto localized = corpus.localize(custom).segments();
        check(localized.size() == 3 && localized[0].first == VerseRef{22, 1} && localized[1].book == "John" &&
                  localized[2].first == VerseRef{22, 6},
              "Mixed-book picker ranges must retain their framing coordinates and order");
        const auto custom_speech = reading_speech(corpus, {custom}, lexicon);
        check(custom_speech.size() == 5 &&
                  custom_speech[0].display_text == corpus.verse("sv1917", "Ps", {23, 1})->text &&
                  custom_speech[1].cue->verse == VerseRef{22, 2} &&
                  custom_speech[2].cue == SpeechCue{0, 1, "John", osb_reference, {1, 3}, {1, 3}} &&
                  custom_speech[3].cue->verse == VerseRef{1, 4} &&
                  custom_speech[4].cue == SpeechCue{0, 2, "Ps", osb_reference, {22, 6}, {22, 6}},
              "Custom playback must skip unselected gaps and follow the chosen range order across books");
        check(batch[0].display_text == corpus.verse("sv1917", "John", {1, 1})->text &&
                  batch[3].display_text == spoken[0].display_text,
              "Every reading in a batch must start with its first verse without an announcement");

        Reading english{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 2}}, {}, "en"};
        english.source_override = "en-lxx2012";
        const auto explicit_edition = reading_speech(corpus, {english}, lexicon);
        check(explicit_edition.size() == 2 && explicit_edition[0].language == "en" &&
                  explicit_edition[0].cue == SpeechCue{0, 0, "Ps", osb_reference, {23, 1}, {23, 1}} &&
                  explicit_edition[0].display_text == corpus.verse("en-lxx2012", "Ps", {23, 1})->text,
              "An explicit edition must start with its first verse and retain its own coordinates");

        Reading chapters{ReadingKind::Gospel, {"John", {1, 50}, {2, 2}}};
        chapters.reference = osb_reference;
        std::vector<int> resolved;
        const auto selected_sources = reading_speech(
            corpus, {chapters}, lexicon, [&](const Reading&, const std::string& book, int chapter) {
                check(book == "John", "Source selection receives the framing book");
                resolved.push_back(chapter);
                return chapter == 1 ? "sv1917" : "en-web";
            });
        check(resolved == std::vector<int>{1, 2} && selected_sources.front().language == "sv" &&
                  selected_sources.back().language == "en" &&
                  selected_sources.back().display_text == corpus.verse("en-web", "John", {2, 2})->text &&
                  selected_sources.back().cue == SpeechCue{0, 0, "John", osb_reference, {2, 2}, {2, 2}},
              "Each chapter uses its displayed source without changing section or OSB coordinates");

        Reading untranslated{ReadingKind::OldTestament, {"1Kgs", {2, 35, "a"}, {2, 35, "b"}}};
        untranslated.reference = "grc-lxx";
        bool rejected = false;
        try {
            reading_speech(corpus, {untranslated}, lexicon);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "Ingen text finns att läsa upp.";
        }
        check(rejected, "A passage without Swedish text must report that no speech is available");
        check(reading_speech(corpus, {}, lexicon).empty(), "An empty batch must contain no utterances");

        PlaybackController read_aloud;
        check(!read_aloud.status(corpus), "Nothing being read must leave the address field to the window");
        read_aloud.start({sections, psalm});
        read_aloud.update({SpeechState::Buffering, {}, 0, 0, 0.4});
        check(read_aloud.status(corpus)->title == "Förbereder uppläsningen… 40 %" &&
                  !read_aloud.status(corpus)->tooltip.empty(),
              "Startup buffering must be shown at once with its share of ready audio");
        read_aloud.update({SpeechState::Playing, batch[3].cue, 0, 0.5});
        check(read_aloud.status(corpus)->title == "Psaltaren 22:1",
              "The address field must show the first verse as each reading starts");
        read_aloud.update({SpeechState::Paused, batch[0].cue, 0, 0.1});
        check(read_aloud.status(corpus)->title ==
                  "Pausad: " + passage_label(corpus, {{"John", {1, 1}, {1, 1}}}),
              "A paused verse must show where the reading is");
        const auto start = PlaybackController::Clock::now();
        read_aloud.update({SpeechState::Playing, batch[2].cue, 0, 0.2}, start);
        read_aloud.update({SpeechState::Buffering, batch[2].cue, 0, 0.2}, start);
        read_aloud.update({SpeechState::Buffering, batch[2].cue, 0, 0.2},
                          start + std::chrono::milliseconds(179));
        check(read_aloud.feedback() == SpeechState::Playing,
              "A brief gap during playback must read as playing");
        read_aloud.update({SpeechState::Buffering, batch[2].cue, 0, 0.2},
                          start + std::chrono::milliseconds(180));
        check(read_aloud.feedback() == SpeechState::Buffering &&
                  read_aloud.status(corpus)->title == "Förbereder fortsättningen…",
              "A lasting gap must be shown as buffering");
        read_aloud.report("Ljudet försvann");
        read_aloud.update({SpeechState::Error, {}, 0, 0});
        check(read_aloud.status(corpus)->tooltip == "Ljudet försvann", "An error must keep its message");
        read_aloud.update({SpeechState::Stopped, {}, 0, 0});
        check(!read_aloud.status(corpus) && read_aloud.readings().size() == 2,
              "A stopped queue must leave the address field to the window");
        std::cout << "Reading speech checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
