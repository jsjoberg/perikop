#include "speech/espeak_voice.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <numbers>
#include <stdexcept>
#include <string_view>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace ortho {
namespace {
std::string trim(const std::string& text) {
    const auto begin = text.find_first_not_of(' ');
    return begin == std::string::npos ? "" : text.substr(begin, text.find_last_not_of(' ') - begin + 1);
}

// speak_lib.h's documented API, declared here so that no eSpeak header is needed.
using Initialize = int (*)(int output, int buffer_ms, const char* path, int options);
using Callback = int (*)(short* samples, int count, void* events);
using SetSynthCallback = void (*)(Callback);
using SetVoiceByName = int (*)(const char* name);
struct Voice {
    const char *name, *languages, *identifier;
    unsigned char gender, age, variant, reserved;
    int score;
    void* spare;
};
using ListVoices = const Voice** (*)(Voice*);
using SetParameter = int (*)(int parameter, int value, int relative);
using Synth = int (*)(const void* text, size_t size, unsigned position, int position_type, unsigned end,
                      unsigned flags, unsigned* identifier, void* user_data);
using Info = const char* (*)(const char** data_path);
using Terminate = int (*)();
constexpr int audio_output_synchronous = 2, initialize_dont_exit = 0x8000, parameter_rate = 1,
              position_character = 1, chars_utf8 = 1, phoneme_input = 0x100, ee_ok = 0;
constexpr int output_rate = 24000;

std::vector<std::filesystem::path> library_candidates() {
#if defined(_WIN32)
    std::vector<std::filesystem::path> result;
    // The eSpeak NG installer records its folder here.
    wchar_t folder[MAX_PATH];
    DWORD size = sizeof folder;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\eSpeak NG", L"Path", RRF_RT_REG_SZ, nullptr, folder,
                     &size) == ERROR_SUCCESS)
        result.push_back(std::filesystem::path(folder) / "libespeak-ng.dll");
    // No bare name: the DLL search order includes the working directory and PATH.
    return result;
#elif defined(__APPLE__)
    // Only where Homebrew and MacPorts install; a bare name would also search the working directory.
    return {"/opt/homebrew/lib/libespeak-ng.1.dylib", "/usr/local/lib/libespeak-ng.1.dylib",
            "/opt/local/lib/libespeak-ng.1.dylib"};
#else
    // The soname resolves as any linked library does: ld.so.cache and system directories.
    return {"libespeak-ng.so.1"};
#endif
}
#if defined(_WIN32)
using Library = HMODULE;
#else
using Library = void*;
#endif
Library open_library() {
    for (const auto& candidate : library_candidates()) {
#if defined(_WIN32)
        const auto library = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
        const auto library = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        if (library)
            return library;
    }
    return nullptr;
}
void close_library(Library library) {
#if defined(_WIN32)
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}
// Audio arrives through a plain C callback; eSpeak's state is global, so ours is too.
std::mutex engine;
bool engine_open = false;
std::vector<short>* capture = nullptr;
const std::function<bool()>* capture_cancelled = nullptr;
int receive(short* samples, int count, void*) {
    if (capture && samples && count > 0)
        capture->insert(capture->end(), samples, samples + count);
    return capture_cancelled && *capture_cancelled && (*capture_cancelled)() ? 1 : 0;
}
// Windowed-sinc resampling of eSpeak's native rate to the 24 kHz the player uses.
std::vector<float> resample(const std::vector<short>& input, int rate) {
    constexpr int half = 16;
    const double step = double(rate) / output_rate, cutoff = std::min(1.0, 1 / step);
    const auto width = half / cutoff;
    std::vector<float> output(size_t(std::floor(input.size() / step)));
    for (size_t n = 0; n < output.size(); ++n) {
        const double at = n * step;
        double sum = 0;
        const auto first = std::max<std::ptrdiff_t>(0, std::ptrdiff_t(std::ceil(at - width)));
        const auto last =
            std::min<std::ptrdiff_t>(std::ptrdiff_t(input.size()) - 1, std::ptrdiff_t(at + width));
        for (auto k = first; k <= last; ++k) {
            const double x = (at - k) * cutoff;
            const double sinc = x == 0 ? 1 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
            const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * x / half); // Hann
            sum += input[k] * cutoff * sinc * window;
        }
        output[n] = float(std::clamp(sum / 32768, -1.0, 1.0));
    }
    return output;
}
} // namespace

