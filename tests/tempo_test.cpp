#include "speech/tempo.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
// Rising zero crossings per second estimate the pitch of a pure tone.
double frequency(const std::vector<float>& samples) {
    int crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] < 0 && samples[i] >= 0)
            ++crossings;
    return crossings * 24000.0 / samples.size();
}
std::vector<float> play(float speed, std::vector<float> tone) {
    ortho::PcmStream stream;
    stream.append(std::move(tone));
    stream.complete();
    ortho::TempoStretch stretch;
    std::vector<float> heard;
    float block[480];
    for (int guard = 0; guard < 1000; ++guard) {
        const auto count = stretch.render(stream, block, 480, speed);
        if (!count && stretch.finished())
            break;
        heard.insert(heard.end(), block, block + count);
    }
    return heard;
}
void reused_address() {
    alignas(ortho::PcmStream) std::byte storage[sizeof(ortho::PcmStream)];
    ortho::TempoStretch stretch;
    float block[256];
    for (int iteration = 0; iteration < 12; ++iteration) {
        auto* stream = std::construct_at(reinterpret_cast<ortho::PcmStream*>(storage));
        const float sample = iteration % 2 ? -0.5f : 0.5f;
        stream->append(std::vector<float>(24000, sample));
        stream->complete();
        const auto count = stretch.render(*stream, block, 256, iteration % 3 ? 0.5f : 1.0f);
        check(count == 256, "Replacement stream produces audio");
        for (size_t i = 0; i < count; ++i)
            check(std::abs(block[i] - sample) < 0.01f, "Reused address must not replay a previous reading");
        std::destroy_at(stream);
    }
}
void transitions(float first, float middle, float last) {
    ortho::PcmStream stream;
    std::vector<float> samples(96000, -0.5f);
    std::fill_n(samples.begin(), 48000, 0.5f);
    stream.append(std::move(samples));
    stream.complete();
    ortho::TempoStretch stretch;
    float block[256];
    bool negative = false, changed = false;
    size_t positive_frames = 0, negative_frames = 0, after_change = 0;
    uint64_t previous = 0;
    for (int guard = 0; guard < 3000; ++guard) {
        const auto speed = !changed ? first : after_change < 4096 ? middle : last;
        const auto count = stretch.render(stream, block, 256, speed);
        check(stretch.played() >= previous, "Delivered progress must be monotonic");
        previous = stretch.played();
        if (!stretch.finished())
            check(stretch.played() < 96000, "Final progress waits for tempo output");
        for (size_t i = 0; i < count; ++i) {
            if (block[i] < -0.4f) {
                negative = true;
                ++negative_frames;
            }
            if (block[i] > 0.4f) {
                check(!negative, "Speed changes must not replay earlier sections");
                ++positive_frames;
            }
        }
        if (negative && !changed)
            changed = true;
        else if (changed)
            after_change += count;
        if (!count && stretch.finished())
            break;
    }
    check(stretch.finished(), "Changing speed must drain all tempo output");
    check(positive_frames > 10000 && negative_frames > 10000, "Both source sections survive rate changes");
    check(stretch.played() == 96000, "Drained progress reaches the exact source endpoint");
}
void completion(float speed) {
    ortho::PcmStream stream;
    stream.append(std::vector<float>(2400, 0.5f));
    stream.complete();
    ortho::TempoStretch stretch;
    float block[256];
    bool pending_tail = false;
    size_t heard = 0;
    for (int guard = 0; guard < 100; ++guard) {
        const auto count = stretch.render(stream, block, 256, speed);
        heard += count;
        if (stream.finished() && !stretch.finished()) {
            pending_tail = true;
            check(stretch.played() < 2400, "Read-ahead does not finish progress");
        }
        if (stretch.finished()) {
            check(stretch.render(stream, block, 256, speed) == 0,
                  "Flush runs once without an audible extra tail");
            break;
        }
    }
    check(stretch.finished() && heard > 0, "Every supported rate finishes after output drain");
    if (speed == 0.25f)
        check(pending_tail && heard > 8000,
              "Quarter speed retains its audible tail after source consumption");
}
} // namespace
int main() {
    try {
        std::vector<float> tone(48000);
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.5f * std::sin(2 * std::numbers::pi * 220 * i / 24000.0);
        const auto normal = play(1.0f, tone), slow = play(0.8f, tone);
        check(normal.size() == tone.size(), "Normal speed plays every sample unchanged");
        check(std::abs(double(slow.size()) / tone.size() - 1.25) < 0.03, "80 % speed lasts 25 % longer");
        check(std::abs(frequency(slow) - 220) < 5, "Slower playback keeps the voice's pitch");
        reused_address();
        transitions(0.5f, 1.0f, 0.5f);
        transitions(0.25f, 2.0f, 0.25f);
        transitions(2.0f, 0.25f, 2.0f);
        for (const auto speed : {0.25f, 0.5f, 0.75f, 0.8f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f})
            completion(speed);
        std::cout << "Tempo checks passed: " << slow.size() << " frames at 80 %, " << frequency(slow)
                  << " Hz\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
