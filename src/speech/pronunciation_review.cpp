#include "speech/pronunciation_review.hpp"
#include <algorithm>
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
std::map<std::string, Homograph> load_homographs(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Cannot open prepared homograph list");
    std::map<std::string, Homograph> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const auto a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos)
            throw std::runtime_error("Invalid homograph list row");
        Homograph word{line.substr(b + 1, c - b - 1), {}};
        for (auto begin = c + 1; begin <= line.size();) {
            const auto end = std::min(line.find('|', begin), line.size());
            const auto choice = line.substr(begin, end - begin);
            const auto equals = choice.find('=');
            if (equals == std::string::npos || equals == 0)
                throw std::runtime_error("Invalid homograph alternative");
            word.choices.push_back({choice.substr(0, equals), choice.substr(equals + 1)});
            begin = end + 1;
        }
        if (word.choices.size() < 2)
            throw std::runtime_error("A homograph needs two alternatives");
        result.emplace(line.substr(0, a), std::move(word));
    }
    if (input.bad() || result.empty())
        throw std::runtime_error("Empty or unreadable homograph list");
    return result;
}
} // namespace ortho
