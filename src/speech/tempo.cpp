#include "speech/tempo.hpp"
#include <sonic.h>
namespace ortho {
TempoStretch::~TempoStretch() {
    if (stream_)
        sonicDestroyStream(static_cast<sonicStream>(stream_));
}
size_t TempoStretch::render(PcmStream& source, float* output, size_t frames, float speed) {
    if (speed == 1.0f)
        return source.render(output, frames);
    std::fill_n(output, frames, 0.0f);
    // A new reading must not inherit the previous one's buffered audio.
    if (&source != source_) {
        if (stream_)
            sonicDestroyStream(static_cast<sonicStream>(stream_));
        stream_ = sonicCreateStream(24000, 1);
        source_ = &source;
    }
    auto* stream = static_cast<sonicStream>(stream_);
    sonicSetSpeed(stream, speed);
    int produced = 0;
    while (produced < int(frames)) {
        if (sonicSamplesAvailable(stream) > 0) {
            produced += sonicReadFloatFromStream(stream, output + produced, int(frames) - produced);
            continue;
        }
        float input[512];
        const auto copied = source.render(input, 512);
        if (copied)
            sonicWriteFloatToStream(stream, input, int(copied));
        else {
            if (source.finished())
                sonicFlushStream(stream);
            if (sonicSamplesAvailable(stream) == 0)
                break;
        }
    }
    return size_t(produced);
}
} // namespace ortho
