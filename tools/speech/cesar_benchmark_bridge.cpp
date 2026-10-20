#include "speech/cesar_voice.hpp"
#include <algorithm>
#include <cstdlib>
// Research-only C ABI for timing the native synthesizer in the same Python
// process environment as the ONNX candidates. Never linked into the app.
extern "C" float* compact_cesar(const char* ipa, size_t* count) {
    try {
        if (!ortho::cesar_unknown_phones(ipa).empty())
            return nullptr;
        const auto audio = ortho::cesar_speech(ipa, {.compact_melody = true});
        *count = audio.size();
        auto* result = static_cast<float*>(std::malloc(audio.size() * sizeof(float)));
        if (!result)
            return nullptr;
        std::copy(audio.begin(), audio.end(), result);
        return result;
    } catch (...) {
        *count = 0;
        return nullptr;
    }
}
extern "C" void compact_cesar_free(float* samples) {
    std::free(samples);
}
