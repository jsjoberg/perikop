#include "speech/espeak_voice.hpp"
#include "speech/kokoro_text.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
// Reads Swedish text through the voice pack's front end and the installed eSpeak NG, as the app will.
namespace {
std::filesystem::path path(const char* text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text)));
}
void wav(const std::filesystem::path& file, const std::vector<float>& samples) {
    std::ofstream out(file, std::ios::binary);
    const auto put = [&](uint32_t v, int n) {
        for (int i = 0; i < n; ++i)
            out.put(char((v >> (8 * i)) & 255));
    };
    const auto data = uint32_t(samples.size() * 2);
    out.write("RIFF", 4);
    put(36 + data, 4);
    out.write("WAVEfmt ", 8);
    put(16, 4);
    put(1, 2);
    put(1, 2);
    put(24000, 4);
    put(48000, 4);
    put(2, 2);
    put(16, 2);
    out.write("data", 4);
    put(data, 4);
    for (float sample : samples)
        put(uint16_t(int16_t(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767))), 2);
    if (!out)
        throw std::runtime_error("Cannot write probe audio.");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc < 4 || argc > 6)
            throw std::runtime_error("Usage: espeak-audio-probe PACK INPUT OUTPUT.wav [VOICE] [RATE]");
        std::ifstream input(path(argv[2]));
        if (!input)
            throw std::runtime_error("Cannot read probe input.");
        const std::string text{std::istreambuf_iterator<char>(input), {}};
        ortho::Espeak::Settings settings;
        if (argc > 4)
            settings.voice = argv[4];
        if (argc > 5)
            settings.rate = std::stoi(argv[5]);
        const auto ipa = ortho::KokoroText(path(argv[1])).ipa(text);
        ortho::Espeak espeak;
        const auto audio = espeak.speak(ipa, settings);
        wav(path(argv[3]), audio);
        std::cout << "espeak=" << espeak.version() << ", seconds=" << audio.size() / 24000.0
                  << "\nphonemes=" << ortho::espeak_phonemes(ipa) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
