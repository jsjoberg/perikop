#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
inline constexpr char compact_pack_id[] = "bjorn-lite-v1";
std::string compact_revision(const std::filesystem::path& pack);
struct CompactVoiceStats {
    size_t words = 0, spelled_words = 0, diphones = 0, backoffs = 0;
};
// Reusable Björn waveform units and a Swedish lexicon. No inference runtime.
class CompactVoice {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit CompactVoice(const std::filesystem::path& pack);
    ~CompactVoice();
    std::vector<float> generate(const std::string& text, const std::function<bool()>& cancelled = {},
                                CompactVoiceStats* stats = nullptr);
    std::string ipa(const std::string& text) const;
};
} // namespace ortho
