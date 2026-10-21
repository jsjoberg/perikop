#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
// KokoroText::ipa() as eSpeak NG Swedish phoneme input: codes in [[...]], punctuation outside,
// function words unstressed. Symbols without a code are skipped.
std::string espeak_phonemes(const std::string& ipa);
// The phones in ipa that have no eSpeak code, for tests.
std::vector<std::string> espeak_unknown_phones(const std::string& ipa);
// Settings values of the two eSpeak voices: eSpeak's stock "sv" and "sv+f2".
inline constexpr char espeak_male_id[] = "espeak";
inline constexpr char espeak_female_id[] = "espeak-f2";
// Whether an eSpeak NG library can be loaded, without starting it.
bool espeak_installed();
// eSpeak NG (GPL), loaded at run time from the user's own installation through its documented
// API. Perikop ships no eSpeak code or data. eSpeak keeps global state: use one instance at a time.
class Espeak {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    struct Settings {
        std::string voice = "sv"; // a stock voice, such as "sv" or "sv+f2"
        int rate = 150;           // words per minute
    };
    // Throws std::runtime_error when no usable eSpeak NG installation is found.
    Espeak();
    ~Espeak();
    Espeak(const Espeak&) = delete;
    Espeak& operator=(const Espeak&) = delete;
    // The installed version, as eSpeak reports it.
    std::string version() const;
    // 24 kHz mono samples for KokoroText::ipa() output; stops early when cancelled() is true.
    std::vector<float> speak(const std::string& ipa, const Settings& settings,
                             const std::function<bool()>& cancelled = {});
};
} // namespace ortho
