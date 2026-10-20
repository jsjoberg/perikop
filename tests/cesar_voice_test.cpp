#include "speech/cesar_voice.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char** argv) {
    try {
        // Every phone in the voice pack's lexicon.tsv, g2p-corpus.tsv and custom_lexicon.tsv,
        // with length marks attached. A phone without a mapping would be silently skipped.
        const auto unknown =
            ortho::cesar_unknown_phones("a b d e eː f h i iː j k l m n o oː p r s t uː v y yː øː ŋ œ ɑː ɔ "
                                        "ɕ ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ ʉː ʊ ʏ ʧ");
        for (const auto& phone : unknown)
            std::cerr << "Cesar has no sound for " << phone << '\n';
        check(unknown.empty(), "every lexicon phone has a Cesar sound");
        // Saying every phone looks up every sound element; a missing one throws.
        const auto all = ortho::cesar_speech("a b d e eː f h i iː j k l m n o oː p r s t uː v y yː øː ŋ œ ɑː "
                                             "ɔ ɕ ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ ʉː ʊ ʏ ʧ.");
        check(all == ortho::cesar_speech("a b d e eː f h i iː j k l m n o oː p r s t uː v y yː øː ŋ œ ɑː ɔ ɕ "
                                         "ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ ʉː ʊ ʏ ʧ."),
              "the same text always gives the same audio");
        check(ortho::cesar_unknown_phones("ˈa Q") == std::vector<std::string>{"Q"},
              "unknown symbols are reported");

        // KokoroText::ipa() for "Herren är min herde, mig skall intet fattas."
        const auto speech = ortho::cesar_speech("hˈɛrən ˈɛːr mˈiːn hˈeːɖə, mˈɛj skˈal ˈɪntət fˈatas.");
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

        const auto pause = ortho::cesar_speech(", .");
        check(pause.size() == 4800 && std::all_of(pause.begin(), pause.end(),
                                                  [](float s) {
                                                      return s == 0;
                                                  }),
              "text without phones is a short pause");
        if (argc > 1) {
            const std::filesystem::path fixtures(argv[1]);
            std::ifstream input(fixtures / "psalm.ipa.txt"), contour(fixtures / "psalm.pitch.tsv");
            check(bool(input) && bool(contour), "listening benchmark is available");
            const std::string ipa((std::istreambuf_iterator<char>(input)), {});
            std::vector<ortho::PitchPoint> points;
            float seconds, hz;
            while (contour >> seconds >> hz)
                points.push_back({seconds, hz});
            check(contour.eof() && !points.empty(), "benchmark contour is valid");
            ortho::CesarTrace baseline, reference, compact;
            const auto before = ortho::cesar_speech(ipa, {}, &baseline);
            const auto after = ortho::cesar_speech(ipa, {.reference_pitch = points}, &reference);
            const auto candidate = ortho::cesar_speech(ipa, {.compact_melody = true}, &compact);
            check(before.size() == 347160 && after.size() == before.size() &&
                      candidate.size() == before.size(),
                  "melody changes preserve the reviewed 14.465-second reading");
            check(before != after && before != candidate, "melody controls affect synthesized audio");
            for (const auto* trace : {&reference, &compact}) {
                check(trace->sounds.size() == baseline.sounds.size(), "same sound sequence");
                size_t offset = 0;
                for (size_t i = 0; i < trace->sounds.size(); ++i) {
                    const auto& actual = trace->sounds[i];
                    const auto& expected = baseline.sounds[i];
                    check(std::string(actual.name) == expected.name && actual.frames == expected.frames &&
                              actual.samples == expected.samples && actual.pause == expected.pause,
                          "every sound duration and pause stays fixed");
                    offset += actual.samples;
                }
                check(offset == before.size(), "trace accounts for every rendered sample");
            }
            for (const auto* audio : {&after, &candidate})
                check(std::all_of(audio->begin(), audio->end(),
                                  [](float value) {
                                      return std::isfinite(value) && std::abs(value) <= 1;
                                  }),
                      "alternative melodies produce finite bounded audio");
            // Invalid controls fail at the API boundary, before synthesis.
            bool rejected = false;
            try {
                const std::vector<ortho::PitchPoint> invalid{{0, 120},
                                                             {1, std::numeric_limits<float>::quiet_NaN()}};
                ortho::cesar_speech(ipa, {.reference_pitch = invalid});
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected, "invalid reference pitch is rejected");
        }
        // Identical two-syllable input, explicitly different lexical accents. Syllable
        // count must not decide accent identity; the second peak follows its label.
        const std::vector<ortho::ProsodySyllable> syllables{{0, 0.1f, 0.25f, 3}, {0, 0.35f, 0.5f, 0}};
        const std::vector<ortho::ProsodyPhrase> phrases{{0, 0.65f, '.'}};
        std::vector<ortho::SwedishWordProsody> words{{ortho::SwedishAccent::Accent1, 0.8f, false, true}};
        const auto accent1 = ortho::swedish_melody(syllables, words, phrases, 0.75f);
        words[0].accent = ortho::SwedishAccent::Accent2;
        const auto accent2 = ortho::swedish_melody(syllables, words, phrases, 0.75f);
        check(ortho::pitch_at(accent2, 0.43f) > ortho::pitch_at(accent1, 0.43f),
              "explicit accent 2 has its prominence rise in the following syllable");
        check(ortho::pitch_at(accent2, 0.12f) > ortho::pitch_at(accent2, 0.23f),
              "accent 2 falls in the primary stressed syllable");
        words[0].prominence = 0;
        const auto weak = ortho::swedish_melody(syllables, words, phrases, 0.75f);
        check(ortho::pitch_at(accent2, 0.43f) > ortho::pitch_at(weak, 0.43f),
              "prominence is independent of lexical accent and can be suppressed");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
