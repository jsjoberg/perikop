#pragma once
#include "speech/speech.hpp"
#include <filesystem>
#include <functional>
#include <memory>
namespace ortho {
struct SpeechUpdate {
    uint64_t sequence;
    std::string text;
};
using SpeechStatus = std::function<void(const SpeechUpdate&)>;
// Turns one chunk of speech text into 24 kHz mono audio for a voice and speed (1 is normal);
// returns early when cancelled() is true. Tests supply their own; Perikop uses eSpeak NG.
using SpeechSynthesis = std::function<std::vector<float>(
    const std::string& text, const std::string& voice, double speed, const std::function<bool()>& cancelled)>;
// `speech_data` is the bundled Swedish pronunciation data.
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& speech_data, SpeechStatus,
                                                     SpeechSynthesis = {});
} // namespace ortho
