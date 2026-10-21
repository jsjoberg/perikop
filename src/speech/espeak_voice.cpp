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
// eSpeak NG 1.52 Swedish codes (ph_swedish and base1). eSpeak's r is English; Swedish r is R.
// Retroflex stops have no code, so they are written r + dental, as eSpeak's own rules do.
constexpr std::pair<std::string_view, std::string_view> codes[] = {
    {"iː", "i:"}, {"i", "i"},   {"ɪ", "I"},  {"eː", "e:"}, {"e", "e"},   {"ɛː", "E:"}, {"ɛ", "E"},
    {"ə", "E-"},  {"yː", "y:"}, {"y", "y"},  {"ʏ", "y"},   {"ʉː", "u-"}, {"ʉ", "u-"},  {"ɨː", "u-"},
    {"ɵ", "8"},   {"øː", "Y:"}, {"ø", "Y:"}, {"œː", "W"},  {"œ", "W"},   {"æː", "&:"}, {"æ", "&"},
    {"ɑː", "A:"}, {"ɑ", "A:"},  {"a", "a"},  {"oː", "o:"}, {"o", "o"},   {"ɔ", "O"},   {"uː", "u:"},
    {"u", "u"},   {"ʊ", "U"},   {"p", "p"},  {"b", "b"},   {"t", "t"},   {"d", "d"},   {"k", "k"},
    {"ɡ", "g"},   {"m", "m"},   {"n", "n"},  {"ŋ", "N"},   {"f", "f"},   {"v", "v"},   {"s", "s"},
    {"h", "h"},   {"l", "l"},   {"j", "j"},  {"r", "R"},   {"ɕ", "S;"},  {"ɧ", "sx"},  {"ʂ", "s."},
    {"ʈ", "Rt"},  {"ɖ", "Rd"},  {"ɳ", "n."}, {"ɭ", "l."},  {"ʧ", "tS"},  {"ˈ", "'"},   {"ˌ", ","},
    {" ", " "},   {"-", " "},
};
// The front end stresses every word. These function words (i, och, ett, en, det, den, de, att, som,
// på, av, för, till, med, om, är, var, har, hade, han, hon, honom, henne, hans, hennes, jag, du, vi,
// ni, dem, mig, dig, sig, sin, sitt, sina, min, mitt, mina, din, ditt, dina, vår, er, så, men, när,
// då, ty, ej, skall, ska, kan, vill, må, vid, mot, hos, åt, från, ur) are unstressed in running
// speech; listed as the front end transcribes them.
constexpr std::string_view unstressed[] = {
    "iː",   "ɔk",    "ɛt",    "ɛn",   "deːt",  "deːn",  "dɛ",   "at",   "sɔm",   "poː",  "ɑːv",
    "føːr", "tɪl",   "meːd",  "ɔm",   "ɛːr",   "vɑːr",  "hɑːr", "hadə", "hɑːn",  "huːn", "hɔnɔm",
    "hɛnə", "hɑːns", "hɛnəs", "jɑːɡ", "dyː",   "viː",   "niː",  "dɔm",  "mɛj",   "dɛj",  "sɛj",
    "sɪn",  "sɪt",   "siːna", "miːn", "mɪt",   "miːna", "diːn", "dɪt",  "diːna", "voːr", "eːr",
    "soː",  "mɛn",   "nɛːr",  "doː",  "tyː",   "ɛj",    "skal", "skɑː", "kɑːn",  "vɪl",  "moː",
    "viːd", "muːt",  "huːs",  "oːt",  "froːn", "ʉːr"};
