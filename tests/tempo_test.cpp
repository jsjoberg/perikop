#include "speech/tempo.hpp"
#include <cmath>
#include <iostream>
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
        if (!count && stream.finished())
            break;
        heard.insert(heard.end(), block, block + count);
    }
    return heard;
}
} // namespace
int main() {
    try {
        std::vector<float> tone(48000);
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.5f * std::sin(2 * M_PI * 220 * i / 24000.0);
        const auto normal = play(1.0f, tone), slow = play(0.8f, tone);
        check(normal.size() == tone.size(), "Normal speed plays every sample unchanged");
        check(std::abs(double(slow.size()) / tone.size() - 1.25) < 0.03, "80 % speed lasts 25 % longer");
        check(std::abs(frequency(slow) - 220) < 5, "Slower playback keeps the voice's pitch");
        std::cout << "Tempo checks passed: " << slow.size() << " frames at 80 %, " << frequency(slow)
                  << " Hz\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
