#include "speech/tempo.hpp"
#include <sonic.h>
namespace ortho {
TempoStretch::~TempoStretch() {
    if (stream_)
        sonicDestroyStream(static_cast<sonicStream>(stream_));
}
size_t TempoStretch::render(PcmStream& source, float* output, size_t frames, float speed) {
    std::fill_n(output, frames, 0.0f);
    // A new reading must not inherit the previous one's buffered audio.
    if (source.generation() != generation_) {
        if (stream_)
            sonicDestroyStream(static_cast<sonicStream>(stream_));
        stream_ = nullptr;
        generation_ = source.generation();
        delivered_ = 0;
        played_ = 0;
        flushed_ = finished_ = false;
        waiting_ = true;
    }
    // Keep unchanged PCM at normal speed until stretching is first needed.
    // Once Sonic has consumed input, unity speed must also go through it.
    if (!stream_ && speed == 1.0f) {
        const auto copied = source.render(output, frames);
        played_ = source.played();
        delivered_ = double(played_);
        finished_ = source.finished();
        waiting_ = !copied && source.waiting();
        return copied;
    }
    if (!stream_)
        stream_ = sonicCreateStream(24000, 1);
    auto* stream = static_cast<sonicStream>(stream_);
    if (!stream)
        return 0;
    int produced = 0;
    while (produced < int(frames)) {
        // Deliver previously processed audio at its original rate before changing
        // Sonic's speed. Retain its unprocessed input across all rate changes.
        if (sonicSamplesAvailable(stream) > 0) {
            const auto count = sonicReadFloatFromStream(stream, output + produced, int(frames) - produced);
            produced += count;
            delivered_ += count * double(output_speed_);
            continue;
        }
        if (flushed_)
            break;
        sonicSetSpeed(stream, speed);
        output_speed_ = speed;
        float input[512];
        const auto copied = source.render(input, 512);
        if (copied)
            sonicWriteFloatToStream(stream, input, int(copied));
        else {
            if (source.finished()) {
                sonicFlushStream(stream);
                flushed_ = true;
            }
            if (sonicSamplesAvailable(stream) == 0)
                break;
        }
    }
    finished_ = flushed_ && sonicSamplesAvailable(stream) == 0;
    waiting_ = !produced && !finished_ && source.waiting();
    // Sonic's overlap windows make source-frame progress approximate. Do not
    // let rounding or a final speed change announce the last cue before drain.
    const auto consumed = source.played();
    played_ = finished_ ? consumed : std::min(uint64_t(delivered_), consumed ? consumed - 1 : 0);
    return size_t(produced);
}
} // namespace ortho
