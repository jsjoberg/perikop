#include "speech/swedish_text.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ortho {
namespace {
std::u32string decode(const std::string& text) {
    std::u32string result;
    for (size_t i = 0; i < text.size();) {
        char32_t c = static_cast<unsigned char>(text[i++]);
        if (c >= 0x80) {
            const int extra = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : 3;
            c &= extra == 1 ? 0x1f : extra == 2 ? 0x0f : 0x07;
            for (int j = 0; j < extra && i < text.size(); ++j)
                c = (c << 6) | (static_cast<unsigned char>(text[i++]) & 0x3f);
        }
        result += c;
    }
    return result;
}
std::string encode(std::u32string_view text) {
    std::string result;
    for (char32_t c : text) {
        if (c < 0x80)
            result += char(c);
        else if (c < 0x800) {
            result += char(0xc0 | (c >> 6));
            result += char(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            result += char(0xe0 | (c >> 12));
            result += char(0x80 | ((c >> 6) & 0x3f));
            result += char(0x80 | (c & 0x3f));
        } else {
            result += char(0xf0 | (c >> 18));
            result += char(0x80 | ((c >> 12) & 0x3f));
            result += char(0x80 | ((c >> 6) & 0x3f));
            result += char(0x80 | (c & 0x3f));
        }
    }
    return result;
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    const auto name = path.filename().u8string();
    if (!input)
        throw std::runtime_error("Swedish pronunciation file is missing: " +
                                 std::string(reinterpret_cast<const char*>(name.data()), name.size()));
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
void replace_all(std::string& text, const std::string& from, const std::string& to) {
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

// --- Numbers -----------------------------------------------------------------
// The upstream spells digits as Swedish words before phonemization.
std::string cardinal(uint64_t value) {
    static const char* ones[] = {"noll",    "ett",    "två",    "tre",     "fyra",  "fem",   "sex",
                                 "sju",     "åtta",   "nio",    "tio",     "elva",  "tolv",  "tretton",
                                 "fjorton", "femton", "sexton", "sjutton", "arton", "nitton"};
    static const char* tens[] = {"",       "",       "tjugo",   "trettio", "fyrtio",
                                 "femtio", "sextio", "sjuttio", "åttio",   "nittio"};
    const auto rest = [](uint64_t part) {
        return part ? cardinal(part) : std::string();
    };
    if (value < 20)
        return ones[value];
    if (value < 100)
        return tens[value / 10] + rest(value % 10);
    if (value < 1000)
        return cardinal(value / 100) + "hundra" + rest(value % 100);
    if (value < 1000000) {
        auto result = cardinal(value / 1000) + "tusen";
        replace_all(result, "etttusen", "ettusen");
        return result + rest(value % 1000);
    }
    const auto large = [&](uint64_t unit, const char* one, const char* many) {
        const auto count = value / unit;
        return (count == 1 ? std::string(one) : cardinal(count) + " " + many) +
               (value % unit ? " " + cardinal(value % unit) : "");
    };
    if (value < 1000000000)
        return large(1000000, "en miljon", "miljoner");
    return large(1000000000, "en miljard", "miljarder");
}
std::string cardinal(std::string_view digits) {
    while (digits.size() > 1 && digits.front() == '0')
        digits.remove_prefix(1);
    // Longer digit strings, such as identifiers, read digit by digit.
    if (digits.size() > 12) {
        std::string result;
        for (char digit : digits)
            result += (result.empty() ? "" : " ") + cardinal(uint64_t(digit - '0'));
        return result;
    }
    uint64_t value = 0;
    for (char digit : digits)
        value = value * 10 + uint64_t(digit - '0');
    return cardinal(value);
}
bool sv_letter(char32_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == U'Å' || c == U'Ä' || c == U'Ö' ||
           c == U'å' || c == U'ä' || c == U'ö';
}
bool digit(char32_t c) {
    return c >= '0' && c <= '9';
}

// --- Lexicon -----------------------------------------------------------------
// lexicon.tsv: "word\tp h o n e s", byte-sorted with unique words. Binary
// search over line offsets keeps the file without a per-entry heap node.
class Lexicon {
    std::string data_;
    std::vector<uint32_t> lines_;
    std::unordered_map<std::string, std::string> custom_;
    std::string_view word(uint32_t line) const {
        const auto end = data_.find('\t', line);
        return std::string_view(data_).substr(line, end - line);
    }
    static std::string joined(std::string_view phones) {
        std::string result;
        for (char c : phones)
            if (c != ' ')
                result += c;
        return result;
    }

public:
    Lexicon(const std::filesystem::path& lexicon, const std::filesystem::path& custom)
        : data_(read_file(lexicon)) {
        for (size_t begin = 0; begin < data_.size();) {
            auto end = data_.find('\n', begin);
            if (end == std::string::npos)
                end = data_.size();
            const auto tab = data_.find('\t', begin);
            if (tab >= end || data_.find('\t', tab + 1) < end)
                throw std::runtime_error("Invalid Swedish lexicon row.");
            lines_.push_back(static_cast<uint32_t>(begin));
            begin = end + 1;
        }
        for (size_t i = 1; i < lines_.size(); ++i)
            if (!(word(lines_[i - 1]) < word(lines_[i])))
                throw std::runtime_error("Swedish lexicon is not sorted.");
        // Custom overrides win over the NST lexicon. Their words are lowercased.
        std::istringstream input(read_file(custom));
        for (std::string line; std::getline(input, line);) {
            if (line.empty() || line[0] == '#')
                continue;
            const auto tab = line.find('\t');
            if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos)
                throw std::runtime_error("Invalid Swedish custom lexicon row.");
            auto text = decode(line.substr(0, tab));
            for (auto& c : text)
                if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7))
                    c += 32;
            custom_[encode(text)] = joined(std::string_view(line).substr(tab + 1));
        }
    }
    // Empty when the word is unknown or its entry is empty, as `lex.get(w) or ...`.
    std::string find(const std::string& key) const {
        if (const auto entry = custom_.find(key); entry != custom_.end())
            return entry->second;
        const auto at = std::lower_bound(lines_.begin(), lines_.end(), key,
                                         [this](uint32_t line, const std::string& value) {
                                             return word(line) < value;
                                         });
        if (at == lines_.end() || word(*at) != key)
            return {};
        const auto begin = *at + key.size() + 1;
        auto end = data_.find('\n', begin);
        if (end == std::string::npos)
            end = data_.size();
        return joined(std::string_view(data_).substr(begin, end - begin));
    }
};

// Flat JSON objects of string keys and integer values, such as {"a": 3}.
} // namespace

