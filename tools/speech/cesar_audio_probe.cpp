#include "speech/cesar_voice.hpp"
#include "speech/kokoro_text.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace {
std::filesystem::path path(const char* text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text)));
}
std::string read(const std::filesystem::path& file) {
    std::ifstream input(file);
    if (!input)
        throw std::runtime_error("Cannot read probe input.");
    return {(std::istreambuf_iterator<char>(input)), {}};
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
        if (argc < 4)
            throw std::runtime_error("Usage: cesar-audio-probe PACK INPUT OUTPUT.wav [--ipa] "
                                     "[--pitch CONTOUR|--compact] [--source klatt|lf] "
                                     "[--lf-shape TP TE TA] [--smooth-controls] [--smooth-closure] "
                                     "[--trace PREFIX] [--raw OUTPUT.f32]");
        ortho::CesarOptions options;
        std::vector<ortho::PitchPoint> reference;
        bool ipa = false;
        std::filesystem::path trace, raw;
        for (int i = 4; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--ipa") {
                ipa = true;
            } else if (option == "--compact") {
                options.compact_melody = true;
            } else if (option == "--smooth-controls") {
                options.source.smooth_controls = true;
            } else if (option == "--smooth-closure") {
                options.source.smooth_closure = true;
            } else if (option == "--lf-shape" && i + 3 < argc) {
                const auto number = [&]() {
                    const std::string value = argv[++i];
                    size_t consumed = 0;
                    const float result = std::stof(value, &consumed);
                    if (consumed != value.size())
                        throw std::runtime_error("Invalid LF shape number.");
                    return result;
                };
                options.source.lf = {number(), number(), number()};
            } else if (option == "--source" && i + 1 < argc) {
                const std::string source = argv[++i];
                if (source == "lf")
                    options.source.kind = ortho::VoiceSource::LiljencrantsFant;
                else if (source == "klatt")
                    options.source.kind = ortho::VoiceSource::Klatt;
                else
                    throw std::runtime_error("Unknown voice source.");
            } else if (option == "--pitch" && i + 1 < argc) {
                std::ifstream input(path(argv[++i]));
                if (!input)
                    throw std::runtime_error("Cannot read pitch contour.");
                float seconds, hz;
                while (input >> seconds >> hz)
                    reference.push_back({seconds, hz});
                if (!input.eof() || reference.empty())
                    throw std::runtime_error("Invalid pitch contour file.");
            } else if (option == "--trace" && i + 1 < argc) {
                trace = path(argv[++i]);
            } else if (option == "--raw" && i + 1 < argc) {
                raw = path(argv[++i]);
            } else {
                throw std::runtime_error("Unknown or incomplete probe option.");
            }
        }
        options.reference_pitch = reference;
        auto pronunciation = read(path(argv[2]));
        if (!ipa)
            pronunciation = ortho::KokoroText(path(argv[1])).ipa(pronunciation);
        // An IPA file's terminating newline is file formatting, not a spoken symbol.
        while (!pronunciation.empty() && (pronunciation.back() == '\n' || pronunciation.back() == '\r'))
            pronunciation.pop_back();
        ortho::CesarTrace details;
        const auto audio = ortho::cesar_speech(pronunciation, options, trace.empty() ? nullptr : &details);
        wav(path(argv[3]), audio);
        if (!raw.empty()) {
            std::ofstream output(raw, std::ios::binary);
            output.write(reinterpret_cast<const char*>(audio.data()), audio.size() * sizeof(float));
            if (!output)
                throw std::runtime_error("Cannot write raw probe audio.");
        }
        if (!trace.empty()) {
            auto timing_path = trace, pitch_path = trace;
            timing_path += ".timing.tsv";
            pitch_path += ".pitch.tsv";
            std::ofstream timing(timing_path), pitch(pitch_path);
            timing << "source\tword\tname\tframes\tsamples\tstress\tvowel\tpause\tstart_sample\n"
                   << std::setprecision(10);
            size_t offset = 0;
            for (const auto& sound : details.sounds) {
                timing << sound.source << '\t' << sound.word << '\t' << sound.name << '\t' << sound.frames
                       << '\t' << sound.samples << '\t' << sound.stress << '\t' << sound.vowel << '\t'
                       << int(sound.pause) << '\t' << offset << '\n';
                offset += sound.samples;
            }
            pitch << "seconds\thz\n" << std::setprecision(10);
            for (const auto& point : details.pitch)
                pitch << point.seconds << '\t' << point.hz << '\n';
            if (!timing || !pitch)
                throw std::runtime_error("Cannot write probe traces.");
        }
        std::cout << "samples=" << audio.size() << ", seconds=" << audio.size() / 24000.0 << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