bool punctuation(std::string_view symbol) {
    return symbol.size() == 1 && std::strchr(",.?!;:", symbol[0]);
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
const std::string_view* code(std::string_view symbol) {
    for (const auto& [ipa, espeak] : codes)
        if (symbol == ipa)
            return &espeak;
    return nullptr;
}
std::string trim(const std::string& text) {
    const auto begin = text.find_first_not_of(' ');
    return begin == std::string::npos ? "" : text.substr(begin, text.find_last_not_of(' ') - begin + 1);
}

// speak_lib.h's documented API, declared here so that no eSpeak header is needed.
using Initialize = int (*)(int output, int buffer_ms, const char* path, int options);
using Callback = int (*)(short* samples, int count, void* events);
using SetSynthCallback = void (*)(Callback);
using SetVoiceByName = int (*)(const char* name);
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

std::string espeak_phonemes(const std::string& ipa) {
    auto symbols = split(ipa);
    // Function words lose their stress mark.
    for (size_t begin = 0; begin < symbols.size();) {
        size_t end = begin;
        std::string word;
        while (end < symbols.size() && symbols[end] != " " && !punctuation(symbols[end])) {
            if (symbols[end] != "ˈ")
                word += symbols[end];
            ++end;
        }
        if (std::ranges::find(unstressed, word) != std::end(unstressed))
            for (size_t i = begin; i < end; ++i)
                if (symbols[i] == "ˈ")
                    symbols[i].clear();
        begin = end + 1;
    }
    std::string out, word;
    for (const auto& symbol : symbols) {
        if (punctuation(symbol)) {
            if (const auto phones = trim(word); !phones.empty())
                out += "[[" + phones + "]]";
            out += symbol + " ";
            word.clear();
        } else if (const auto* espeak = code(symbol)) {
            word += *espeak;
        }
    }
    if (const auto phones = trim(word); !phones.empty())
        out += "[[" + phones + "]]";
    return trim(out);
}

std::vector<std::string> espeak_unknown_phones(const std::string& ipa) {
    std::vector<std::string> result;
    for (const auto& symbol : split(ipa))
        if (!symbol.empty() && !punctuation(symbol) && !code(symbol) &&
            std::find(result.begin(), result.end(), symbol) == result.end())
            result.push_back(symbol);
    return result;
}

bool espeak_installed() {
    const auto library = open_library();
    if (library)
        close_library(library);
    return library != nullptr;
}

struct Espeak::Impl {
    Library library = nullptr;
    int rate = 0;
    std::string voice;
    Initialize initialize = nullptr;
    SetSynthCallback set_callback = nullptr;
    SetVoiceByName set_voice = nullptr;
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
    void close() {
        close_library(library);
    }
};

Espeak::Espeak() : impl_(std::make_unique<Impl>()) {
    std::lock_guard lock(engine);
    if (engine_open)
        throw std::logic_error("eSpeak NG is already open.");
    impl_->library = open_library();
    if (!impl_->library)
        throw std::runtime_error("eSpeak NG är inte installerat. Anvisningar finns under Inställningar.");
    try {
        impl_->bind(impl_->initialize, "espeak_Initialize");
        impl_->bind(impl_->set_callback, "espeak_SetSynthCallback");
        impl_->bind(impl_->set_voice, "espeak_SetVoiceByName");
        impl_->bind(impl_->set_parameter, "espeak_SetParameter");
        impl_->bind(impl_->synth, "espeak_Synth");
        impl_->bind(impl_->info, "espeak_Info");
        impl_->bind(impl_->terminate, "espeak_Terminate");
        // Without DONT_EXIT, eSpeak ends the whole process when its data is missing.
        impl_->rate = impl_->initialize(audio_output_synchronous, 0, nullptr, initialize_dont_exit);
        if (impl_->rate <= 0)
            throw std::runtime_error("eSpeak NG kunde inte starta; dess språkdata saknas kanske.");
    } catch (...) {
        impl_->close();
        throw;
    }
    impl_->set_callback(receive);
    engine_open = true;
}

Espeak::~Espeak() {
    std::lock_guard lock(engine);
    impl_->set_callback(nullptr);
    impl_->terminate();
    impl_->close();
    engine_open = false;
}

std::string Espeak::version() const {
    const char* version = impl_->info(nullptr);
    return version ? version : "";
}

std::vector<float> Espeak::speak(const std::string& ipa, const Settings& settings,
                                 const std::function<bool()>& cancelled) {
    std::lock_guard lock(engine);
    if (settings.voice != impl_->voice) {
        if (impl_->set_voice(settings.voice.c_str()) != ee_ok)
            throw std::runtime_error("eSpeak NG har ingen röst som heter " + settings.voice + ".");
        impl_->voice = settings.voice;
    }
    impl_->set_parameter(parameter_rate, settings.rate, 0);
    const auto text = espeak_phonemes(ipa);
    std::vector<short> samples;
    capture = &samples;
    capture_cancelled = &cancelled;
    const int result = impl_->synth(text.c_str(), text.size() + 1, 0, position_character, 0,
                                    chars_utf8 | phoneme_input, nullptr, nullptr);
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
