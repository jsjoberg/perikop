#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
inline constexpr char kokoro_pack_id[] = "kokoro-sv-alice-bjorn-2c7968d-v1";
void clean_kokoro_audio(std::vector<float>& pcm);
// The acoustic stage consumes token IDs from KokoroText.
class KokoroAudio {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit KokoroAudio(const std::filesystem::path& pack);
    ~KokoroAudio();
    std::vector<float> generate(const std::vector<int64_t>& phones, const std::string& voice);
};
} // namespace ortho
