#pragma once
#include <filesystem>
#include <memory>
#include <string>
namespace ortho {
// Swedish pronunciation front end, from the pinned kokoro-sv text path (g2p_sv.SwedishG2P with
// the nst_g2p backend): digits spelled as Swedish words, the NST lexicon with custom overrides,
// precomputed pronunciations for the corpus words the lexicon lacks, and its word fixes.
// Two additions: "⟦phonemes⟧" in the text gives one word's exact phonemes, and a capitalized
// name's genitive -s reuses the lexicon stem. A word found in none of these stays as ⟨word⟩.
std::string swedish_numbers(const std::string& text);
class SwedishText {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    // `data` holds lexicon.tsv, custom_lexicon.tsv and g2p-corpus.tsv.
    explicit SwedishText(const std::filesystem::path& data);
    ~SwedishText();
    // IPA in the NST symbols of the lexicon.
    std::string ipa(const std::string& text);
};
} // namespace ortho
