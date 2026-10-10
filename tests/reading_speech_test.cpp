#include "core/reading_display.hpp"
#include "speech/read_aloud.hpp"
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

        const Reading psalm{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}};
        const auto spoken = reading_speech(corpus, {psalm}, lexicon);
        check(spoken.size() == 7, "Psalm 23 must have one introduction and six Swedish verses");
        check(spoken[0].display_text == "Läsning ur Psaltaren, kapitel 22, vers 1 till 6.",
              "The introduction must announce the reader's framing numbers");
        check(spoken[0].cue == SpeechCue{0, 0, "Ps", "", {22, 1}, {22, 6}, true},
              "Introductions must retain framing coordinates for following");
        check(spoken[1].display_text == corpus.verse("sv1917", "Ps", {23, 1})->text,
              "Septuagint framing must still read the corresponding Swedish text");
        check(spoken[1].speech_text.find("Herren-test") != std::string::npos &&
                  spoken[1].display_text.find("Herren-test") == std::string::npos,
              "User pronunciation overrides must affect speech only");
        for (std::size_t i = 1; i < spoken.size(); ++i) {
            check(spoken[i].language == "sv" &&
                      spoken[i].cue == SpeechCue{0, 0, "Ps", "grc-lxx", {22, int(i)}, {22, int(i)}, false},
                  "Spoken verses must follow the framing order and cue source");
        }

        Reading merged{ReadingKind::OldTestament, {"1Chr", {12, 4}, {12, 5}}};
        merged.reference = "grc-lxx";
        const auto merged_speech = reading_speech(corpus, {merged}, lexicon);
        check(merged_speech.size() == 2, "A merged Swedish verse must be read only once");
        check(merged_speech[1].display_text == corpus.verse("sv1917", "1Chr", {12, 4})->text &&
                  merged_speech[1].cue == SpeechCue{0, 0, "1Chr", "grc-lxx", {12, 4}, {12, 5}, false},
              "One utterance must cover both framing verses in a merged passage");

        Reading nehemiah{ReadingKind::OldTestament, {"Neh", {1, 1}, {1, 2}}};
        nehemiah.reference = "sv1917";
        const auto nehemiah_speech = reading_speech(corpus, {nehemiah}, lexicon);
        check(nehemiah_speech.size() == 3 &&
                  nehemiah_speech[0].display_text == "Läsning ur Nehemja, kapitel 1, vers 1 till 2.",
              "The introduction must use the displayed Nehemiah chapter, not Ezra 11");
        check(nehemiah_speech[1].cue == SpeechCue{0, 0, "Ezra", "grc-lxx", {11, 1}, {11, 1}, false} &&
                  nehemiah_speech[1].display_text == corpus.verse("sv1917", "Neh", {1, 1})->text,
              "Nehemiah must retain its framing cue and its Swedish text");
        check(passage_label(corpus, {{"Ezra", {11, 1}, {11, 2}}, {"Ezra", {11, 4}, {11, 4}}}) ==
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
        check(batch.size() == 11 && batch[1].cue->reading == 0 && batch[1].cue->section == 0 &&
                  batch[2].cue->section == 1 && batch[3].cue->section == 1 && batch[4].cue->reading == 1 &&
                  batch[4].cue->introduction,
              "Multiple readings and sections must preserve their playback identities");

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
        check(custom_speech.size() == 6 &&
                  custom_speech[1].display_text == corpus.verse("sv1917", "Ps", {23, 1})->text &&
                  custom_speech[2].cue->verse == VerseRef{22, 2} &&
                  custom_speech[3].cue == SpeechCue{0, 1, "John", "sv1917", {1, 3}, {1, 3}, false} &&
                  custom_speech[4].cue->verse == VerseRef{1, 4} &&
                  custom_speech[5].cue == SpeechCue{0, 2, "Ps", "grc-lxx", {22, 6}, {22, 6}, false},
              "Custom playback must skip unselected gaps and follow the chosen range order across books");

        Reading english{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 2}}, {}, "en"};
        english.source_override = "en-kjv";
        const auto explicit_edition = reading_speech(corpus, {english}, lexicon);
        check(explicit_edition.size() == 2 && explicit_edition[0].language == "en" &&
                  explicit_edition[0].cue == SpeechCue{0, 0, "Ps", "en-kjv", {23, 1}, {23, 1}, false} &&
                  explicit_edition[0].display_text == corpus.verse("en-kjv", "Ps", {23, 1})->text,
              "An explicit edition must retain its own coordinates and skip a Swedish introduction");

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

        ReadAloud read_aloud;
        check(!read_aloud.status(corpus), "Nothing being read must leave the address field to the window");
        read_aloud.start({sections, psalm}, batch);
        read_aloud.update({SpeechState::Loading, {}, 0, 0});
        check(read_aloud.status(corpus)->title == "Laddar rösten…", "Loading must be shown");
        read_aloud.update({SpeechState::Buffering, {}, 0, 0, 0.4});
        check(read_aloud.status(corpus)->title == "Förbereder uppläsningen… 40 %" &&
                  !read_aloud.status(corpus)->tooltip.empty(),
              "Startup buffering must be shown at once with its share of ready audio");
        read_aloud.update({SpeechState::Playing, batch[4].cue, 0, 0.5});
        check(read_aloud.status(corpus)->title == batch[4].display_text,
              "An introduction must show what the voice says");
        read_aloud.update({SpeechState::Paused, batch[1].cue, 0, 0.1});
        check(read_aloud.status(corpus)->title ==
                  "Pausad · " + passage_label(corpus, {{"John", {1, 1}, {1, 1}}}),
              "A paused verse must show where the reading is");
        const auto start = ReadAloud::Clock::now();
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
        read_aloud.start({}, {{"", "", "sv", SpeechCue{0, 0, "Ps", "", {22, 1}, {22, 6}, true}}});
        read_aloud.update({SpeechState::Playing, SpeechCue{0, 0, "Ps", "", {22, 1}, {22, 6}, true}, 0, 0});
        check(read_aloud.status(corpus)->title == "Introduktion", "An unspoken introduction must be named");
        std::cout << "Reading speech checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
