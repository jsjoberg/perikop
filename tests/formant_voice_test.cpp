#include "speech/formant_voice.hpp"
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
        // Every phone in the voice pack's lexicon.tsv, g2p-corpus.tsv and custom_lexicon.tsv,
        // with length marks attached. A phone without a mapping would be silently skipped.
        const auto unknown =
            ortho::formant_unknown_phones("a b d e eː f h i iː j k l m n o oː p r s t uː v y yː øː ŋ œ ɑː ɔ "
                                          "ɕ ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ ʉː ʊ ʏ ʧ");
        for (const auto& phone : unknown)
            std::cerr << "No formant mapping for " << phone << '\n';
        check(unknown.empty(), "every lexicon phone has a formant mapping");
        check(ortho::formant_unknown_phones("ˈa Q") == std::vector<std::string>{"Q"},
              "unknown symbols are reported");

        // KokoroText::ipa() for "Herren är min herde, mig skall intet fattas."
        const auto speech = ortho::formant_speech("hˈɛrən ˈɛːr mˈiːn hˈeːɖə, mˈɛj skˈal ˈɪntət fˈatas.");
        const double seconds = speech.size() / 24000.0;
        double peak = 0, energy = 0;
        for (float sample : speech) {
            check(std::isfinite(sample), "finite samples");
            peak = std::max(peak, double(std::abs(sample)));
            energy += double(sample) * sample;
        }
        check(seconds > 2 && seconds < 6, "a short sentence lasts a few seconds at 24 kHz");
        check(peak > 0.1 && peak <= 1 && std::sqrt(energy / speech.size()) > 0.01,
              "audible without clipping");

        const auto pause = ortho::formant_speech(", .");
        check(pause.size() == 4800 && std::all_of(pause.begin(), pause.end(),
                                                  [](float s) {
                                                      return s == 0;
                                                  }),
              "text without phones is a short pause");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
