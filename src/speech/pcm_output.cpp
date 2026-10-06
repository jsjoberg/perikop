#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/tempo.hpp"
#include <atomic>
#include <cstring>
#include <miniaudio.h>
#include <stdexcept>
namespace ortho {
struct PcmOutput::Impl {
    ma_device device{};
    std::shared_ptr<PcmStream> playing;
    std::atomic<bool> paused{false};
    std::atomic<float> speed{1};
    TempoStretch stretch; // Audio callback only.
    static void render(ma_device* device, void* output, const void*, ma_uint32 frames) {
        auto* self = static_cast<Impl*>(device->pUserData);
        auto* samples = static_cast<float*>(output);
        std::memset(samples, 0, frames * sizeof(float));
        const auto playing = std::atomic_load(&self->playing);
        if (!playing || self->paused.load())
            return;
        self->stretch.render(*playing, samples, frames, self->speed.load());
    }
    Impl() {
        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 1;
        config.sampleRate = 24000;
        config.dataCallback = render;
        config.pUserData = this;
        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
            throw std::runtime_error("Ljudenheten kunde inte öppnas.");
        if (ma_device_start(&device) != MA_SUCCESS) {
            ma_device_uninit(&device);
            throw std::runtime_error("Ljudenheten kunde inte startas.");
        }
    }
    ~Impl() {
        ma_device_uninit(&device);
    }
};
PcmOutput::PcmOutput() : impl_(std::make_unique<Impl>()) {}
PcmOutput::~PcmOutput() = default;
void PcmOutput::start(bool paused) {
    impl_->paused.store(paused);
    std::atomic_store(&impl_->playing, std::make_shared<PcmStream>());
}
void PcmOutput::append(std::vector<float> samples) {
    const auto playback = std::atomic_load(&impl_->playing);
    if (!playback || !playback->append(std::move(samples)))
        throw std::runtime_error("Ljudbufferten är full eller stängd.");
}
void PcmOutput::complete() {
    if (const auto playback = std::atomic_load(&impl_->playing))
        playback->complete();
}
size_t PcmOutput::buffered() const {
    const auto playback = std::atomic_load(&impl_->playing);
    return playback ? playback->buffered() : 0;
}
uint64_t PcmOutput::buffered_frames() const {
    const auto playback = std::atomic_load(&impl_->playing);
    return playback ? playback->buffered_frames() : 0;
}
void PcmOutput::release() {
    if (const auto playback = std::atomic_load(&impl_->playing))
        playback->release();
}
void PcmOutput::pause(bool paused) {
    impl_->paused.store(paused);
}
void PcmOutput::set_speed(double speed) {
    impl_->speed.store(float(speed));
}
void PcmOutput::stop() {
    std::atomic_store(&impl_->playing, std::shared_ptr<PcmStream>{});
    impl_->paused.store(false);
}
bool PcmOutput::finished() const {
    const auto playback = std::atomic_load(&impl_->playing);
    return !playback || playback->finished();
}
PcmProgress PcmOutput::progress() const {
    const auto playback = std::atomic_load(&impl_->playing);
    return playback ? PcmProgress{playback->played(), playback->waiting(), playback->finished()}
                    : PcmProgress{};
}
} // namespace ortho
