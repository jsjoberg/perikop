#include "speech/formant_voice.hpp"
#include <algorithm>
#include <cstring>
#define SPEECH_IMPLEMENTATION
#include <speech.h>
namespace ortho {
namespace {
// speech.h runs the Klatt synthesizer at 11025 Hz; every filter takes the rate as a parameter.
constexpr int sample_rate = 24000;
enum Length { long_vowel, short_vowel, schwa };
struct Vowel {
    const char* ipa;
    int slot, donor, f1, f2, f3;
    Length length;
};
// Swedish vowel qualities (male, approximate formants in Hz), written into speech.h's
// English vowel elements. Every vowel element has the same rank and vowel feature.
constexpr Vowel vowels[] = {
    {"iː", TTS_ELM_EE, TTS_ELM_EE, 255, 2190, 3150, long_vowel},
    {"i", TTS_ELM_EE, TTS_ELM_EE, 255, 2190, 3150, short_vowel},
    {"ɪ", TTS_ELM_I, TTS_ELM_I, 340, 2000, 2650, short_vowel},
    {"yː", TTS_ELM_AI, TTS_ELM_EE, 260, 1950, 2400, long_vowel},
    {"y", TTS_ELM_AI, TTS_ELM_EE, 260, 1950, 2400, short_vowel},
    {"ʏ", TTS_ELM_IE, TTS_ELM_I, 330, 1800, 2300, short_vowel},
    {"ʉː", TTS_ELM_OI, TTS_ELM_EE, 290, 1650, 2150, long_vowel},
    {"ɨː", TTS_ELM_OI, TTS_ELM_EE, 290, 1650, 2150, long_vowel}, // letter names in the custom lexicon
    {"ɵ", TTS_ELM_OU, TTS_ELM_OO, 390, 1300, 2250, short_vowel},
    {"eː", TTS_ELM_OV, TTS_ELM_EE, 345, 2200, 2800, long_vowel},
    {"e", TTS_ELM_E, TTS_ELM_E, 480, 1800, 2500, short_vowel},
    {"ɛː", TTS_ELM_OA, TTS_ELM_E, 490, 1900, 2550, long_vowel},
    {"ɛ", TTS_ELM_IA, TTS_ELM_E, 500, 1750, 2500, short_vowel},
    {"øː", TTS_ELM_IB, TTS_ELM_EE, 360, 1650, 2200, long_vowel},
    {"ø", TTS_ELM_IB, TTS_ELM_EE, 360, 1650, 2200, short_vowel},
    {"œ", TTS_ELM_AIR, TTS_ELM_E, 520, 1350, 2300, short_vowel},
    {"œː", TTS_ELM_AIR, TTS_ELM_E, 520, 1350, 2300, long_vowel},
    {"æ", TTS_ELM_AA, TTS_ELM_AA, 650, 1650, 2450, short_vowel},
    {"æː", TTS_ELM_AA, TTS_ELM_AA, 650, 1650, 2450, long_vowel},
    {"a", TTS_ELM_U, TTS_ELM_AA, 650, 1350, 2450, short_vowel},
    {"ɑː", TTS_ELM_AR, TTS_ELM_AR, 600, 950, 2550, long_vowel},
    {"ɑ", TTS_ELM_AR, TTS_ELM_AR, 600, 950, 2550, short_vowel},
    {"oː", TTS_ELM_AW, TTS_ELM_UU, 390, 720, 2400, long_vowel},
    {"o", TTS_ELM_AW, TTS_ELM_UU, 390, 720, 2400, short_vowel},
    {"ɔ", TTS_ELM_O, TTS_ELM_O, 450, 800, 2450, short_vowel},
    {"uː", TTS_ELM_UU, TTS_ELM_UU, 300, 620, 2250, long_vowel},
    {"u", TTS_ELM_UU, TTS_ELM_UU, 300, 620, 2250, short_vowel},
    {"ʊ", TTS_ELM_OO, TTS_ELM_OO, 360, 750, 2300, short_vowel},
    {"ə", TTS_ELM_A, TTS_ELM_A, 450, 1400, 2500, schwa},
};
// An element to play, and the element whose duration it takes.
struct Part {
    int slot, timing = -1;
};
struct Consonant {
    const char* ipa;
    Part parts[3];
    int count = 1;
    int frames = 0; // 0: the timing elements' own durations
};
// Swedish-only sounds use English-only elements of similar rank; tune() fills in their values.
constexpr Consonant consonants[] = {
    {"p", {{TTS_ELM_P}, {TTS_ELM_PY}, {TTS_ELM_PZ}}, 3},
    {"t", {{TTS_ELM_T}, {TTS_ELM_TY}, {TTS_ELM_TZ}}, 3},
    {"k", {{TTS_ELM_K}, {TTS_ELM_KY}, {TTS_ELM_KZ}}, 3},
    {"b", {{TTS_ELM_B}, {TTS_ELM_BY}, {TTS_ELM_BZ}}, 3},
    {"d", {{TTS_ELM_D}, {TTS_ELM_DY}, {TTS_ELM_DZ}}, 3},
    {"ɡ", {{TTS_ELM_G}, {TTS_ELM_GY}, {TTS_ELM_GZ}}, 3},
    {"ʈ", {{TTS_ELM_CH, TTS_ELM_T}, {TTS_ELM_TY}, {TTS_ELM_Z, TTS_ELM_TZ}}, 3},
    {"ɖ", {{TTS_ELM_J, TTS_ELM_D}, {TTS_ELM_DY}, {TTS_ELM_ZZ, TTS_ELM_DZ}}, 3},
    {"m", {{TTS_ELM_M}}},
    {"n", {{TTS_ELM_N}}},
    {"ɳ", {{TTS_ELM_DH, TTS_ELM_N}}},
    {"ŋ", {{TTS_ELM_NG}}},
    {"f", {{TTS_ELM_F}}},
    {"v", {{TTS_ELM_V}, {TTS_ELM_QQ}, {TTS_ELM_V}}, 3},
    {"s", {{TTS_ELM_S}}},
    {"ʂ", {{TTS_ELM_CI, TTS_ELM_SH}}},
    {"ɕ", {{TTS_ELM_TH, TTS_ELM_SH}}},
    {"ʧ", {{TTS_ELM_T}, {TTS_ELM_TY}, {TTS_ELM_SH}}, 3},
    {"ɧ", {{TTS_ELM_X}}},
    {"h", {{TTS_ELM_H}}},
    {"l", {{TTS_ELM_L}}},
    {"ɭ", {{TTS_ELM_LL, TTS_ELM_L}}},
    {"r", {{TTS_ELM_RX}}, 1, 3},
    {"j", {{TTS_ELM_Y}}},
};
using Interpolators = decltype(tts_element_t::mInterpolator);
void set(Interpolators& element, int parameter, float steady, float fixed) {
    element[parameter].mSteady = steady;
    element[parameter].mFixed = fixed;
}
// Writes the Swedish values into speech.h's element tables, once, before any synthesis.
// Only the transition tables change; each element keeps its rank and durations.
bool tune() {
    static Interpolators original[TTS_ELM_OR + 1];
    for (int i = 0; i <= TTS_ELM_OR; ++i)
        std::memcpy(original[i], tts_gElement[i].mInterpolator, sizeof(Interpolators));
    const auto copy = [](int slot, int donor) -> Interpolators& {
        std::memcpy(tts_gElement[slot].mInterpolator, original[donor], sizeof(Interpolators));
        return tts_gElement[slot].mInterpolator;
    };
    for (const auto& vowel : vowels) {
        auto& element = copy(vowel.slot, vowel.donor);
        const int formants[] = {vowel.f1, vowel.f2, vowel.f3};
        // The donors' consonant loci lie at half the vowel's formant.
        for (int k = 0; k < 3; ++k)
            set(element, TTS_ELM_F1 + k, formants[k], formants[k] / 2.0f);
    }
    // Retroflex ʈ ɖ ɳ ɭ ʂ: alveolar elements with the F3 locus lowered to about 1900 Hz,
    // which also colours the neighbouring vowels, and F2 slightly lower.
    const int retroflex[][2] = {{TTS_ELM_CH, TTS_ELM_T},  {TTS_ELM_Z, TTS_ELM_TZ}, {TTS_ELM_J, TTS_ELM_D},
                                {TTS_ELM_ZZ, TTS_ELM_DZ}, {TTS_ELM_DH, TTS_ELM_N}, {TTS_ELM_LL, TTS_ELM_L},
                                {TTS_ELM_CI, TTS_ELM_SH}};
    for (const auto& [slot, donor] : retroflex) {
        auto& element = copy(slot, donor);
        set(element, TTS_ELM_F2, 1600, element[TTS_ELM_F2].mFixed * 0.9f);
        // A boundary is fixed + proportion% of the neighbour; aim at 1900 Hz beside a 2500 Hz F3.
        const float share = element[TTS_ELM_F3].mProportion / 100.0f;
        set(element, TTS_ELM_F3, 1900, share >= 1 ? 0 : 1900 - share * 2500);
        // speech.h's alveolars leave the vowel's F3 alone; a retroflex bends it over ~40 ms.
        if (share < 1)
            element[TTS_ELM_F3].mExtDelay = 4;
    }
    // ɕ (tj): brighter and fronter than ʂ, with the frication weighted towards F4 and F5.
    auto& tj = copy(TTS_ELM_TH, TTS_ELM_SH);
    set(tj, TTS_ELM_F2, 2400, 1300);
    set(tj, TTS_ELM_F3, 3000, 0);
    set(tj, TTS_ELM_A3, 28, 0);
    set(tj, TTS_ELM_A4, 42, 0);
    set(tj, TTS_ELM_A5, 35, 0);
    // r: a 30 ms voiced tap, a brief shallow dip in an l, instead of English's F3-lowering approximant.
    auto& r = copy(TTS_ELM_RX, TTS_ELM_L);
    set(r, TTS_ELM_F1, 300, 150);
    set(r, TTS_ELM_F3, 2500, 1250);
    for (int a = TTS_ELM_A1; a <= TTS_ELM_A4; ++a)
        set(r, a, r[a].mSteady - 6, r[a].mFixed);
    // l's 60 ms transitions would smear the tap into a long dip.
    for (auto& parameter : r) {
        parameter.mExtDelay = std::min<char>(parameter.mExtDelay, 1);
        parameter.mIntDelay = std::min<char>(parameter.mIntDelay, 1);
    }
    return true;
}
// One UTF-8 symbol per phone, with a following length mark attached.
std::vector<std::string> split(const std::string& ipa) {
    std::vector<std::string> result;
    for (size_t at = 0; at < ipa.size();) {
        const auto lead = static_cast<unsigned char>(ipa[at]);
        const size_t size = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        auto symbol = ipa.substr(at, size);
        at += size;
        if (symbol == "ː" && !result.empty())
            result.back() += symbol;
        else
            result.push_back(std::move(symbol));
    }
    return result;
}
bool silent(const std::string& symbol) {
    return symbol == " " || symbol == "ˈ" || symbol == "ˌ" || symbol == "-" ||
           (symbol.size() == 1 && std::strchr(",.!?;:", symbol[0]));
}
const Vowel* vowel(const std::string& symbol) {
    for (const auto& candidate : vowels)
        if (symbol == candidate.ipa)
            return &candidate;
    return nullptr;
}
const Consonant* consonant(const std::string& symbol) {
    for (const auto& candidate : consonants)
        if (symbol == candidate.ipa)
            return &candidate;
    return nullptr;
}
} // namespace
std::vector<float> formant_speech(const std::string& ipa) {
    static const bool tuned = tune();
    (void)tuned;
    // speech.h reads (element, frames of 10 ms, stress) triplets.
    std::vector<unsigned char> elements;
    int frames = 0, stress = 0;
    bool spoken = false, after_short_stressed = false;
    const auto put = [&](int element, int duration, int level) {
        elements.insert(elements.end(),
                        {static_cast<unsigned char>(element), static_cast<unsigned char>(duration),
                         static_cast<unsigned char>(level)});
        frames += duration;
    };
    // Three pauses in a row also reset speech.h's falling pitch for the next sentence.
    const auto pause = [&](int count) {
        for (int i = 0; i < count; ++i)
            put(TTS_ELM_Q, 12, 0);
    };
    for (const auto& symbol : split(ipa)) {
        if (symbol == "ˈ" || symbol == "ˌ") {
            stress = symbol == "ˈ" ? 3 : 2;
        } else if (symbol == "." || symbol == "!" || symbol == "?") {
            pause(3);
        } else if (silent(symbol)) {
            if (symbol != " ")
                pause(1);
        } else if (const auto* v = vowel(symbol)) {
            const int duration = v->length == long_vowel    ? (stress ? 17 : 11)
                                 : v->length == short_vowel ? (stress ? 9 : 6)
                                                            : 5;
            put(v->slot, duration, stress);
            after_short_stressed = stress && v->length == short_vowel;
            stress = 0;
            spoken = true;
        } else if (const auto* c = consonant(symbol)) {
            for (int k = 0; k < c->count; ++k) {
                const auto& part = c->parts[k];
                const auto* timing = &tts_gElement[part.timing < 0 ? part.slot : part.timing];
                int duration = c->frames ? c->frames : TTS_StressDur(timing);
                // A Swedish consonant is long after a short stressed vowel.
                if (after_short_stressed && k == 0)
                    duration = duration * 3 / 2;
                put(part.slot, duration, 0);
            }
            after_short_stressed = false;
            spoken = true;
        }
    }
    // Text without speakable phones reads as a short pause, as with the neural voices.
    if (!spoken)
        return std::vector<float>(sample_rate / 5, 0.0f);
    pause(1);
    tts_klatt_t synth;
    tts_klatt_init(&synth);
    synth.mSampleRate = sample_rate;
    synth.mNspFr = sample_rate / 100;
    tts_resonator_initResonator(&synth.mDownSampLowPassFilter, 950 * sample_rate / 10000,
                                630 * sample_rate / 10000, sample_rate);
    std::vector<short> pcm(static_cast<size_t>(synth.mNspFr) * (frames + 1));
    tts_klatt_initsynth(&synth, static_cast<int>(elements.size()), elements.data());
    size_t written = 0;
    for (int count; (count = tts_klatt_synth(&synth, 0, pcm.data() + written)) >= 0;)
        written += static_cast<size_t>(count);
    std::vector<float> result(written);
    std::transform(pcm.begin(), pcm.begin() + static_cast<long>(written), result.begin(), [](short sample) {
        return sample / 32768.0f;
    });
    return result;
}
std::vector<std::string> formant_unknown_phones(const std::string& ipa) {
    std::vector<std::string> unknown;
    for (const auto& symbol : split(ipa))
        if (!silent(symbol) && !vowel(symbol) && !consonant(symbol) &&
            std::find(unknown.begin(), unknown.end(), symbol) == unknown.end())
            unknown.push_back(symbol);
    return unknown;
}
} // namespace ortho
