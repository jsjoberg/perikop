#include "speech/klatt.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
namespace ortho {
namespace {
constexpr float pi = std::numbers::pi_v<float>;
constexpr float rate = klatt_sample_rate;
// rsynth was tuned at 11025 Hz; per-sample filter constants are rescaled to keep their response in Hz.
constexpr float tuned_rate = 11025;
// The glottal source runs at four times the output rate.
constexpr int oversampling = 4;
// Parameters are re-evaluated every 2.5 ms instead of rsynth's 10 ms, so they glide rather than step.
constexpr int subframes_per_frame = 4;
constexpr int subframe_samples = klatt_sample_rate / 100 / subframes_per_frame;
// Fraction of each pitch period with the glottis open. rsynth fixed it at 30 samples at its
// rate, which at 24 kHz made every pulse sharp and buzzy.
constexpr float open_quotient = 0.5f;
// Fixed upper formants, frequency and parallel bandwidth in Hz, placed for a 24 kHz output.
constexpr float upper_formants[3][2] = {{3300, 400}, {4400, 600}, {6200, 1500}};
// Voicing spectral tilt in dB (rsynth's default 10).
constexpr float tilt_db = 0;
// Slow pitch jitter (Klatt's flutter), percent.
constexpr float flutter = 25;

// rsynth's amplitude scale: 87 dB is full 16-bit scale and 6 dB halves it, without its 1 dB steps.
float linear(float db) {
    return db <= 0 ? 0.0f : 32.767f * std::exp2((std::min(db, 87.0f) - 87) / 6);
}
float rescaled(float coefficient) {
    return std::pow(coefficient, tuned_rate / rate);
}
struct Resonator {
    float a = 0, b = 0, c = 0, p1 = 0, p2 = 0;
    void set(float frequency, float bandwidth, float gain = 1, float sampling = rate) {
        const float r = std::exp(-pi * bandwidth / sampling);
        c = -r * r;
        b = 2 * r * std::cos(2 * pi * frequency / sampling);
        a = (1 - b - c) * gain;
    }
    void set_anti(float frequency, float bandwidth) {
        set(frequency, bandwidth);
        a = 1 / a;
        b *= -a;
        c *= -a;
    }
    float resonate(float input) {
        const float output = a * input + b * p1 + c * p2;
        p2 = p1;
        p1 = output;
        return output;
    }
    float antiresonate(float input) {
        const float output = a * input + b * p1 + c * p2;
        p2 = p1;
        p1 = input;
        return output;
    }
};
struct Slope {
    float value = 0, time = 0;
};
float lerp(float from, float to, float t, float duration) {
    return t <= 0 ? from : t >= duration ? to : from + (to - from) * t / duration;
}
// rsynth's transition shape: glide in from the start boundary, hold, glide out to the end boundary.
float interpolate(const Slope& start, const Slope& end, float steady, float t, float duration) {
    const float hold = duration - (start.time + end.time);
    if (hold >= 0) {
        if (t < start.time)
            return lerp(start.value, steady, t, start.time);
        t -= start.time;
        return t <= hold ? steady : lerp(steady, end.value, t - hold, end.time);
    }
    const float weight = 1 - t / duration;
    return weight * lerp(start.value, steady, t, start.time) +
           (1 - weight) * lerp(end.value, steady, duration - t, end.time);
}
// The transition between `dominant` and its neighbour, timed by the dominant sound.
std::array<Slope, klatt_parameters> transition(const KlattElement& dominant, const KlattElement& other,
                                               bool external) {
    std::array<Slope, klatt_parameters> slopes;
    for (int i = 0; i < klatt_parameters; ++i) {
        const auto& target = dominant.targets[i];
        slopes[i].time = static_cast<float>(external ? target.external : target.internal);
        slopes[i].value = slopes[i].time ? target.fixed + target.proportion * other.targets[i].steady / 100
                                         : other.targets[i].steady;
    }
    return slopes;
}
// Silence before and after the speech (rsynth's END element).
constexpr KlattElement boundary{"END",
                                31,
                                5,
                                5,
                                false,
                                {{{270, 135, 50, 3, 3},
                                  {490, 0, 100, 0, 0},
                                  {1480, 0, 100, 0, 0},
                                  {2500, 0, 100, 0, 0},
                                  {60, 0, 100, 0, 0},
                                  {90, 0, 100, 0, 0},
                                  {150, 0, 100, 0, 0},
                                  {-30, -10.5f, 100, 3, 0},
                                  {-30, -10.5f, 100, 3, 0},
                                  {-30, -10.5f, 100, 3, 0},
                                  {-30, -10.5f, 100, 3, 0},
                                  {-30, -10.5f, 100, 3, 0},
                                  {-30, 0, 100, 3, 0},
                                  {-30, 0, 100, 3, 0},
                                  {-30, 0, 100, 3, 0},
                                  {0, 0, 50, 0, 0},
                                  {0, 0, 50, 0, 0},
                                  {0, 0, 50, 0, 0},
                                  {0, 0, 50, 0, 0}}}};
class Synthesizer {
    Resonator nasal_pole, nasal_zero, formants[6], source_lowpass, output_lowpass;
    std::array<float, klatt_parameters> parameters{};
    // Glottal source, counted in oversampled samples.
    int period = oversampling, position = 0, open = 0, modulation = 0;
    float pulse_a = 0, pulse_b = 0, wave = 0;
    float voice_amplitude = 0, breath_amplitude = 0, aspiration = 0, frication = 0, bypass = 0;
    float voicing_db = 0, f0 = 0, decay = 0;
    float voice_last = 0, noise_last = 0, glottal_last = 0;
    const float noise_decay = rescaled(0.75f);
    uint32_t seed = 5;
    double seconds = 0;
    // Voicing parameters change pitch-synchronously, at the start of a glottal period.
    void next_period() {
        if (f0 > 0) {
            period = std::max(oversampling, static_cast<int>(std::lround(oversampling * rate / f0)));
            voice_amplitude = linear(voicing_db);
            modulation = voicing_db > 0 ? period / 2 : period;
            breath_amplitude = linear(parameters[klatt_avc]) * 0.1f;
            open = std::clamp(static_cast<int>(open_quotient * period), 40, std::max(40, period - 2));
            // Derivative of a k1 t^2 - k2 t^3 flow pulse; its peak stays constant as `open` changes.
            pulse_b = 1920000.0f / (static_cast<float>(open) * open);
            pulse_a = pulse_b * open / 3;
        } else {
            period = modulation = oversampling;
            voice_amplitude = breath_amplitude = pulse_a = pulse_b = 0;
        }
    }
    float natural_source() {
        if (position >= open) {
            wave = 0;
            return 0;
        }
        pulse_a -= pulse_b;
        wave += pulse_a;
        return wave * 0.028f;
    }

public:
    Synthesizer() {
        // Smooths the oversampled source; set relative to the output rate as in rsynth.
        source_lowpass.set(0.095f * rate, 0.063f * rate);
        // A two-pole low-pass at 0 Hz; rsynth's bandwidth of the full rate put its 3 dB point at
        // 12 kHz here, leaving hiss above 8 kHz that natural recordings of this voice lack.
        output_lowpass.set(0, 0.75f * rate, linear(59));
        nasal_pole.set(270, 100);
        decay = rescaled(0.033f * tilt_db);
    }
    void render(float pitch, const std::array<float, klatt_parameters>& values, std::vector<float>& output) {
        parameters = values;
        seconds += subframe_samples / rate;
        const auto t = static_cast<float>(seconds);
        f0 = pitch * (1 + flutter / 50 / 100 *
                              (std::sin(2 * pi * 12.7f * t) + std::sin(2 * pi * 7.1f * t) +
                               std::sin(2 * pi * 4.7f * t)));
        voicing_db = std::max(0.0f, values[klatt_av] - 7);
        aspiration = linear(values[klatt_asp]) * 0.05f;
        frication = linear(values[klatt_af]) * 0.25f;
        const auto& gains = klatt_branch_gains;
        // Its "TTS_AMP_ADJ" lifts the table amplitudes to Klatt levels.
        constexpr float adjust = 14;
        for (int i = 0; i < 3; ++i)
            formants[i].set(values[klatt_f1 + i], values[klatt_b1 + i],
                            gains[i] * linear(adjust + values[klatt_a1 + i]));
        for (int i = 3; i < 6; ++i)
            formants[i].set(upper_formants[i - 3][0], upper_formants[i - 3][1],
                            gains[i] * linear(adjust + values[klatt_a1 + i]));
        bypass = linear(adjust + values[klatt_ab]) * 0.05f;
        nasal_zero.set_anti(values[klatt_fn], 100);
        for (int n = 0; n < subframe_samples; ++n) {
            seed = seed * 1664525 + 1;
            const auto random = static_cast<float>(static_cast<int32_t>(seed) >> 18);
            float noise = random + noise_decay * noise_last;
            noise_last = noise;
            // Less noise in the second half of a voiced period.
            if (position > modulation)
                noise *= 0.5f;
            float source = frication * noise;
            // Averaging the oversampled source, rather than keeping every fourth value as rsynth
            // did, stops the pulse's sharp closure from aliasing into a gritty high-frequency floor.
            float voice = 0;
            for (int k = 0; k < oversampling; ++k) {
                float value = natural_source();
                if (position >= period) {
                    position = 0;
                    next_period();
                }
                voice += source_lowpass.resonate(value) / oversampling;
                ++position;
            }
            voice = voice * (1 - decay) + voice_last * decay;
            voice_last = voice;
            if (position < open)
                voice += breath_amplitude * random;
            // rsynth feeds the source through the nasal resonators, then F1 on its own.
            const float glottal =
                nasal_pole.resonate(nasal_zero.antiresonate(voice_amplitude * voice + aspiration * noise));
            float out = formants[0].resonate(glottal);
            // F2-F6 and the bypass take frication plus the differentiated voicing, with alternating sign.
            source += glottal - glottal_last;
            glottal_last = glottal;
            for (int i = 5; i >= 1; --i)
                out = formants[i].resonate(source) - out;
            out = bypass * source - out;
            out = output_lowpass.resonate(out);
            output.push_back(std::clamp(out, -32767.0f, 32767.0f) / 32768);
        }
    }
};
} // namespace
std::vector<float> klatt_synthesize(std::span<const KlattSegment> segments,
                                    const std::function<float(float)>& pitch) {
    Synthesizer synthesizer;
    std::vector<float> output;
    std::array<float, klatt_parameters> values{};
    float elapsed = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (segment.frames <= 0)
            continue; // still shapes its neighbours' transitions
        const auto& current = *segment.element;
        const auto& previous = i ? *segments[i - 1].element : boundary;
        const auto& next = i + 1 < segments.size() ? *segments[i + 1].element : boundary;
        const auto start = current.rank > previous.rank ? transition(current, previous, false)
                                                        : transition(previous, current, true);
        const auto end =
            next.rank > current.rank ? transition(next, current, true) : transition(current, next, false);
        const int subframes =
            std::max(1, static_cast<int>(std::lround(segment.frames * subframes_per_frame)));
        for (int k = 0; k < subframes; ++k) {
            const float t = static_cast<float>(k) / subframes_per_frame;
            for (int p = 0; p < klatt_parameters; ++p)
                values[p] = interpolate(start[p], end[p], current.targets[p].steady, t, segment.frames);
            synthesizer.render(pitch(elapsed + t), values, output);
        }
        elapsed += segment.frames;
    }
    return output;
}
} // namespace ortho
