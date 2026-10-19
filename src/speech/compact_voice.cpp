#include "speech/compact_voice.hpp"
#include "speech/swedish_numbers.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
namespace ortho {
namespace {
constexpr size_t rate = 24000, guard = 240, maximum_samples = rate * 120;
constexpr uint16_t mono = 65535;
std::u32string decode(std::string_view text) {
    std::u32string out;
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 128) {
            out += first;
            continue;
        }
        const unsigned extra = first >= 0xc2 && first <= 0xdf   ? 1
                               : first <= 0xef && first >= 0xe0 ? 2
                               : first >= 0xf0 && first <= 0xf4 ? 3
                                                                : 0;
        if (!extra || i + extra > text.size())
            throw std::runtime_error("Ogiltig UTF-8 i uppläsningen.");
        char32_t value = first & ((1u << (6 - extra)) - 1);
        for (unsigned j = 0; j < extra; ++j) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xc0) != 0x80)
                throw std::runtime_error("Ogiltig UTF-8 i uppläsningen.");
            value = (value << 6) | (byte & 0x3f);
        }
        if (value < (extra == 1   ? 128u
                     : extra == 2 ? 2048u
                                  : 65536u) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::runtime_error("Ogiltig UTF-8 i uppläsningen.");
        out += value;
    }
    return out;
}
std::string encode(std::u32string_view text) {
    std::string out;
    for (auto c : text) {
        if (c < 128)
            out += char(c);
        else if (c < 2048) {
            out += char(0xc0 | (c >> 6));
            out += char(0x80 | (c & 63));
        } else if (c < 65536) {
            out += char(0xe0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        } else {
            out += char(0xf0 | (c >> 18));
            out += char(0x80 | ((c >> 12) & 63));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        }
    }
    return out;
}
bool letter(char32_t c) {
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
           std::u32string_view(U"åäöéèüáàâëïÅÄÖÉÈÜÁÀÂËÏ").find(c) != std::u32string_view::npos;
}
char32_t lower(char32_t c) {
    if ((c >= U'A' && c <= U'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7))
        return c + 32;
    return c;
}
std::string read(const std::filesystem::path& path, size_t limit) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Röstpaketet för Björn Bas saknas eller är ofullständigt.");
    const auto bytes = std::filesystem::file_size(path);
    if (bytes > limit)
        throw std::runtime_error("Röstfilen för Björn Bas är för stor.");
    std::string result(static_cast<size_t>(bytes), '\0');
    if (!stream.read(result.data(), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("Röstfilen för Björn Bas kunde inte läsas.");
    return result;
}
std::u32string remap(std::u32string text) {
    for (auto& c : text) {
        if (c == U'ʏ')
            c = U'y';
        if (c == U'ʉ')
            c = U'ɨ';
        if (c == U'ɵ')
            c = U'ɜ';
        if (c == U'ɧ')
            c = U'ʂ';
        if (c == U'ɭ')
            c = U'l';
    }
    return text;
}
bool voiced(std::u32string_view phone) {
    for (auto c : phone)
        if (std::u32string_view(U"aɑeɛəiɪoɔuʊyøœɨɜmnlrjvŋɳ").find(c) != std::u32string_view::npos)
            return true;
    return false;
}
void join(std::vector<float>& out, std::vector<float> input, bool align, size_t requested = 480) {
    if (input.empty())
        return;
    if (out.empty()) {
        out = std::move(input);
        return;
    }
    size_t overlap = std::min({requested, out.size(), input.size() / 2});
    size_t shift = 0;
    const auto align_phase = [&] {
        double best = -2;
        const auto search = std::min(guard, input.size() - overlap - 1);
        for (size_t at = 0; at <= search; at += 2) {
            double dot = 0, a = 1e-12, b = 1e-12;
            for (size_t i = 0; i < overlap; i += 2) {
                const double x = out[out.size() - overlap + i], y = input[at + i];
                dot += x * y;
                a += x * x;
                b += y * y;
            }
            const double score = dot / std::sqrt(a * b) - 0.015 * double(at) / guard;
            if (score > best) {
                best = score;
                shift = at;
            }
        }
        return best;
    };
    if (align && overlap > 64 && align_phase() < 0.85 && overlap > 120) {
        // A long overlap between different pitches produces beating even when
        // their initial phases match. Keep these transitions to five milliseconds.
        overlap = 120;
        shift = 0;
        align_phase();
    }
    for (size_t i = 0; i < overlap; ++i) {
        const float t = float(i + 1) / float(overlap + 1);
        out[out.size() - overlap + i] = out[out.size() - overlap + i] * (1 - t) + input[shift + i] * t;
    }
    out.insert(out.end(), input.begin() + static_cast<ptrdiff_t>(shift + overlap), input.end());
}
} // namespace
std::string compact_revision(const std::filesystem::path& pack) {
    auto text = read(pack / "revision.txt", 128);
    if (!text.empty() && text.back() == '\n')
        text.pop_back();
    if (text.size() != 64 || text.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Ogiltig version av Björn Bas.");
    return std::string(compact_pack_id) + "/" + text;
}
struct CompactVoice::Impl {
    struct Unit {
        uint32_t offset, count;
        uint16_t lead, tail;
    };
    std::string bank;
    std::unordered_map<uint32_t, Unit> units;
    std::map<std::u32string, uint16_t> ids;
    std::vector<std::u32string> labels;
    std::unordered_map<std::string, std::string> lexicon;
    std::array<float, 256> amplitudes{};
    explicit Impl(const std::filesystem::path& pack) {
        compact_revision(pack);
        for (size_t i = 0; i < amplitudes.size(); ++i) {
            const double value = double(i) / 127.5 - 1;
            amplitudes[i] = float(std::copysign(std::expm1(std::abs(value) * std::log(256)) / 255, value));
        }
        std::istringstream inventory(read(pack / "phones.tsv", 65536));
        for (std::string line; std::getline(inventory, line);) {
            const auto first = line.find('\t'), last = line.rfind('\t');
            if (first == std::string::npos || first == last)
                throw std::runtime_error("Ogiltigt ljudregister för Björn Bas.");
            const auto id = std::stoul(line.substr(0, first));
            const auto label = decode(std::string_view(line).substr(first + 1, last - first - 1));
            if (id != labels.size() || id >= 512 || label.empty() || ids.contains(label))
                throw std::runtime_error("Ogiltigt ljudregister för Björn Bas.");
            ids[label] = static_cast<uint16_t>(id);
            labels.push_back(label);
        }
        bank = read(pack / "units.bin", size_t{32} * 1024 * 1024);
        size_t cursor = 0;
        const auto integer = [&](size_t count) {
            if (count > bank.size() - cursor)
                throw std::runtime_error("Ofullständig ljudbank för Björn Bas.");
            uint32_t value = 0;
            for (size_t i = 0; i < count; ++i)
                value |= uint32_t(static_cast<unsigned char>(bank[cursor++])) << (i * 8);
            return value;
        };
        if (integer(4) != 0x31554a42 || integer(4) != rate || integer(4) != labels.size())
            throw std::runtime_error("Ogiltig ljudbank för Björn Bas.");
        const auto count = integer(4);
        if (!count || count > 32768)
            throw std::runtime_error("Ogiltigt antal ljudenheter.");
        for (size_t i = 0; i < count; ++i) {
            const auto left = integer(2), right = integer(2), samples = integer(4);
            const auto lead = integer(2), tail = integer(2);
            if ((left != mono && left >= labels.size()) || right >= labels.size() || samples <= lead + tail ||
                samples > rate * 2 || samples > bank.size() - cursor || units.contains((left << 16) | right))
                throw std::runtime_error("Ogiltig ljudenhet för Björn Bas.");
            units[(left << 16) | right] = {static_cast<uint32_t>(cursor), samples,
                                           static_cast<uint16_t>(lead), static_cast<uint16_t>(tail)};
            cursor += samples;
        }
        if (cursor != bank.size())
            throw std::runtime_error("Ogiltig längd på ljudbanken.");
        for (size_t id = 0; id < labels.size(); ++id)
            if (!units.contains((uint32_t(mono) << 16) | id))
                throw std::runtime_error("Röstpaketet saknar grundljud.");
        std::istringstream dictionary(read(pack / "lexicon.tsv", size_t{8} * 1024 * 1024));
        for (std::string line; std::getline(dictionary, line);) {
            const auto tab = line.find('\t');
            if (tab == std::string::npos || !tab || tab + 1 >= line.size())
                throw std::runtime_error("Ogiltig uttalsordlista för Björn Bas.");
            lexicon[line.substr(0, tab)] = line.substr(tab + 1);
        }
    }
    std::string word(std::u32string value, bool* spelled) const {
        const bool name = !value.empty() && value.front() != lower(value.front());
        for (auto& c : value)
            c = lower(c);
        const auto key = encode(value);
        if (const auto found = lexicon.find(key); found != lexicon.end())
            return found->second;
        if (name && key.size() > 1 && key.back() == 's')
            if (const auto found = lexicon.find(key.substr(0, key.size() - 1)); found != lexicon.end())
                return found->second + "s";
        // Out-of-corpus words are spelled rather than silently dropped or sent to a model.
        if (spelled)
            *spelled = true;
        static constexpr std::u32string_view letters = U"abcdefghijklmnopqrstuvwxyzåäö";
        static const char* const names[] = {"ɑː",  "beː",   "seː", "deː", "eː", "ɛf",  "ɡeː",      "hoː",
                                            "iː",  "jiː",   "koː", "ɛl",  "ɛm", "ɛn",  "uː",       "peː",
                                            "kʉː", "ɛr",    "ɛs",  "teː", "ʉː", "veː", "dʊbəlveː", "ɛks",
                                            "yː",  "sɛːta", "oː",  "ɛː",  "øː"};
        std::string result;
        for (auto c : value) {
            if (c == U'é' || c == U'è' || c == U'ë')
                c = U'e';
            if (c == U'ü')
                c = U'y';
            if (c == U'á' || c == U'à' || c == U'â')
                c = U'a';
            if (c == U'ï')
                c = U'i';
            const auto at = letters.find(c);
            if (at == std::u32string_view::npos)
                throw std::runtime_error("Det saknas uttal för ett tecken i texten.");
            result += names[at];
        }
        return result;
    }
    std::vector<uint16_t> phone_ids(const std::string& ipa) const {
        std::vector<std::u32string> keys;
        std::u32string stress;
        for (auto c : remap(decode(ipa))) {
            if (c == U'ˈ' || c == U'ˌ')
                stress = c;
            else if (c == U'ː' && !keys.empty())
                keys.back() += c;
            else if (c != U' ' && c != U'-' && c != 0x303) {
                keys.push_back(stress + c);
                stress.clear();
            }
        }
        std::vector<uint16_t> out;
        for (const auto& key : keys) {
            auto found = ids.find(key);
            if (found == ids.end()) {
                // An explicit override can ask for an untrained stress/length variant.
                auto base = key;
                std::erase(base, U'ˈ');
                std::erase(base, U'ˌ');
                std::erase(base, U'ː');
                found = ids.find(base);
                if (found == ids.end())
                    found = std::find_if(ids.begin(), ids.end(), [&](const auto& entry) {
                        auto value = entry.first;
                        std::erase(value, U'ˈ');
                        std::erase(value, U'ˌ');
                        std::erase(value, U'ː');
                        return value == base;
                    });
            }
            if (found == ids.end())
                throw std::runtime_error("Björn Bas saknar ett ljud i uttalet.");
            out.push_back(found->second);
        }
        return out;
    }
    std::vector<float> waveform(const Unit& unit, size_t begin = 0, size_t end = 0) const {
        if (!end)
            end = unit.count;
        std::vector<float> samples;
        samples.reserve(end - begin);
        for (size_t i = begin; i < end; ++i)
            samples.push_back(amplitudes[static_cast<unsigned char>(bank[unit.offset + i])]);
        return samples;
    }
    const Unit& monophone(uint16_t id) const {
        return units.at((uint32_t(mono) << 16) | id);
    }
    std::vector<float> half(uint16_t id, bool first) const {
        const auto& unit = monophone(id);
        const size_t center = (unit.lead + unit.count - unit.tail) / 2;
        return first ? waveform(unit, unit.lead, std::min<size_t>(unit.count, center + guard))
                     : waveform(unit, center > guard ? center - guard : 0, unit.count - unit.tail);
    }
    std::vector<float> render(const std::vector<uint16_t>& plan, const std::function<bool()>& cancelled,
                              CompactVoiceStats& stats) const {
        if (plan.empty())
            return {};
        auto result = half(plan.front(), true);
        for (size_t i = 1; i < plan.size(); ++i) {
            if (cancelled && cancelled())
                throw std::runtime_error("Speech cancelled");
            const auto found = units.find((uint32_t(plan[i - 1]) << 16) | plan[i]);
            std::vector<float> part;
            if (found != units.end()) {
                part = waveform(found->second);
                ++stats.diphones;
            } else {
                part = half(plan[i - 1], false);
                join(part, half(plan[i], true), false, 72);
                ++stats.backoffs;
            }
            join(result, std::move(part), voiced(labels[plan[i - 1]]));
            if (result.size() > maximum_samples)
                throw std::runtime_error("Texten är för lång för ett ljudstycke.");
        }
        join(result, half(plan.back(), false), voiced(labels[plan.back()]));
        const auto fade = std::min<size_t>(120, result.size() / 2);
        for (size_t i = 0; i < fade; ++i) {
            result[i] *= float(i) / float(fade);
            result[result.size() - 1 - i] *= float(i) / float(fade);
        }
        return result;
    }
    template <class Word, class Pause>
    void parse(const std::string& input, Word&& on_word, Pause&& on_pause) const {
        decode(input); // Validate before the shared number normalizer handles UTF-8.
        const auto text = decode(swedish_numbers(input));
        for (size_t i = 0; i < text.size();) {
            if (text[i] == U'⟦') {
                const auto end = text.find(U'⟧', i + 1);
                if (end == std::u32string::npos)
                    throw std::runtime_error("Ofullständigt fonetiskt uttal.");
                on_word(encode(std::u32string_view(text).substr(i + 1, end - i - 1)), false);
                i = end + 1;
            } else if (letter(text[i])) {
                const auto begin = i++;
                while (i < text.size() && letter(text[i]))
                    ++i;
                bool spelled = false;
                auto ipa = word(text.substr(begin, i - begin), &spelled);
                on_word(ipa, spelled);
            } else {
                const auto c = text[i++];
                if (c == U'.' || c == U'!' || c == U'?')
                    on_pause(320);
                else if (c == U',' || c == U';' || c == U':')
                    on_pause(c == U',' ? 140 : 220);
            }
        }
    }
};
CompactVoice::CompactVoice(const std::filesystem::path& pack) : impl_(std::make_unique<Impl>(pack)) {}
CompactVoice::~CompactVoice() = default;
std::string CompactVoice::ipa(const std::string& text) const {
    std::string result;
    impl_->parse(
        text,
        [&](const std::string& ipa, bool) {
            if (!result.empty() && result.back() != ' ')
                result += ' ';
            result += ipa;
        },
        [&](int) {
            result += ", ";
        });
    return result;
}
std::vector<float> CompactVoice::generate(const std::string& text, const std::function<bool()>& cancelled,
                                          CompactVoiceStats* statistics) {
    if (cancelled && cancelled())
        throw std::runtime_error("Speech cancelled");
    if (text.size() > 65536)
        throw std::runtime_error("Texten är för lång för ett ljudstycke.");
    CompactVoiceStats stats;
    std::vector<float> output;
    std::vector<uint16_t> plan;
    const auto flush = [&] {
        auto pcm = impl_->render(plan, cancelled, stats);
        output.insert(output.end(), pcm.begin(), pcm.end());
        plan.clear();
        if (output.size() > maximum_samples)
            throw std::runtime_error("Texten är för lång för ett ljudstycke.");
    };
    impl_->parse(
        text,
        [&](const std::string& ipa, bool spelled) {
            ++stats.words;
            stats.spelled_words += spelled;
            const auto ids = impl_->phone_ids(ipa);
            plan.insert(plan.end(), ids.begin(), ids.end());
        },
        [&](int milliseconds) {
            flush();
            output.insert(output.end(), rate * static_cast<size_t>(milliseconds) / 1000, 0.0f);
        });
    flush();
    if (output.empty())
        output.resize(2400);
    if (statistics)
        *statistics = stats;
    return output;
}
} // namespace ortho
