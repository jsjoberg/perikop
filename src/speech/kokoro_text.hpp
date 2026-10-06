#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
// Swedish front end for the Kokoro voices. Reproduces the pinned upstream
// kokoro-sv text path (g2p_sv.SwedishG2P with the nst_g2p backend):
// number spelling, NST lexicon with custom overrides, neural fallback for
// unknown words, word fixes and the Kokoro symbol remap.
std::string swedish_numbers(const std::string& text);
class KokoroText {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit KokoroText(const std::filesystem::path& pack);
    ~KokoroText();
    // IPA string in Kokoro's symbol inventory.
    std::string phonemes(const std::string& text);
    // Kokoro token IDs for the phoneme string; unknown symbols are dropped.
    std::vector<int64_t> tokens(const std::string& text);
};
}
