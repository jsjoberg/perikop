#pragma once
#include "speech/swedish_prosody.hpp"
#include <span>
#include <string>
#include <vector>
namespace ortho {
// The settings value of Cesar, the simple Swedish voice (C after Alice and Björn).
inline constexpr char cesar_voice_id[] = "cesar";
// Cesar: a Klatt formant synthesizer with Swedish sounds and melody, driven by KokoroText::ipa().
// Robotic but intelligible, needs no model and runs far faster than real time.
// Returns 24 kHz mono samples. Symbols it cannot say are skipped.
std::vector<float> cesar_speech(const std::string& ipa);
// Diagnostic controls. Production continues to use the default melody until a
// general replacement has passed native listening review on unseen passages.
struct CesarOptions {
    std::span<const PitchPoint> reference_pitch;
    bool compact_melody = false;
    std::span<const SwedishWordProsody> words;
};
struct CesarSound {
    const char* name;
    float frames;
    size_t source, word;
    int samples, stress;
    bool vowel;
    char pause;
};
struct CesarTrace {
    std::vector<CesarSound> sounds;
    std::vector<PitchPoint> pitch;
};
std::vector<float> cesar_speech(const std::string& ipa, const CesarOptions& options,
                                CesarTrace* trace = nullptr);
// The phones in ipa that Cesar cannot say, for tests.
std::vector<std::string> cesar_unknown_phones(const std::string& ipa);
} // namespace ortho
