#include "speech/speech.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ortho {
namespace {
struct Token {
    std::size_t begin, end;
    std::string folded;
};
unsigned decode(const std::string& text, std::size_t& i) {
    unsigned c = static_cast<unsigned char>(text[i++]);
    if (c < 128)
        return c;
    const int extra = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : 3;
    c &= extra == 1 ? 31 : extra == 2 ? 15 : 7;
    for (int j = 0; j < extra && i < text.size(); ++j)
        c = (c << 6) | (static_cast<unsigned char>(text[i++]) & 63);
    return c;
}
bool letter(unsigned c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           (c >= 0xc0 && c <= 0x2af && c != 0xd7 && c != 0xf7) || (c >= 0x300 && c <= 0x52f) ||
           (c >= 0x1e00 && c <= 0x1fff);
}
std::string folded(const std::string& text) {
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        unsigned c = decode(text, i);
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if ((c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xde))
            c += 32;
        if (c < 128)
            result += char(c);
        else if (c < 2048) {
            result += char(0xc0 | (c >> 6));
            result += char(0x80 | (c & 63));
        } else {
            result += char(0xe0 | (c >> 12));
            result += char(0x80 | ((c >> 6) & 63));
            result += char(0x80 | (c & 63));
        }
    }
    return result;
}
std::vector<Token> tokens(const std::string& text) {
    std::vector<Token> result;
    for (std::size_t i = 0; i < text.size();) {
        const auto begin = i;
        if (!letter(decode(text, i)))
            continue;
        while (i < text.size()) {
            auto next = i;
            if (!letter(decode(text, next)))
                break;
            i = next;
        }
        result.push_back({begin, i, folded(text.substr(begin, i - begin))});
    }
    return result;
}
bool only_space(const std::string& text, std::size_t first, std::size_t last) {
    return std::all_of(text.begin() + first, text.begin() + last, [](unsigned char c) {
        return std::isspace(c);
    });
}
} // namespace
std::string pronunciation_key(const std::string& text) {
    return folded(text);
}
bool contains_speech_word(const std::string& text, const std::string& word) {
    const auto key = folded(word);
    for (const auto& token : tokens(text))
        if (token.folded == key)
            return true;
    return false;
}
struct PronunciationMatcher::Impl {
    std::string language;
    std::vector<Pronunciation> ordered;
    struct Candidate {
        std::size_t entry;
        std::vector<Token> phrase;
    };
    std::unordered_map<std::string, std::vector<Candidate>> candidates;
    Impl(std::string language, std::vector<Pronunciation> lexicon)
        : language(std::move(language)), ordered(std::move(lexicon)) {
        std::stable_sort(ordered.begin(), ordered.end(), [](auto& a, auto& b) {
            return a.priority != b.priority ? a.priority > b.priority : a.source.size() > b.source.size();
        });
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            const auto& entry = ordered[i];
            if (entry.language != this->language || (entry.spoken.empty() && entry.phonemes.empty()))
                continue;
            auto phrase = tokens(entry.source);
            if (!phrase.empty()) {
                const auto first = phrase.front().folded;
                candidates[first].push_back({i, std::move(phrase)});
            }
        }
    }
};
PronunciationMatcher::PronunciationMatcher(std::string language, std::vector<Pronunciation> lexicon)
    : impl_(std::make_shared<Impl>(std::move(language), std::move(lexicon))) {}
