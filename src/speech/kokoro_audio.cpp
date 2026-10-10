#include "speech/kokoro_audio.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <numbers>
#include <onnxruntime_cxx_api.h>
#include <stdexcept>
namespace ortho {
namespace {
std::vector<float> style(const std::filesystem::path& file) {
    constexpr size_t count = size_t{510} * 256;
    if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) != count * sizeof(float))
        throw std::runtime_error("Kokoro voice tensor is missing or incomplete.");
    std::ifstream input(file, std::ios::binary);
    std::vector<float> result(count);
    input.read(reinterpret_cast<char*>(result.data()), count * sizeof(float));
    if (!input)
        throw std::runtime_error("Cannot read Kokoro voice tensor.");
    if constexpr (std::endian::native != std::endian::little)
        for (auto& value : result)
            value = std::bit_cast<float>(std::byteswap(std::bit_cast<uint32_t>(value)));
    if (std::ranges::any_of(result, [](float value) {
            return !std::isfinite(value);
        }))
        throw std::runtime_error("Invalid Kokoro voice tensor.");
    return result;
}
// The upstream sample renderer applies scipy.signal.iirnotch + filtfilt.
// Preserve that zero-phase filter, including its odd endpoint extension.
void notch(std::vector<float>& pcm, double frequency) {
    constexpr size_t pad = 9;
    if (pcm.size() <= pad)
        throw std::runtime_error("Kokoro audio is too short.");
    const double w = 2 * std::numbers::pi * frequency / 24000;
    const double gain = 1 / (1 + std::tan(w / 70));
    const double b0 = gain, b1 = -2 * gain * std::cos(w), b2 = gain, a1 = b1, a2 = 2 * gain - 1;
    std::vector<double> extended(pcm.size() + 2 * pad);
    for (size_t i = 0; i < pad; ++i)
        extended[i] = 2 * pcm.front() - pcm[pad - i];
    std::copy(pcm.begin(), pcm.end(), extended.begin() + pad);
    for (size_t i = 0; i < pad; ++i)
        extended[pad + pcm.size() + i] = 2 * pcm.back() - pcm[pcm.size() - 2 - i];
    const auto filter = [&] {
        double z1 = (1 - b0) * extended.front(), z2 = (b2 - a2) * extended.front();
        for (auto& value : extended) {
            const double output = b0 * value + z1;
            z1 = b1 * value - a1 * output + z2;
            z2 = b2 * value - a2 * output;
            value = output;
        }
    };
    filter();
    std::reverse(extended.begin(), extended.end());
    filter();
    std::reverse(extended.begin(), extended.end());
    for (size_t i = 0; i < pcm.size(); ++i)
        pcm[i] = static_cast<float>(extended[pad + i]);
}
} // namespace
void clean_kokoro_audio(std::vector<float>& pcm) {
    for (double frequency : {2400, 4800, 7200, 9600})
        notch(pcm, frequency);
}
struct KokoroAudio::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "orthodox-reader-kokoro"};
    Ort::SessionOptions options;
    Ort::Session session{nullptr};
    std::vector<float> alice, bjorn;
    explicit Impl(const std::filesystem::path& pack)
        : alice(style(pack / "alice.bin")), bjorn(style(pack / "bjorn.bin")) {
        env.DisableTelemetryEvents();
        options.SetIntraOpNumThreads(4);
        options.SetInterOpNumThreads(1);
        // Park the pool between requests without disabling coordination during inference.
        options.AddConfigEntry("session.force_spinning_stop", "1");
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session = Ort::Session(env, (pack / "kokoro.onnx").native().c_str(), options);
    }
};
KokoroAudio::KokoroAudio(const std::filesystem::path& pack) : impl_(std::make_unique<Impl>(pack)) {}
KokoroAudio::~KokoroAudio() = default;
std::vector<float> KokoroAudio::generate(const std::vector<int64_t>& phones, const std::string& voice) {
    if (phones.empty() || phones.size() > 510)
        throw std::runtime_error("Kokoro phoneme sequence exceeds the voice context.");
    if (voice != "alice" && voice != "bjorn")
        throw std::runtime_error("Unknown Kokoro voice.");
    if (std::ranges::any_of(phones, [](int64_t id) {
            return id <= 0 || id >= 178;
        }))
        throw std::runtime_error("Invalid Kokoro phoneme token.");
    std::vector<int64_t> tokens;
    tokens.reserve(phones.size() + 2);
    tokens.push_back(0);
    tokens.insert(tokens.end(), phones.begin(), phones.end());
    tokens.push_back(0);
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const std::array<int64_t, 2> input_shape = {1, static_cast<int64_t>(tokens.size())},
                                 style_shape = {1, 256};
    auto& styles = voice == "alice" ? impl_->alice : impl_->bjorn;
    auto input = Ort::Value::CreateTensor<int64_t>(memory, tokens.data(), tokens.size(), input_shape.data(),
                                                   input_shape.size());
    auto reference = Ort::Value::CreateTensor<float>(memory, styles.data() + (phones.size() - 1) * 256, 256,
                                                     style_shape.data(), style_shape.size());
    std::array<Ort::Value, 2> inputs = {std::move(input), std::move(reference)};
    const char* names[] = {"input_ids", "ref_s"};
    const char* output_names[] = {"audio"};
    auto result =
        impl_->session.Run(Ort::RunOptions{nullptr}, names, inputs.data(), inputs.size(), output_names, 1);
    const auto count = result[0].GetTensorTypeAndShapeInfo().GetElementCount();
    const auto* samples = result[0].GetTensorData<float>();
    if (count < 1000 || count > size_t{24000} * 120 || std::any_of(samples, samples + count, [](float value) {
            return !std::isfinite(value);
        }))
        throw std::runtime_error("Invalid Kokoro audio output.");
    std::vector<float> pcm(samples, samples + count);
    clean_kokoro_audio(pcm);
    if (!std::ranges::any_of(pcm, [](float value) {
            return std::abs(value) > 0.0001f;
        }))
        throw std::runtime_error("Kokoro produced silent audio.");
    return pcm;
}
} // namespace ortho