std::string swedish_numbers(const std::string& text) {
    const auto source = decode(text);
    std::u32string spaced;
    for (size_t i = 0; i < source.size(); ++i) {
        if (i && ((digit(source[i]) && sv_letter(source[i - 1])) ||
                  (sv_letter(source[i]) && digit(source[i - 1]))))
            spaced += U' ';
        spaced += source[i];
    }
    std::string result;
    for (size_t i = 0; i < spaced.size();) {
        if (!digit(spaced[i])) {
            result += encode(spaced.substr(i, 1));
            ++i;
            continue;
        }
        auto end = i;
        while (end < spaced.size() && digit(spaced[end]))
            ++end;
        const auto whole = encode(spaced.substr(i, end - i));
        if (end + 1 < spaced.size() && (spaced[end] == ',' || spaced[end] == '.') && digit(spaced[end + 1])) {
            auto last = end + 1;
            while (last < spaced.size() && digit(spaced[last]))
                ++last;
            result += cardinal(whole) + " komma";
            for (auto at = end + 1; at < last; ++at)
                result += " " + cardinal(uint64_t(spaced[at] - '0'));
            i = last;
        } else {
            result += cardinal(whole);
            i = end;
        }
    }
    return result;
}

struct SwedishText::Impl {
    Lexicon lexicon;
    // Pronunciations of the corpus words the lexicon lacks, from the kokoro-sv neural G2P.
    std::unordered_map<std::string, std::string> corpus;
    explicit Impl(const std::filesystem::path& data)
        : lexicon(data / "lexicon.tsv", data / "custom_lexicon.tsv") {
        std::istringstream rows(read_file(data / "g2p-corpus.tsv"));
        for (std::string line; std::getline(rows, line);) {
            const auto tab = line.find('\t');
            if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos)
                throw std::runtime_error("Invalid corpus pronunciation row.");
            corpus[line.substr(0, tab)] = line.substr(tab + 1);
        }
    }
    // Other words keep their spelling as ⟨word⟩, for the voice's own rules.
    std::string unknown(const std::string& word) const {
        if (const auto found = corpus.find(word); found != corpus.end())
            return found->second;
        return "⟨" + word + "⟩";
    }
};

