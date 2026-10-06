#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace ortho {
struct PronunciationWord { std::string form,kind,sampa; int occurrences=0; };
std::vector<PronunciationWord> load_pronunciation_words(const std::filesystem::path&);
}
