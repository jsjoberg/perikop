#pragma once
#include "speech/swedish_text.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace ortho {
// SwedishText::tokens() as eSpeak NG input: adjacent phoneme words in one [[...]], spelled words and
// punctuation outside as text.
std::string espeak_phonemes(const std::vector<SpeechToken>& tokens);
// Legacy settings values, kept so existing voice selections survive upgrades.
inline constexpr char espeak_male_id[] = "espeak";
inline constexpr char espeak_female_id[] = "espeak-f2";
// An eSpeak settings identity and its installed display name.
struct EspeakVoice {
    std::string id, name;
};
// The native eSpeak selector for a settings identity; empty for other engines.
std::string espeak_voice_name(const std::string& id);
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
    // Installed, loadable voices and variants for one language. Safe during playback.
    static std::vector<EspeakVoice> voices(const std::string& language = "sv");
    // 24 kHz mono samples for SwedishText::tokens() output; stops early when cancelled() is true.
    std::vector<float> speak(const std::vector<SpeechToken>& tokens, const Settings& settings,
                             const std::function<bool()>& cancelled = {});
    // Plain UTF-8 uses the selected language's built-in pronunciation rules.
    std::vector<float> speak_text(const std::string& text, const Settings& settings,
                                  const std::function<bool()>& cancelled = {});

private:
    std::vector<float> synthesize(const std::string&, const Settings&, unsigned flags,
                                  const std::function<bool()>&);
};
} // namespace ortho
