#pragma once
#include <string>
#include <vector>
namespace ortho {
// The settings value of Cesar, the simple Swedish voice (C after Alice and Björn).
inline constexpr char cesar_voice_id[] = "cesar";
// Cesar: a Klatt formant synthesizer with Swedish sounds and melody, driven by KokoroText::ipa().
// Robotic but intelligible, needs no model and runs far faster than real time.
// Returns 24 kHz mono samples. Symbols it cannot say are skipped.
std::vector<float> cesar_speech(const std::string& ipa);
// The phones in ipa that Cesar cannot say, for tests.
std::vector<std::string> cesar_unknown_phones(const std::string& ipa);
} // namespace ortho
