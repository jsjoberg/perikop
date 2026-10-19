#include "speech/swedish_numbers.hpp"
#include <cstdint>
#include <string_view>
namespace ortho {
namespace {
void replace_all(std::string& text, const std::string& from, const std::string& to) {
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}
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

} // namespace ortho
