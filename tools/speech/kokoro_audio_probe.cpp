#include "speech/kokoro_audio.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
std::filesystem::path path(const char* text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text)));
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--filter") {
            std::ifstream input(path(argv[2]), std::ios::binary);
            const auto size = std::filesystem::file_size(path(argv[2]));
            if (size % sizeof(float) || size > 24000 * 120 * sizeof(float))
                throw std::runtime_error("Invalid filter input.");
            std::vector<float> pcm(size / sizeof(float));
            input.read(reinterpret_cast<char*>(pcm.data()), size);
            if (!input)
                throw std::runtime_error("Cannot read filter input.");
            ortho::clean_kokoro_audio(pcm);
            std::ofstream output(path(argv[3]), std::ios::binary);
            output.write(reinterpret_cast<const char*>(pcm.data()), size);
            if (!output)
                throw std::runtime_error("Cannot write filter output.");
            return 0;
        }
        if (argc != 5)
            throw std::runtime_error("Usage: kokoro-audio-probe PACK TOKEN-FILE alice|bjorn OUTPUT-FLOAT32");
        std::ifstream source(path(argv[2]));
        if (!source)
            throw std::runtime_error("Cannot read input tokens.");
        std::vector<int64_t> tokens;
        int64_t id;
        while (source >> id)
            tokens.push_back(id);
        const auto start = std::chrono::steady_clock::now();
        ortho::KokoroAudio model(path(argv[1]));
        const auto loaded = std::chrono::steady_clock::now();
        const auto pcm = model.generate(tokens, argv[3]);
        const auto finished = std::chrono::steady_clock::now();
        std::ofstream output(path(argv[4]), std::ios::binary);
        output.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(float));
        if (!output)
            throw std::runtime_error("Cannot write probe audio.");
        std::cout << "load_seconds=" << std::chrono::duration<double>(loaded - start).count()
                  << ", generation_seconds=" << std::chrono::duration<double>(finished - loaded).count()
                  << ", audio_seconds=" << pcm.size() / 24000.0 << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
