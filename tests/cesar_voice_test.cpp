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
            ortho::CesarTrace baseline, reference, compact, lf_trace, clear_trace, edges_trace;
            const auto before = ortho::cesar_speech(ipa, {}, &baseline);
            const auto after = ortho::cesar_speech(ipa, {.reference_pitch = points}, &reference);
            const auto candidate = ortho::cesar_speech(ipa, {.compact_melody = true}, &compact);
            const auto lf = ortho::cesar_speech(
                ipa, {.compact_melody = true, .source = {ortho::VoiceSource::LiljencrantsFant}}, &lf_trace);
            const ortho::CesarOptions clear_options{
                .compact_melody = true,
                .source = {ortho::VoiceSource::LiljencrantsFant, {0.40f, 0.65f, 0.008f}, true}};
            const auto clear = ortho::cesar_speech(ipa, clear_options, &clear_trace);
            const ortho::CesarOptions edges_options{
                .compact_melody = true, .source = {.smooth_controls = true, .smooth_closure = true}};
            const auto edges = ortho::cesar_speech(ipa, edges_options, &edges_trace);
            check(before.size() == 347160 && after.size() == before.size() &&
                      candidate.size() == before.size() && lf.size() == before.size() &&
                      clear.size() == before.size() && edges.size() == before.size(),
                  "melody changes preserve the reviewed 14.465-second reading");
            check(before != after && before != candidate, "melody controls affect synthesized audio");
            check(lf != candidate, "LF source changes timbre");
            for (const auto* trace : {&lf_trace, &clear_trace, &edges_trace}) {
                check(trace->pitch.size() == compact.pitch.size(), "same number of pitch controls");
                for (size_t i = 0; i < compact.pitch.size(); ++i)
                    check(trace->pitch[i].seconds == compact.pitch[i].seconds &&
                              trace->pitch[i].hz == compact.pitch[i].hz,
                          "source changes preserve every pitch control");
            }
            check(lf == ortho::cesar_speech(
                            ipa, {.compact_melody = true, .source = {ortho::VoiceSource::LiljencrantsFant}}),
                  "LF rendering is deterministic");
            check(clear != lf && clear == ortho::cesar_speech(ipa, clear_options),
                  "refined source is distinct and deterministic");
            check(edges != candidate && edges == ortho::cesar_speech(ipa, edges_options),
                  "polynomial edge experiment is distinct and deterministic");
            for (const auto* trace : {&reference, &compact, &lf_trace, &clear_trace, &edges_trace}) {
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
            for (const auto* audio : {&after, &candidate, &lf, &clear, &edges})
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
        // A glottal flow derivative must integrate to zero over each cycle; its
        // integrated flow cannot become negative or accumulate a baseline offset.
        for (const ortho::LfParameters shape :
             {ortho::LfParameters{}, ortho::LfParameters{0.35f, 0.60f, 0.02f},
              ortho::LfParameters{0.40f, 0.65f, 0.008f}, ortho::LfParameters{0.42f, 0.70f, 0.08f}}) {
            const ortho::LfPulse pulse(shape);
            double flow = 0, minimum = 0, peak_flow = 0;
            constexpr int steps = 20000;
            for (int i = 0; i < steps; ++i) {
                const auto value = pulse.sample((i + 0.5f) / steps);
                check(std::isfinite(value), "finite LF cycle");
                flow += value / steps;
                minimum = std::min(minimum, flow);
                peak_flow = std::max(peak_flow, flow);
            }
            check(std::abs(flow) < 1e-5 && minimum > -1e-5 && peak_flow > 0.01,
                  "LF flow rises and returns to zero without DC accumulation");
            check(pulse.sample(0) == 0 && pulse.sample(1) == 0, "LF cycle endpoints are zero");
            check(std::abs(pulse.sample(shape.te) + 1) < 0.01,
                  "LF closure excitation is normalized independently of shape");
            check(std::abs(pulse.sample(shape.te - 1e-5f) - pulse.sample(shape.te + 1e-5f)) < 0.01,
                  "LF open and return phases meet continuously");
        }
        bool invalid_shape = false;
        try {
            const ortho::LfPulse invalid({0.4f, 0.65f, 0.5f});
        } catch (const std::invalid_argument&) {
            invalid_shape = true;
        }
        check(invalid_shape, "LF return time must fit within the period");
        bool wrong_source = false;
        try {
            ortho::cesar_speech(
                "a", {.source = {.kind = ortho::VoiceSource::LiljencrantsFant, .smooth_closure = true}});
        } catch (const std::invalid_argument&) {
            wrong_source = true;
        }
        check(wrong_source, "polynomial closure correction cannot silently apply to LF");
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
