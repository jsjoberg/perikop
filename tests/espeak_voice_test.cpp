#include "speech/espeak_voice.hpp"
#include "speech/swedish_text.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
// Sign changes per sample. Repeated renders differ sample by sample (eSpeak varies its voice source
// at random) but keep this within about 0.1 %; another voice, such as sv+f2, differs by about 20 %.
double crossings(const std::vector<float>& audio) {
    size_t count = 0;
    for (size_t i = 1; i < audio.size(); ++i)
        count += (audio[i - 1] < 0) != (audio[i] < 0);
    return double(count) / double(std::max<size_t>(1, audio.size()));
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 2, "Expected the bundled speech data directory");
        ortho::SwedishText front(argv[1]);
        using Token = ortho::SpeechToken;
        check(front.tokens("Kors män") ==
                  std::vector<Token>{{Token::Kind::Phonemes, "k'u:s."}, {Token::Kind::Phonemes, "m'En"}},
              "the bundled data is read as eSpeak codes");
        check(front.tokens("Han ja")[0].text == "hA:n" && front.tokens("han")[0].text == "hA:n",
              "function words are unstressed by their spelling, in any case");
        check(front.phonemes("män och ja") == "m'En Ok j'A:",
              "a content word that sounds like a function word keeps its stress");
        check(front.tokens("⟦f'Os.⟧ ja") ==
                  std::vector<Token>{{Token::Kind::Phonemes, "f'Os."}, {Token::Kind::Phonemes, "j'A:"}},
              "given codes keep their dots instead of ending a sentence");
        check(front.phonemes("Ja?! Xyzzy, 3.") == "j'A:? ⟨xyzzy⟩, tR'e:.",
              "repeated end marks, spelled words and numbers");
        using Kind = ortho::SpeechToken::Kind;
        const auto words = [](std::initializer_list<ortho::SpeechToken> tokens) {
            return std::vector<ortho::SpeechToken>(tokens);
        };
        const auto phrase = words({{Kind::Phonemes, "h'ERE-n"},
                                   {Kind::Phonemes, "E:R"},
                                   {Kind::Phonemes, "mi:n"},
                                   {Kind::Phonemes, "h'e:RdE-"},
                                   {Kind::Punctuation, ","},
                                   {Kind::Phonemes, "mEj"},
                                   {Kind::Phonemes, "skal"},
                                   {Kind::Phonemes, "'IntE-t"},
                                   {Kind::Phonemes, "f'atas"},
                                   {Kind::Punctuation, "."}});
        check(ortho::espeak_phonemes(phrase) == "[[h'ERE-n E:R mi:n h'e:RdE-]], [[mEj skal 'IntE-t f'atas]].",
              "adjacent words share one phoneme group, punctuation stays outside");
        check(ortho::espeak_phonemes({}) == "" &&
                  ortho::espeak_phonemes(words({{Kind::Punctuation, ","}, {Kind::Punctuation, "."}})) ==
                      ", .",
              "empty text and bare punctuation");
        check(ortho::espeak_phonemes(words({{Kind::Phonemes, "Ok"},
                                            {Kind::Spelled, "xyzzy"},
                                            {Kind::Phonemes, "hA:n"},
                                            {Kind::Punctuation, "."}})) == "[[Ok]] xyzzy [[hA:n]].",
              "a word without known phonemes is read by eSpeak's own rules");
        check(ortho::espeak_voice_name("espeak") == "sv" &&
                  ortho::espeak_voice_name("espeak-f2") == "sv+f2" &&
                  ortho::espeak_voice_name("espeak:sv+adam") == "sv+adam" &&
                  ortho::espeak_voice_name("alice").empty(),
              "installed voice selectors and legacy identities");

        // The library is the user's own installation; without it only the conversion is tested.
        std::unique_ptr<ortho::Espeak> espeak;
        std::vector<ortho::EspeakVoice> voices;
        try {
            voices = ortho::Espeak::voices();
            espeak = std::make_unique<ortho::Espeak>();
        } catch (const std::runtime_error& error) {
            std::cout << "eSpeak NG synthesis skipped: " << error.what() << '\n';
            return 0;
        }
        std::cout << "eSpeak NG " << espeak->version() << '\n';
        const auto& tokens = phrase;
        const auto male = espeak->speak(tokens, {});
        const double seconds = male.size() / 24000.0;
        check(seconds > 2 && seconds < 6, "a psalm phrase lasts a few seconds at 150 words per minute");
        check(std::ranges::all_of(male,
                                  [](float x) {
                                      return std::isfinite(x) && std::abs(x) <= 1;
                                  }),
              "finite bounded samples");
        check(std::ranges::any_of(male,
                                  [](float x) {
                                      return std::abs(x) > 0.05f;
                                  }),
              "audible speech");
        // eSpeak varies its voice source slightly at random, which moves the end by a pitch period.
        const auto again = espeak->speak(tokens, {}).size();
        check(std::max(again, male.size()) - std::min(again, male.size()) < 240,
              "the same input keeps its timing");
        check(!voices.empty(), "installed Swedish voices are discovered before synthesis starts");
        const auto during = ortho::Espeak::voices();
        check(during.size() == voices.size(), "discovery also works while a speech engine is open");
        const auto after = espeak->speak(tokens, {});
        check(std::max(after.size(), male.size()) - std::min(after.size(), male.size()) < 240,
              "discovery keeps the playback timing");
        const auto reference = crossings(male);
        check(std::abs(crossings(after) - reference) < 0.02 * reference &&
                  std::abs(crossings(espeak->speak(tokens, {.voice = "sv+f2"})) - reference) >
                      0.1 * reference,
              "discovery does not replace the playback voice");
        std::vector<std::string> ids;
        for (const auto& voice : voices) {
            check(!voice.name.empty() && std::ranges::find(ids, voice.id) == ids.end(),
                  "discovered choices have names and unique identities");
            ids.push_back(voice.id);
            const auto native = ortho::espeak_voice_name(voice.id);
            check(!native.empty(), "every discovered identity has a native selector");
            check(!native.starts_with("mb/"), "MBROLA voices, with their own sample rate, are not offered");
            espeak->speak(tokens, {.voice = native}, [] {
                return true;
            });
        }
        std::cout << voices.size() << " installed Swedish voice choices\n";
        check(espeak->speak(tokens, {.voice = "sv+f2"}) != male, "the female variant sounds different");
        check(espeak->speak(tokens, {.rate = 200}).size() < male.size(), "a faster rate is shorter");
        check(espeak
                  ->speak(tokens, {},
                          [] {
                              return true;
                          })
                  .empty(),
              "cancelling returns no audio");
        bool missing = false;
        try {
            espeak->speak(tokens, {.voice = "no-such-voice"});
        } catch (const std::runtime_error&) {
            missing = true;
        }
        check(missing, "an unknown voice is an error");
        bool second = false;
        try {
            ortho::Espeak another;
        } catch (const std::logic_error&) {
            second = true;
        }
        check(second, "eSpeak's global state allows one instance");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
