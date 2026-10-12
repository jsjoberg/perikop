#pragma once
#include "speech/pcm_stream.hpp"
namespace ortho {
// Plays a PcmStream at another tempo without changing pitch (Sonic).
// Used from the audio callback only.
class TempoStretch {
    void* stream_ = nullptr;
    uint64_t generation_ = 0;
    double delivered_ = 0;
    float output_speed_ = 1;
    bool flushed_ = false, finished_ = false, waiting_ = true;
    uint64_t played_ = 0;

public:
    TempoStretch() = default;
    TempoStretch(const TempoStretch&) = delete;
    TempoStretch& operator=(const TempoStretch&) = delete;
    ~TempoStretch();
    // Fills output from source at the given speed; returns frames written.
    size_t render(PcmStream& source, float* output, size_t frames, float speed);
    // Source-frame position estimated from audio delivered, never source read-ahead.
    uint64_t played() const {
        return played_;
    }
    bool finished() const {
        return finished_;
    }
    bool waiting() const {
        return waiting_;
    }
};
} // namespace ortho
