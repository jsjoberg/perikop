#include "core/reading_display.hpp"
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

        const Reading psalm{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}, "Psalm 23"};
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

        Reading merged{ReadingKind::OldTestament, {"1Chr", {12, 4}, {12, 5}}, "Första Krönikeboken"};
        merged.reference = "grc-lxx";
        const auto merged_speech = reading_speech(corpus, {merged}, lexicon);
        check(merged_speech.size() == 2, "A merged Swedish verse must be read only once");
        check(merged_speech[1].display_text == corpus.verse("sv1917", "1Chr", {12, 4})->text &&
                  merged_speech[1].cue == SpeechCue{0, 0, "1Chr", "grc-lxx", {12, 4}, {12, 5}, false},
              "One utterance must cover both framing verses in a merged passage");

        Reading nehemiah{ReadingKind::OldTestament, {"Neh", {1, 1}, {1, 2}}, "Nehemja"};
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

        const Reading sections{
            ReadingKind::Gospel, {"John", {1, 1}, {1, 1}}, "Johannesevangeliet", {{"John", {1, 3}, {1, 4}}}};
        const auto batch = reading_speech(corpus, {sections, psalm}, lexicon);
        check(batch.size() == 11 && batch[1].cue->reading == 0 && batch[1].cue->section == 0 &&
                  batch[2].cue->section == 1 && batch[3].cue->section == 1 && batch[4].cue->reading == 1 &&
                  batch[4].cue->introduction,
              "Multiple readings and sections must preserve their playback identities");

        Reading english{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 2}}, "Psalm 23", {}, "en"};
        english.source_override = "en-kjv";
        const auto explicit_edition = reading_speech(corpus, {english}, lexicon);
        check(explicit_edition.size() == 2 && explicit_edition[0].language == "en" &&
                  explicit_edition[0].cue == SpeechCue{0, 0, "Ps", "en-kjv", {23, 1}, {23, 1}, false} &&
                  explicit_edition[0].display_text == corpus.verse("en-kjv", "Ps", {23, 1})->text,
              "An explicit edition must retain its own coordinates and skip a Swedish introduction");

        Reading untranslated{
            ReadingKind::OldTestament, {"1Kgs", {2, 35, "a"}, {2, 35, "b"}}, "Första Kungaboken"};
        untranslated.reference = "grc-lxx";
        bool rejected = false;
        try {
            reading_speech(corpus, {untranslated}, lexicon);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "Ingen text finns att läsa upp.";
        }
        check(rejected, "A passage without Swedish text must report that no speech is available");
        check(reading_speech(corpus, {}, lexicon).empty(), "An empty batch must contain no utterances");
        std::cout << "Reading speech checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