SpeechUtterance PronunciationMatcher::utterance(const std::string& text) const {
    const auto words = tokens(text);
    std::string result;
    std::size_t copied = 0;
    for (std::size_t i = 0; i < words.size();) {
        std::size_t count = 0;
        std::string replacement;
        const auto bucket = impl_->candidates.find(words[i].folded);
        if (bucket != impl_->candidates.end())
            for (const auto& candidate : bucket->second) {
                const auto& phrase = candidate.phrase;
                if (i + phrase.size() > words.size())
                    continue;
                bool match = true;
                for (std::size_t j = 0; j < phrase.size(); ++j) {
                    if (phrase[j].folded != words[i + j].folded ||
                        (j && !only_space(text, words[i + j - 1].end, words[i + j].begin))) {
                        match = false;
                        break;
                    }
                }
                // Phonemes go to the Swedish front end as one ⟦…⟧ word.
                if (match) {
                    const auto& entry = impl_->ordered[candidate.entry];
                    count = phrase.size();
                    replacement = entry.phonemes.empty() ? entry.spoken : "⟦" + entry.phonemes + "⟧";
                    break;
                }
            }
        if (!count) {
            ++i;
            continue;
        }
        result += text.substr(copied, words[i].begin - copied);
        result += replacement;
        copied = words[i + count - 1].end;
        i += count;
    }
    result += text.substr(copied);
    return {text, result, impl_->language, {}};
}
SpeechUtterance make_utterance(const std::string& text, const std::string& language,
                               const std::vector<Pronunciation>& lexicon) {
    return PronunciationMatcher(language, lexicon).utterance(text);
}
namespace {
// The byte ranges of closed ⟦…⟧ phoneme overrides. eSpeak codes use . and ; themselves.
std::vector<std::pair<size_t, size_t>> overrides(const std::string& text) {
    std::vector<std::pair<size_t, size_t>> result;
    for (auto open = text.find("⟦"); open != std::string::npos; open = text.find("⟦", open + 1)) {
        const auto close = text.find("⟧", open);
        if (close == std::string::npos)
            break;
        result.emplace_back(open, close + std::string_view("⟧").size());
        open = close;
    }
    return result;
}
} // namespace
std::vector<std::string> speech_chunks(const std::string& text) {
    const auto space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    // A chunk never ends inside an override, and its punctuation ends no sentence.
    const auto ranges = overrides(text);
    const auto inside = [&](size_t at) {
        return std::ranges::any_of(ranges, [at](const auto& range) {
            return range.first < at && at < range.second;
        });
    };
    const auto first_of = [&](const char* set, size_t from) {
        auto at = text.find_first_of(set, from);
        while (at != std::string::npos && inside(at))
            at = text.find_first_of(set, at + 1);
        return at;
    };
    const auto last_of = [&](const char* set, size_t from) {
        auto at = text.find_last_of(set, from);
        while (at != std::string::npos && inside(at))
            at = text.find_last_of(set, at - 1);
        return at;
    };
    std::vector<std::string> result;
    size_t begin = 0;
    while (begin < text.size()) {
        while (begin < text.size() && space(text[begin]))
            ++begin;
        if (begin == text.size())
            break;
        auto end = std::min(begin + 240, text.size());
        if (end < text.size()) {
            // Finish a nearby sentence instead of leaving its last words in a
            // separate synthesis request, which adds a pause inside the phrase.
            const auto limit = std::min(begin + 320, text.size());
            const auto next_sentence = first_of(".!?;", end);
            if (next_sentence != std::string::npos && next_sentence < limit)
                end = next_sentence + 1;
            else if (text.size() <= limit)
                end = text.size();
            else {
                auto split = last_of(" \t\n\r\v\f", end);
                if (split != std::string::npos && split > begin)
                    end = split;
                else {
                    end = first_of(" \t\n\r\v\f", end);
                    if (end == std::string::npos)
                        end = text.size();
                }
                const auto sentence = last_of(".!?;", end - 1);
                if (sentence != std::string::npos && sentence >= begin + 40)
                    end = sentence + 1;
            }
        }
        result.push_back(text.substr(begin, end - begin));
        begin = end;
    }
    return result;
}
double speech_text_weight(const std::string& text) {
    double weight = 0;
    bool phonemes = false;
    for (size_t i = 0; i < text.size();) {
        const auto c = decode(text, i);
        if (c <= 127 && std::isspace(static_cast<unsigned char>(c)))
            continue;
        // Phoneme codes inside ⟦…⟧ use punctuation marks as letters.
        if (c == U'⟦' || c == U'⟧') {
            phonemes = c == U'⟦';
            weight += 1;
        } else if (phonemes)
            weight += 1;
        else
            weight += c == '.' || c == '!' || c == '?' ? 4 : c == ',' || c == ';' || c == ':' ? 2 : 1;
    }
    return weight;
}
} // namespace ortho
