#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
// Swedish front end for the Kokoro voices. Reproduces the pinned upstream
// kokoro-sv text path (g2p_sv.SwedishG2P with the nst_g2p backend): digits
// spelled as Swedish words, NST lexicon with custom overrides, neural fallback
// for unknown words, word fixes and the Kokoro symbol remap. The pack keeps the
// lexicon entries and neural results for the corpus words only; other words
// load the neural model. Two additions:
// "⟦phonemes⟧" in the text gives one word's exact phonemes, and a capitalized
// name's genitive -s reuses the lexicon stem.
std::string swedish_numbers(const std::string& text);
class KokoroText {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit KokoroText(const std::filesystem::path& pack);
    ~KokoroText();
    // IPA in the NST symbols of the lexicon, before the Kokoro remap.
    std::string ipa(const std::string& text);
    // IPA string in Kokoro's symbol inventory.
    std::string phonemes(const std::string& text);
    // Kokoro token IDs for the phoneme string; unknown symbols are dropped.
    std::vector<int64_t> tokens(const std::string& text);
};
} // namespace ortho
