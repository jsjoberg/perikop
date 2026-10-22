#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace ortho {
struct PronunciationWord {
    std::string form, kind, sampa;
    int occurrences = 0;
};
std::vector<PronunciationWord> load_pronunciation_words(const std::filesystem::path&);
// A word that NST pronounces in more than one way: what the voice says without
// bundled corrections, and each alternative in the voice's phonemes with its parts of speech.
struct Homograph {
    struct Choice {
        std::string phonemes, label;
    };
    std::string voice;
    std::vector<Choice> choices;
};
// Keyed by the lowercase form.
std::map<std::string, Homograph> load_homographs(const std::filesystem::path&);
} // namespace ortho