std::string espeak_phonemes(const std::vector<SpeechToken>& tokens) {
    std::string out, word;
    const auto flush = [&](const char* after) {
        if (!word.empty())
            out += "[[" + word + "]]" + after;
        word.clear();
    };
    for (const auto& token : tokens)
        switch (token.kind) {
        case SpeechToken::Kind::Phonemes:
            word += (word.empty() ? "" : " ") + token.text;
            break;
        // A word without known phonemes goes to eSpeak as text, for its own Swedish rules.
        case SpeechToken::Kind::Spelled:
            flush(" ");
            out += token.text + " ";
            break;
        case SpeechToken::Kind::Punctuation:
            flush("");
            out += token.text + " ";
            break;
        }
    flush("");
    return trim(out);
}

bool espeak_installed() {
    const auto library = open_library();
    if (library)
        close_library(library);
    return library != nullptr;
}

struct Espeak::Impl {
    inline static Impl* active = nullptr; // guarded by engine
    Library library = nullptr;
    int rate = 0;
    std::string voice;
    Initialize initialize = nullptr;
    SetSynthCallback set_callback = nullptr;
    SetVoiceByName set_voice = nullptr;
    ListVoices list_voices = nullptr;
    SetParameter set_parameter = nullptr;
    Synth synth = nullptr;
    Info info = nullptr;
    Terminate terminate = nullptr;
    template <typename Function> void bind(Function& function, const char* name) {
#if defined(_WIN32)
        function = reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(library, name)));
#else
        function = reinterpret_cast<Function>(dlsym(library, name));
#endif
        if (!function)
            throw std::runtime_error("Den installerade eSpeak NG saknar " + std::string(name) + ".");
    }
    void open() {
        library = open_library();
        if (!library)
            throw std::runtime_error("eSpeak NG är inte installerat. Anvisningar finns under Inställningar.");
        bind(initialize, "espeak_Initialize");
        bind(set_callback, "espeak_SetSynthCallback");
        bind(set_voice, "espeak_SetVoiceByName");
        bind(list_voices, "espeak_ListVoices");
        bind(set_parameter, "espeak_SetParameter");
        bind(synth, "espeak_Synth");
        bind(info, "espeak_Info");
        bind(terminate, "espeak_Terminate");
        // Without DONT_EXIT, eSpeak ends the whole process when its data is missing.
        rate = initialize(audio_output_synchronous, 0, nullptr, initialize_dont_exit);
        if (rate <= 0)
            throw std::runtime_error("eSpeak NG kunde inte starta; dess språkdata saknas kanske.");
    }
    ~Impl() {
        if (rate > 0) {
            set_callback(nullptr);
            terminate();
        }
        if (library)
            close_library(library);
    }
    std::vector<EspeakVoice> catalog(const std::string& language) {
        const auto definitions = [&](const char* language) {
            std::vector<std::pair<std::string, std::string>> result;
            Voice specification{};
            specification.languages = language;
            if (const auto rows = list_voices(&specification))
                for (auto row = rows; *row; ++row)
                    if ((*row)->name && (*row)->identifier)
                        result.emplace_back((*row)->identifier, (*row)->name);
            return result;
        };
        // ListVoices invalidates the previous result, so copy each list before the next call.
        const auto languages = definitions(language.c_str()), variants = definitions("variant");
        std::vector<EspeakVoice> result;
        // Probing changes global voice state. Force the next synthesis to select its voice again.
        voice.clear();
        const auto add = [&](const std::string& native, const std::string& name) {
            if (set_voice(native.c_str()) != ee_ok)
                return false;
            const auto id = native == "sv"      ? std::string(espeak_male_id)
                            : native == "sv+f2" ? std::string(espeak_female_id)
                                                : "espeak:" + native;
            if (std::ranges::find(result, id, &EspeakVoice::id) == result.end())
                result.push_back({id, "eSpeak – " + name});
            return true;
        };
        for (const auto& [identifier, name] : languages) {
            // MBROLA voices switch eSpeak to their own sample rate, which speak() does not follow.
            if (identifier.starts_with("mb/") || identifier.starts_with("!v/"))
                continue;
            const auto basename = identifier.substr(identifier.find_last_of('/') + 1);
            const auto native = basename == language ? language : identifier;
            if (!add(native, name))
                continue; // e.g. a definition whose voice data is absent
            for (const auto& [variant, variant_name] : variants)
                if (variant.starts_with("!v/"))
                    add(native + "+" + variant.substr(3), name + " – " + variant_name);
        }
        return result;
    }
};

