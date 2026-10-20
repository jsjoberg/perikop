#include "speech/swedish_prosody.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ortho {
namespace {
// Semitone contributions are superimposed on a phrase base. Gaussian gestures
// have smooth onsets/offsets; their alignment follows vowel anchors, not a fixed beat.
struct Gesture {
    float center, width, semitones;
};
struct PhraseMelody {
    ProsodyPhrase phrase;
    float onset;
    std::vector<Gesture> gestures;
};
float smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
float anchor(const ProsodySyllable& syllable, float fraction) {
    return syllable.begin + (syllable.end - syllable.begin) * fraction;
}
} // namespace
void validate_pitch(std::span<const PitchPoint> points) {
    float previous = 0;
    for (const auto& point : points) {
        if (!std::isfinite(point.seconds) || point.seconds < previous || !std::isfinite(point.hz) ||
            point.hz < 50 || point.hz > 400)
            throw std::invalid_argument("Invalid prosody pitch contour.");
        previous = point.seconds;
    }
}
float pitch_at(std::span<const PitchPoint> points, float seconds) {
    if (points.empty())
        throw std::invalid_argument("Empty prosody pitch contour.");
    const auto at = std::lower_bound(points.begin(), points.end(), seconds, [](const auto& point, float t) {
        return point.seconds < t;
    });
    if (at == points.begin())
        return at->hz;
    if (at == points.end())
        return points.back().hz;
    const auto& previous = *(at - 1);
    const float mix = (seconds - previous.seconds) / (at->seconds - previous.seconds);
    return previous.hz + (at->hz - previous.hz) * mix;
}
std::vector<PitchPoint> swedish_melody(std::span<const ProsodySyllable> syllables,
                                       std::span<const SwedishWordProsody> words,
                                       std::span<const ProsodyPhrase> phrases, float duration) {
    if (!std::isfinite(duration) || duration < 0 || duration > 120)
        throw std::invalid_argument("Invalid prosody duration.");
    for (const auto& word : words)
        if (!std::isfinite(word.prominence) || word.prominence < -1 || word.prominence > 1)
            throw std::invalid_argument("Invalid word prominence.");
    float previous = 0;
    for (const auto& syllable : syllables) {
        if (syllable.word >= words.size() || !std::isfinite(syllable.begin) || !std::isfinite(syllable.end) ||
            syllable.begin < previous || syllable.end <= syllable.begin || syllable.end > duration)
            throw std::invalid_argument("Invalid prosody syllable.");
        previous = syllable.end;
    }
    std::vector<PhraseMelody> melodies;
    previous = 0;
    char preceding_ending = '.';
    float preceding_onset = 2.3f;
    for (const auto& phrase : phrases) {
        if (!std::isfinite(phrase.begin) || !std::isfinite(phrase.end) || phrase.begin < previous ||
            phrase.end <= phrase.begin || phrase.end > duration)
            throw std::invalid_argument("Invalid prosody phrase.");
        previous = phrase.end;
        // Commas continue the larger sentence, with a partial rather than a full reset.
        const float onset = preceding_ending == ',' ? std::max(0.3f, preceding_onset - 0.5f) : 2.3f;
        preceding_onset = onset;
        preceding_ending = phrase.ending;
        PhraseMelody melody{phrase, onset, {}};
        size_t focus = words.size();
        bool explicit_focus = false;
        for (const auto& syllable : syllables) {
            if (syllable.begin < phrase.begin || syllable.begin >= phrase.end)
                continue;
            const auto& word = words[syllable.word];
            if (word.focal) {
                focus = syllable.word;
                explicit_focus = true;
            } else if (!explicit_focus && !word.weak && word.prominence != 0) {
                focus = syllable.word;
            }
        }
        for (size_t i = 0; i < syllables.size(); ++i) {
            const auto& primary = syllables[i];
            if (primary.begin < phrase.begin || primary.begin >= phrase.end || primary.stress != 3)
                continue;
            const auto& word = words[primary.word];
            const float prominence = word.prominence >= 0 ? word.prominence : word.weak ? 0.08f : 0.5f;
            if (prominence == 0)
                continue;
            const float width = std::clamp((primary.end - primary.begin) * 0.45f, 0.04f, 0.09f);
            const bool focal = primary.word == focus;
            const auto add = [&](float center, float spread, float height) {
                melody.gestures.push_back({center, spread, height});
            };
            if (word.accent == SwedishAccent::Accent2) {
                // Lexical H*L fall in the primary syllable; prominence is distinct.
                add(anchor(primary, 0.15f), width, 0.8f);
                add(anchor(primary, 0.85f), width, -0.8f);
                const ProsodySyllable* later = nullptr;
                for (size_t k = i + 1; k < syllables.size() && syllables[k].word == primary.word; ++k) {
                    if (syllables[k].begin >= phrase.end)
                        break;
                    if (!later || syllables[k].stress == 2)
                        later = &syllables[k];
                    if (syllables[k].stress == 2)
                        break; // compounds: anchor the rise at the secondary stress
                }
                if (later)
                    add(anchor(*later, 0.55f), std::max(width, 0.07f), prominence * (focal ? 4 : 1.8f));
            } else if (word.accent == SwedishAccent::Accent1) {
                // L* then Ha; avoid treating lexical stress as equal sentence emphasis.
                add(anchor(primary, 0.15f), width, -0.6f * prominence);
                add(anchor(primary, 0.85f), width, prominence * (focal ? 3.5f : 1.5f));
            } else {
                // No accent label: a small broad prominence gesture, without inventing
                // accent 2 from the number of syllables. This is an explicit fallback.
                add(anchor(primary, 0.65f), std::max(width, 0.09f), prominence * (focal ? 2.5f : 0.8f));
            }
        }
        melodies.push_back(std::move(melody));
    }
    std::vector<PitchPoint> result;
    const int steps = static_cast<int>(std::ceil(duration / 0.0025f));
    size_t current = 0;
    for (int step = 0; step <= steps; ++step) {
        const float t = step * 0.0025f;
        while (current + 1 < melodies.size() && t >= melodies[current].phrase.end)
            ++current;
        float semitones = -0.5f;
        if (!melodies.empty()) {
            const auto& melody = melodies[current];
            const auto& phrase = melody.phrase;
            const float position = std::clamp((t - phrase.begin) / (phrase.end - phrase.begin), 0.0f, 1.0f);
            semitones = melody.onset - 2.5f * smooth(position);
            for (const auto& gesture : melody.gestures) {
                const float distance = (t - gesture.center) / gesture.width;
                semitones += gesture.semitones * std::exp(-0.5f * distance * distance);
            }
            // These are provisional boundary settings. Question type and syntactic
            // grouping need richer text input before claiming general Swedish coverage.
            const float boundary = phrase.ending == ',' ? 0.3f : phrase.ending == '?' ? 0.8f : -1.5f;
            semitones += boundary * smooth((t - (phrase.end - 0.18f)) / 0.18f);
        }
        result.push_back({t, std::clamp(110 * std::exp2(semitones / 12), 85.0f, 190.0f)});
    }
    return result;
}
} // namespace ortho
