#pragma once
#include <string>
#include <vector>
namespace ortho {
// The settings value of the simple Swedish voice.
inline constexpr char formant_voice_id[] = "enkel";
// Simple Swedish voice: the Klatt formant synthesizer of speech.h (rsynth, public domain)
// with Swedish vowel and consonant tables, driven by KokoroText::ipa(). Robotic but
// intelligible, needs no model and runs far faster than real time.
// Returns 24 kHz mono samples. Symbols it cannot say are skipped.
std::vector<float> formant_speech(const std::string& ipa);
// The phones in ipa that formant_speech cannot say, for tests.
std::vector<std::string> formant_unknown_phones(const std::string& ipa);
} // namespace ortho