Espeak::Espeak() : impl_(std::make_unique<Impl>()) {
    std::lock_guard lock(engine);
    if (engine_open)
        throw std::logic_error("eSpeak NG is already open.");
    impl_->open();
    impl_->set_callback(receive);
    Impl::active = impl_.get();
    engine_open = true;
}

Espeak::~Espeak() {
    std::lock_guard lock(engine);
    Impl::active = nullptr;
    impl_.reset();
    engine_open = false;
}

std::string espeak_voice_name(const std::string& id) {
    if (id == espeak_male_id)
        return "sv";
    if (id == espeak_female_id)
        return "sv+f2";
    return id.starts_with("espeak:") ? id.substr(7) : "";
}

std::vector<EspeakVoice> Espeak::voices(const std::string& language) {
    std::lock_guard lock(engine);
    if (Impl::active)
        return Impl::active->catalog(language);
    Impl probe;
    probe.open();
    return probe.catalog(language);
}

std::string Espeak::version() const {
    const char* version = impl_->info(nullptr);
    return version ? version : "";
}

std::vector<float> Espeak::speak(const std::vector<SpeechToken>& tokens, const Settings& settings,
                                 const std::function<bool()>& cancelled) {
    return synthesize(espeak_phonemes(tokens), settings, chars_utf8 | phoneme_input, cancelled);
}
std::vector<float> Espeak::speak_text(const std::string& text, const Settings& settings,
                                      const std::function<bool()>& cancelled) {
    return synthesize(text, settings, chars_utf8, cancelled);
}
std::vector<float> Espeak::synthesize(const std::string& text, const Settings& settings, unsigned flags,
                                      const std::function<bool()>& cancelled) {
    std::lock_guard lock(engine);
    if (settings.voice != impl_->voice) {
        if (impl_->set_voice(settings.voice.c_str()) != ee_ok)
            throw std::runtime_error("eSpeak NG har ingen röst som heter " + settings.voice + ".");
        impl_->voice = settings.voice;
    }
    impl_->set_parameter(parameter_rate, settings.rate, 0);
    std::vector<short> samples;
    capture = &samples;
    capture_cancelled = &cancelled;
    const int result =
        impl_->synth(text.c_str(), text.size() + 1, 0, position_character, 0, flags, nullptr, nullptr);
    capture = nullptr;
    capture_cancelled = nullptr;
    if (cancelled && cancelled())
        return {};
    if (result != ee_ok)
        throw std::runtime_error("eSpeak NG kunde inte läsa texten.");
    // Text without speakable phones reads as a short pause, as with the other voices.
    if (samples.empty())
        return std::vector<float>(output_rate / 5, 0.0f);
    return resample(samples, impl_->rate);
}
} // namespace ortho
