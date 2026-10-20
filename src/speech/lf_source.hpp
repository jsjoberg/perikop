#pragma once
#include <array>
namespace ortho {
// LF flow-derivative timing, normalized to one pitch period. These are explicit
// shape controls, not estimates of a particular speaker's vocal folds.
struct LfParameters {
    float tp = 0.40f; // peak of glottal flow (derivative crosses zero)
    float te = 0.65f; // maximum negative derivative at closure
    float ta = 0.04f; // effective exponential return time
};
enum class VoiceSource { Klatt, LiljencrantsFant };
struct VoiceSourceOptions {
    VoiceSource kind = VoiceSource::Klatt;
    LfParameters lf;
};
// A periodic LF flow derivative with Ee=1. Alpha and epsilon are solved from
// continuity and zero net flow; a small table avoids per-sample transcendental
// functions. The synthesizer still oversamples and applies its existing filter.
class LfPulse {
    static constexpr int table_size = 2048;
    std::array<float, table_size + 1> cycle_{};

public:
    explicit LfPulse(const LfParameters& parameters = {});
    // Phase in 0..1; endpoints and out-of-cycle phases are zero.
    float sample(float phase) const;
};
} // namespace ortho
