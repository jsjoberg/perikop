#include "speech/espeak_voice.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        // Reference conversions from tools/speech/espeak_swedish.py.
        check(ortho::espeak_phonemes("hˈɛrən ˈɛːr mˈiːn hˈeːɖə,mˈɛj skˈal ˈɪntət fˈatas.") ==
                  "[[h'ERE-n E:R mi:n h'e:RdE-]], [[mEj skal 'IntE-t f'atas]].",
              "psalm phrase with unstressed function words");
        check(ortho::espeak_phonemes("ɧˈøː ɕˈʏrka bˈɑːɳ bˈuːɖ fˈɔʂ kˈɑːʈa ɡrˈøːna") ==
                  "[[sx'Y: S;'yRka b'A:n. b'u:Rd f'Os. k'A:Rta gR'Y:na]]",
              "Swedish sounds, retroflexes and r");
        check(ortho::espeak_phonemes("ˈɔk dˈeːt rˈɛːknadəs hˈɔnɔm tˈɪl rˈɛtfɛːɖɪɡhˌeːt?") ==
                  "[[Ok de:t R'E:knadE-s hOnOm tIl R'EtfE:RdIgh,e:t]]?",
              "secondary stress and a question");
        check(ortho::espeak_phonemes("") == "" && ortho::espeak_phonemes(", .") == ", .",
              "empty text and bare punctuation");
        // Every phone in the voice pack's lexicons, as in cesar_voice_test.cpp.
        const auto unknown =
            ortho::espeak_unknown_phones("a b d e eː f h i iː j k l m n o oː p r s t uː v y yː "
                                         "øː ŋ œ ɑː ɔ ɕ ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ ʉː ʊ ʏ ʧ");
        for (const auto& phone : unknown)
            std::cerr << "eSpeak has no code for " << phone << '\n';
        check(unknown.empty(), "every lexicon phone has an eSpeak code");
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
        const std::string ipa = "hˈɛrən ˈɛːr mˈiːn hˈeːɖə,mˈɛj skˈal ˈɪntət fˈatas.";
        const auto male = espeak->speak(ipa, {});
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
        const auto again = espeak->speak(ipa, {}).size();
        check(std::max(again, male.size()) - std::min(again, male.size()) < 240,
              "the same input keeps its timing");
        check(!voices.empty(), "installed Swedish voices are discovered before synthesis starts");
        const auto during = ortho::Espeak::voices();
        check(during.size() == voices.size(), "discovery also works while a speech engine is open");
        const auto after = espeak->speak(ipa, {}).size();
        check(std::max(after, male.size()) - std::min(after, male.size()) < 240,
              "discovery does not replace the playback voice");
        std::vector<std::string> ids;
        for (const auto& voice : voices) {
            check(!voice.name.empty() && std::ranges::find(ids, voice.id) == ids.end(),
                  "discovered choices have names and unique identities");
            ids.push_back(voice.id);
            const auto native = ortho::espeak_voice_name(voice.id);
            check(!native.empty(), "every discovered identity has a native selector");
            check(!native.starts_with("mb/"), "MBROLA voices, with their own sample rate, are not offered");
            espeak->speak(ipa, {.voice = native}, [] {
                return true;
            });
        }
        std::cout << voices.size() << " installed Swedish voice choices\n";
        check(espeak->speak(ipa, {.voice = "sv+f2"}) != male, "the female variant sounds different");
        check(espeak->speak(ipa, {.rate = 200}).size() < male.size(), "a faster rate is shorter");
        check(espeak
                  ->speak(ipa, {},
                          [] {
                              return true;
                          })
                  .empty(),
              "cancelling returns no audio");
        bool missing = false;
        try {
            espeak->speak(ipa, {.voice = "no-such-voice"});
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
