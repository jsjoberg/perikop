#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
void clean_kokoro_audio(std::vector<float>& pcm);
// The acoustic stage consumes token IDs supplied by the upstream front end.
// This class does not implement text normalization or pronunciation.
class KokoroAudio {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit KokoroAudio(const std::filesystem::path& pack);
    ~KokoroAudio();
    std::vector<float> generate(const std::vector<int64_t>& phones,const std::string& voice);
};
}
