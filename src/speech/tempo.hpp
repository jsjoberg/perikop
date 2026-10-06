#pragma once
#include "speech/pcm_stream.hpp"
namespace ortho {
// Plays a PcmStream at another tempo without changing pitch (Sonic).
// Used from the audio callback only.
class TempoStretch {
    void* stream_ = nullptr;
    const PcmStream* source_ = nullptr;

public:
    TempoStretch() = default;
    TempoStretch(const TempoStretch&) = delete;
    TempoStretch& operator=(const TempoStretch&) = delete;
    ~TempoStretch();
    // Fills output from source at the given speed; returns frames written.
    size_t render(PcmStream& source, float* output, size_t frames, float speed);
};
} // namespace ortho
