#include "speech/compact_voice.hpp"
#include "speech/swedish_numbers.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Call> void rejects(Call&& call, const char* message) {
    try {
        call();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}
void phase_fixture(const std::filesystem::path& pack, int second_pitch = 120) {
    std::filesystem::create_directories(pack);
    // Pack text is byte-preserved on disk, including on Windows. Text-mode
    // fixture streams would introduce CRLF unlike the committed voice pack.
    std::ofstream(pack / "revision.txt", std::ios::binary) << std::string(64, 'a') << '\n';
    std::ofstream(pack / "phones.tsv", std::ios::binary) << "0\ta\t2400\n";
    std::ofstream(pack / "lexicon.tsv", std::ios::binary) << "a\ta\n";
    std::ofstream output(pack / "units.bin", std::ios::binary);
    const auto put = [&](uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i)
            output.put(char((value >> (8 * i)) & 255));
    };
    output.write("BJU1", 4);
    put(24000, 4);
    put(1, 4);
    put(2, 4);
    for (unsigned unit = 0; unit < 2; ++unit) {
        put(unit ? 0 : 65535, 2);
        put(0, 2);
        put(2880, 4);
        put(240, 2);
        put(240, 2);
        for (size_t i = 0; i < 2880; ++i) {
            const double sample =
                0.3 * std::sin(2 * std::numbers::pi * (unit ? second_pitch : 120) * i / 24000 + unit * 1.4);
            const double code = std::copysign(std::log1p(255 * std::abs(sample)) / std::log(256), sample);
            output.put(char(std::lround((code + 1) * 127.5)));
        }
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        using namespace ortho;
        check(argc == 3, "Expected voice pack and isolated fixture directory");
        const std::filesystem::path pack(argv[1]), temporary(argv[2]);
        CompactVoice voice(pack);
        CompactVoiceStats stats;
        const std::string text = "Mose såg en stjärna. Herren är min herde.";
        const auto pcm = voice.generate(text, {}, &stats);
        check(stats.words == 8 && stats.spelled_words == 0 && stats.diphones > 0,
              "New sentence combinations must resolve words and reusable units without neural inference");
        check(pcm.size() > 24000 && pcm == voice.generate(text),
              "Native synthesis must be nonempty and deterministic");
        check(std::all_of(pcm.begin(), pcm.end(),
                          [](float value) {
                              return std::isfinite(value) && std::abs(value) <= 1;
                          }) &&
                  std::any_of(pcm.begin(), pcm.end(),
                              [](float value) {
                                  return std::abs(value) > 0.01;
                              }),
              "End-to-end synthesis must produce finite, unclipped, non-silent speech");
        check(voice.ipa("23") == voice.ipa("tjugotre") && voice.generate("23") == voice.generate("tjugotre"),
              "Numbers must share the existing Swedish normalization and the same acoustic path");
        check(swedish_numbers("1000 1,25") == "ettusen ett komma två fem",
              "Shared number pronunciation changed");
        check(voice.ipa("oss") == "ˈɔs", "The compact lexicon must preserve the existing word fixes");
        const auto explicit_pcm = voice.generate("⟦manˈasə⟧", {}, &stats);
        check(!explicit_pcm.empty() && stats.words == 1 && !stats.spelled_words,
              "Exact pronunciation overrides must work");
        voice.generate("zzqxxzz", {}, &stats);
        check(stats.spelled_words == 1, "Unknown words must be spelled explicitly rather than dropped");
        rejects(
            [&] {
                voice.generate("⟦☃⟧");
            },
            "Unsupported phonemes must not disappear silently");
        rejects(
            [&] {
                voice.generate("⟦man");
            },
            "Incomplete exact pronunciations must be rejected");
        rejects(
            [&] {
                voice.generate(std::string("a\xff", 2));
            },
            "Invalid UTF-8 must be rejected before normalization");
        rejects(
            [&] {
                voice.generate(text, [] {
                    return true;
                });
            },
            "Cancellation must interrupt unit assembly");
        check(voice.generate("...").size() == size_t{3} * 7680,
              "Punctuation must preserve explicit silence durations");
        std::filesystem::remove_all(temporary);
        for (int pitch : {120, 170}) {
            phase_fixture(temporary, pitch);
            CompactVoice phase(temporary);
            const auto sine = phase.generate("⟦aaaaaaaaaa⟧");
            double minimum = 1, maximum = 0;
            for (size_t at = 800; at + 1000 < sine.size(); at += 100) {
                double energy = 0;
                for (size_t i = 0; i < 400; ++i) {
                    const double sample = sine[at + i];
                    energy += sample * sample;
                }
                const double rms = std::sqrt(energy / 400);
                minimum = std::min(minimum, rms);
                maximum = std::max(maximum, rms);
            }
            check(maximum > 0.2 && minimum / maximum > (pitch == 120 ? 0.92 : 0.85),
                  "Voiced joins must avoid cancellation both at matching and different source pitches");
        }
        std::filesystem::resize_file(temporary / "units.bin", 22);
        rejects(
            [&] {
                CompactVoice invalid(temporary);
            },
            "Truncated voice data must fail cleanly");
        std::filesystem::remove_all(temporary);
        std::cout << "Compact Swedish text-to-speech, phase alignment, and data validation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
