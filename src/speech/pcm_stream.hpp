#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <vector>
namespace ortho {
// One synthesis producer and one audio consumer. Slots remain allocated until
// the producer reuses them; the audio callback neither locks nor frees buffers.
class PcmStream {
    static constexpr size_t capacity=2;
    std::array<std::vector<float>,capacity> chunks_;
    std::atomic<size_t> read_{0},written_{0};
    std::atomic<bool> complete_{false};
    size_t position_=0; // Audio consumer only.
public:
    size_t buffered() const {
        const auto read=read_.load(std::memory_order_acquire);
        return written_.load(std::memory_order_acquire)-read;
    }
    bool append(std::vector<float>&& samples) {
        const auto written=written_.load(std::memory_order_relaxed);
        if(complete_.load(std::memory_order_acquire)||samples.empty()||
           written-read_.load(std::memory_order_acquire)==capacity)return false;
        chunks_[written%capacity]=std::move(samples);
        written_.store(written+1,std::memory_order_release);
        return true;
    }
    void complete(){complete_.store(true,std::memory_order_release);}
    bool finished() const{return complete_.load(std::memory_order_acquire)&&buffered()==0;}
    void render(float* output,size_t frames) {
        std::fill_n(output,frames,0.0f);
        size_t copied=0;
        auto read=read_.load(std::memory_order_relaxed);
        while(copied<frames&&read<written_.load(std::memory_order_acquire)) {
            const auto& chunk=chunks_[read%capacity];
            const auto count=std::min(frames-copied,chunk.size()-position_);
            std::memcpy(output+copied,chunk.data()+position_,count*sizeof(float));
            copied+=count;position_+=count;
            if(position_==chunk.size()) {
                position_=0;
                read_.store(++read,std::memory_order_release);
            }
        }
    }
};
}
