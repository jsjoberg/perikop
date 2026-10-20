#pragma once
#include <array>
#include <functional>
#include <span>
#include <vector>
namespace ortho {
inline constexpr int klatt_sample_rate = 24000;
// Parameters of one sound: nasal zero, formants 1-3 and bandwidths, parallel amplitudes in dB,
// bypass, voicing, breathy voicing, aspiration and frication amplitudes.
enum KlattParameter {
    klatt_fn,
    klatt_f1,
    klatt_f2,
    klatt_f3,
    klatt_b1,
    klatt_b2,
    klatt_b3,
    klatt_an,
    klatt_a1,
    klatt_a2,
    klatt_a3,
    klatt_a4,
    klatt_a5,
    klatt_a6,
    klatt_ab,
    klatt_av,
    klatt_avc,
    klatt_asp,
    klatt_af,
    klatt_parameters
};
// A parameter's steady value, and how it meets a neighbouring sound: the boundary value is
// fixed + proportion% of the neighbour's steady value, reached over `external` 10 ms frames
// inside the neighbour or `internal` frames inside this sound.
struct KlattTarget {
    float steady, fixed;
    int proportion, external, internal;
};
struct KlattElement {
    const char* name;
    int rank; // the higher-ranked of two neighbours shapes their transition
    int stressed_frames, unstressed_frames;
    bool vowel;
    std::array<KlattTarget, klatt_parameters> targets;
};
// Fixed gains of the parallel formant branches F1-F6. rsynth's were set at 10 kHz to imitate a
// cascade vocal tract; these are re-measured against natural Swedish vowels at 24 kHz.
inline constexpr float klatt_branch_gains[6] = {0.4f, 0.45f, 0.1f, 0.4f, 0.3f, 0.05f};
inline constexpr float rsynth_branch_gains[6] = {0.4f, 0.15f, 0.06f, 0.04f, 0.022f, 0.03f};
struct KlattSegment {
    const KlattElement* element;
    float frames; // duration in 10 ms frames
};
// A parallel-formant Klatt synthesizer ported from rsynth (Nick Ing-Simmons et al.) by way of
// SoLoud and speech.h, all public domain. `pitch` gives the voice's pitch in Hz at a time in
// 10 ms frames from the start. Returns mono samples at klatt_sample_rate in [-1, 1].
std::vector<float> klatt_synthesize(std::span<const KlattSegment> segments,
                                    const std::function<float(float)>& pitch);
} // namespace ortho
