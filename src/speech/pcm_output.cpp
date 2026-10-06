#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include <miniaudio.h>
#include "speech/pcm_output.hpp"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ortho {
struct PcmOutput::Impl {
    struct Playback { std::vector<float> samples; std::atomic<size_t> position{0}; };
    ma_device device{};
    std::shared_ptr<Playback> playing;
    std::atomic<bool> paused{false};
    static void render(ma_device* device,void* output,const void*,ma_uint32 frames) {
        auto* self=static_cast<Impl*>(device->pUserData);
        auto* samples=static_cast<float*>(output);
        std::memset(samples,0,frames*sizeof(float));
        const auto playing=std::atomic_load(&self->playing);
        if(!playing||self->paused.load())return;
        const auto position=playing->position.load();
        const auto count=std::min<size_t>(frames,playing->samples.size()-position);
        std::memcpy(samples,playing->samples.data()+position,count*sizeof(float));
        playing->position.fetch_add(count);
    }
    Impl() {
        auto config=ma_device_config_init(ma_device_type_playback);
        config.playback.format=ma_format_f32;config.playback.channels=1;config.sampleRate=24000;
        config.dataCallback=render;config.pUserData=this;
        if(ma_device_init(nullptr,&config,&device)!=MA_SUCCESS)throw std::runtime_error("Ljudenheten kunde inte öppnas.");
        if(ma_device_start(&device)!=MA_SUCCESS){ma_device_uninit(&device);throw std::runtime_error("Ljudenheten kunde inte startas.");}
    }
    ~Impl(){ma_device_uninit(&device);}
};
PcmOutput::PcmOutput():impl_(std::make_unique<Impl>()){}
PcmOutput::~PcmOutput()=default;
void PcmOutput::play(std::vector<float> samples) { auto playback=std::make_shared<Impl::Playback>();playback->samples=std::move(samples);std::atomic_store(&impl_->playing,std::move(playback)); }
void PcmOutput::pause(bool paused){impl_->paused.store(paused);}
void PcmOutput::stop(){std::atomic_store(&impl_->playing,std::shared_ptr<Impl::Playback>{});impl_->paused.store(false);}
bool PcmOutput::finished() const{const auto playback=std::atomic_load(&impl_->playing);return !playback||playback->position.load()>=playback->samples.size();}
}
