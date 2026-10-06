#include "speech/pronunciation_review.hpp"
#include <charconv>
#include <fstream>
#include <stdexcept>
namespace ortho {
std::vector<PronunciationWord> load_pronunciation_words(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Cannot open prepared pronunciation word list");
    std::vector<PronunciationWord> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const auto a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos ||
            line.find('\t', c + 1) != std::string::npos)
            throw std::runtime_error("Invalid pronunciation word list row");
        PronunciationWord word{line.substr(0, a), line.substr(b + 1, c - b - 1), line.substr(c + 1), 0};
        auto count = std::from_chars(line.data() + a + 1, line.data() + b, word.occurrences);
        if (count.ec != std::errc{} || count.ptr != line.data() + b || word.occurrences < 1 ||
            word.form.empty() || (word.kind != "name" && word.kind != "word"))
            throw std::runtime_error("Invalid pronunciation word list value");
        result.push_back(std::move(word));
    }
    if (input.bad() || result.empty())
        throw std::runtime_error("Empty or unreadable pronunciation word list");
    return result;
}
} // namespace ortho
