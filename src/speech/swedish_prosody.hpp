#pragma once
#include <cstddef>
#include <span>
#include <vector>
namespace ortho {
struct PitchPoint {
    float seconds, hz;
};
// Stress alone does not identify a Swedish word accent. Keep unknown explicit.
enum class SwedishAccent { Unknown, Accent1, Accent2 };
struct SwedishWordProsody {
    SwedishAccent accent = SwedishAccent::Unknown;
    float prominence = -1; // -1: automatic; otherwise 0..1
    bool weak = false;
    bool focal = false;
};
struct ProsodySyllable {
    size_t word;
    float begin, end; // vowel interval in seconds on the synthesizer's control clock
    int stress;       // 3 primary, 2 secondary, 0 unstressed
};
struct ProsodyPhrase {
    float begin, end;
    char ending;
};
// Experimental Stockholm-oriented melody planner. Symbolic word accent, prominence,
// phrase boundaries and vowel anchors are separate inputs. It never changes timing.
// The structure follows Swedish prosody research; its numerical parameters are a
// provisional implementation, not the fitted coefficients of a published model.
std::vector<PitchPoint> swedish_melody(std::span<const ProsodySyllable> syllables,
                                       std::span<const SwedishWordProsody> words,
                                       std::span<const ProsodyPhrase> phrases, float duration);
// A reference contour may have repeated times to describe a step. Validate once
// before rendering; interpolation holds the first/last value outside its support.
void validate_pitch(std::span<const PitchPoint> points);
float pitch_at(std::span<const PitchPoint> points, float seconds);
} // namespace ortho
