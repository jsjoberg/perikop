#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
// Swedish pronunciation front end, derived from the pinned kokoro-sv text path (g2p_sv.SwedishG2P
// with the nst_g2p backend): digits spelled as Swedish words, the NST lexicon,
// and precomputed pronunciations for the corpus words the lexicon lacks, all stored as eSpeak NG
// Swedish phoneme codes. Additions: "⟦codes⟧" in the text gives one word's exact phonemes, a
// capitalized name's genitive -s reuses the lexicon stem, and common function words are unstressed.
std::string swedish_numbers(const std::string& text);
// One spoken unit, in reading order.
struct SpeechToken {
    enum class Kind {
        Phonemes,    // one word's eSpeak NG phoneme codes
        Spelled,     // a word found nowhere, for eSpeak's own Swedish rules
        Punctuation, // one of , . ! ? ; :
    };
    Kind kind;
    std::string text;
    bool operator==(const SpeechToken&) const = default;
};
class SwedishText {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    // `data` holds lexicon.tsv and g2p-corpus.tsv.
    explicit SwedishText(const std::filesystem::path& data);
    ~SwedishText();
    std::vector<SpeechToken> tokens(const std::string& text);
    // The tokens on one line for people: codes per word, ⟨spelled⟩ words, punctuation attached.
    std::string phonemes(const std::string& text);
};
} // namespace ortho