SwedishText::SwedishText(const std::filesystem::path& data) : impl_(std::make_unique<Impl>(data)) {}
SwedishText::~SwedishText() = default;

namespace {
std::string single_spaces(const std::string& text) {
    std::string result;
    for (char c : text)
        if (c != ' ' || (!result.empty() && result.back() != ' '))
            result += c;
    if (!result.empty() && result.back() == ' ')
        result.pop_back();
    return result;
}
} // namespace
std::string SwedishText::ipa(const std::string& input) {
    // g2p_infer.SwedishG2P.phonemize: sentences match [^.!?]+[.!?]?; tokens
    // are runs of the letters below or one of ,.!?;:- and all else is dropped.
    static const std::u32string word_letters =
        U"abcdefghijklmnopqrstuvwxyzåäöéèüáàâëïABCDEFGHIJKLMNOPQRSTUVWXYZÅÄÖ";
    static const std::u32string punctuation = U",.!?;:-";
    const auto is_letter = [](char32_t c) {
        return word_letters.find(c) != std::u32string::npos;
    };
    const auto end_mark = [](char32_t c) {
        return c == '.' || c == '!' || c == '?';
    };
    const auto text = decode(swedish_numbers(input));
    std::string result;
    for (size_t i = 0; i < text.size();) {
        if (end_mark(text[i])) {
            ++i;
            continue;
        }
        auto end = i;
        while (end < text.size() && !end_mark(text[end]))
            ++end;
        if (end < text.size())
            ++end;
        std::string sentence;
        bool previous_word = false;
        for (size_t at = i; at < end;) {
            // ⟦…⟧ holds one word's exact phonemes, from a pronunciation entry.
            if (text[at] == U'⟦') {
                const auto close = text.find(U'⟧', at);
                if (close == std::u32string::npos || close >= end) {
                    ++at;
                    continue;
                }
                std::u32string ipa;
                for (auto c : text.substr(at + 1, close - at - 1))
                    if (c != U' ')
                        ipa += c;
                if (previous_word)
                    sentence += ' ';
                sentence += encode(ipa);
                previous_word = true;
                at = close + 1;
            } else if (is_letter(text[at])) {
                auto last = at;
                while (last < end && is_letter(text[last]))
                    ++last;
                auto word = text.substr(at, last - at);
                const bool name = (word[0] >= 'A' && word[0] <= 'Z') || word[0] == U'Å' || word[0] == U'Ä' ||
                                  word[0] == U'Ö';
                for (auto& c : word)
                    if ((c >= 'A' && c <= 'Z') || c == U'Å' || c == U'Ä' || c == U'Ö')
                        c += 32;
                const auto key = encode(word);
                auto ipa = impl_->lexicon.find(key);
                // A name's genitive -s keeps the stem's pronunciation.
                if (ipa.empty() && name && key.size() > 2 && key.back() == 's')
                    if (auto stem = impl_->lexicon.find(key.substr(0, key.size() - 1)); !stem.empty())
                        ipa = stem + "s";
                if (ipa.empty())
                    ipa = impl_->unknown(key);
                if (previous_word)
                    sentence += ' ';
                sentence += ipa;
                previous_word = true;
                at = last;
            } else {
                if (punctuation.find(text[at]) != std::u32string::npos) {
                    sentence += char(text[at]);
                    previous_word = false;
                }
                ++at;
            }
        }
        if (!sentence.empty())
            result += (result.empty() ? "" : " ") + sentence;
        i = end;
    }
    // g2p_sv: NEURAL_FIXES, then single spaces.
    replace_all(result, "ˈuːɕˌɛj", "ˈuːkɛj");
    replace_all(result, "uːəsˈɛs", "ˈɔs");
    return single_spaces(result);
}
} // namespace ortho
