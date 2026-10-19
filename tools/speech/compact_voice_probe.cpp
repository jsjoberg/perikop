#include "speech/compact_voice.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4)
            throw std::runtime_error(
                "Usage: compact-voice-probe PACK OUTPUT.wav [TEXT]; otherwise reads stdin");
        std::string text;
        if (argc == 4)
            text = argv[3];
        else {
            std::ostringstream input;
            input << std::cin.rdbuf();
            text = input.str();
        }
        const auto started = std::chrono::steady_clock::now();
        const auto path = [](const char* value) {
            return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value)));
        };
        ortho::CompactVoice voice(path(argv[1]));
        const auto loaded = std::chrono::steady_clock::now();
        ortho::CompactVoiceStats stats;
        const auto pcm = voice.generate(text, {}, &stats);
        const auto finished = std::chrono::steady_clock::now();
        std::ofstream output(path(argv[2]), std::ios::binary);
        const auto put = [&](uint32_t value, int bytes) {
            for (int i = 0; i < bytes; ++i)
                output.put(char((value >> (8 * i)) & 255));
        };
        output.write("RIFF", 4);
        put(36 + uint32_t(pcm.size() * 2), 4);
        output.write("WAVEfmt ", 8);
        put(16, 4);
        put(1, 2);
        put(1, 2);
        put(24000, 4);
        put(48000, 4);
        put(2, 2);
        put(16, 2);
        output.write("data", 4);
        put(uint32_t(pcm.size() * 2), 4);
        double peak = 0;
        for (auto sample : pcm) {
            if (!std::isfinite(sample) || std::abs(sample) > 1)
                throw std::runtime_error("Invalid output sample");
            peak = std::max(peak, double(std::abs(sample)));
            put(uint16_t(int16_t(std::lround(sample * 32767))), 2);
        }
        if (!output)
            throw std::runtime_error("Cannot write compact voice WAV");
        std::cout << "{\"load_seconds\":" << std::chrono::duration<double>(loaded - started).count()
                  << ",\"render_seconds\":" << std::chrono::duration<double>(finished - loaded).count()
                  << ",\"audio_seconds\":" << pcm.size() / 24000.0 << ",\"words\":" << stats.words
                  << ",\"spelled_words\":" << stats.spelled_words << ",\"diphones\":" << stats.diphones
                  << ",\"backoffs\":" << stats.backoffs << ",\"peak\":" << peak << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
