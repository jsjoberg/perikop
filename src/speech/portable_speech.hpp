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
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& user_data, SpeechStatus);
bool render_speech_probe(const std::filesystem::path& user_data, const std::filesystem::path& output);
} // namespace ortho
