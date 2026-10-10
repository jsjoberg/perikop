#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>
namespace ortho {
// One synthesis producer and one audio consumer. The producer retires played
// chunks; the audio callback neither locks nor frees buffers.
// Output starts held. The producer releases it once enough audio is buffered;
// an underrun holds it again so playback rebuffers instead of stuttering.
class PcmStream {
public:
    static constexpr size_t capacity = 1024;

private:
    std::array<std::vector<float>, capacity> chunks_;
    std::atomic<size_t> read_{0}, written_{0};
    std::atomic<bool> complete_{false}, held_{true};
    std::atomic<uint64_t> played_{0}, appended_{0};
    std::atomic<uint64_t> retained_{0};
    size_t retired_ = 0;  // Synthesis producer only.
    size_t position_ = 0; // Audio consumer only.
public:
    size_t buffered() const {
        const auto read = read_.load(std::memory_order_acquire);
        return written_.load(std::memory_order_acquire) - read;
    }
    uint64_t buffered_frames() const {
        return appended_.load(std::memory_order_acquire) - played_.load(std::memory_order_acquire);
    }
    uint64_t retained_frames() const {
        return retained_.load(std::memory_order_relaxed);
    }
    bool append(std::vector<float>&& samples) {
        const auto written = written_.load(std::memory_order_relaxed);
        const auto read = read_.load(std::memory_order_acquire);
        if (complete_.load(std::memory_order_acquire) || samples.empty() || written - read == capacity)
            return false;
        // Publication of read means the consumer has finished touching these chunks.
        // Release their allocations here instead of retaining an entire ring's history.
        while (retired_ < read) {
            auto& chunk = chunks_[retired_++ % capacity];
            retained_.fetch_sub(chunk.size(), std::memory_order_relaxed);
            std::vector<float>().swap(chunk);
        }
        const auto frames = samples.size();
        chunks_[written % capacity] = std::move(samples);
        appended_.fetch_add(frames, std::memory_order_release);
        retained_.fetch_add(frames, std::memory_order_relaxed);
        written_.store(written + 1, std::memory_order_release);
        return true;
    }
    void release() {
        held_.store(false, std::memory_order_release);
    }
    bool held() const {
        return held_.load(std::memory_order_acquire);
    }
    void complete() {
        complete_.store(true, std::memory_order_release);
        held_.store(false, std::memory_order_release);
    }
    bool finished() const {
        return complete_.load(std::memory_order_acquire) && buffered() == 0;
    }
    uint64_t played() const {
        return played_.load(std::memory_order_acquire);
    }
    bool waiting() const {
        return !complete_.load(std::memory_order_acquire) && (held() || buffered() == 0);
    }
    // Returns the number of frames copied; the rest is silence.
    size_t render(float* output, size_t frames) {
        std::fill_n(output, frames, 0.0f);
        if (held())
            return 0;
        size_t copied = 0;
        auto read = read_.load(std::memory_order_relaxed);
        while (copied < frames && read < written_.load(std::memory_order_acquire)) {
            const auto& chunk = chunks_[read % capacity];
            const auto count = std::min(frames - copied, chunk.size() - position_);
            std::memcpy(output + copied, chunk.data() + position_, count * sizeof(float));
            copied += count;
            position_ += count;
            if (position_ == chunk.size()) {
                position_ = 0;
                read_.store(++read, std::memory_order_release);
            }
        }
        played_.fetch_add(copied, std::memory_order_release);
        if (copied < frames && !complete_.load(std::memory_order_acquire))
            held_.store(true, std::memory_order_release);
        return copied;
    }
};
// Generating the rest must finish before playback reaches it. With a
// real-time factor r (seconds of work per second of audio) and R seconds
// still to generate, playback may run once B >= (r - 1) * R is buffered.
// The margin covers estimation error; the cushion covers chunk granularity.
inline double buffer_target(double remaining_seconds, double realtime_factor, double next_seconds = 0) {
    // Cache playback only needs a small disk-read lead. Fast synthesis needs
    // two seconds; keep the original cushion when synthesis cannot keep up.
    if (remaining_seconds <= 0)
        return 0.25;
    constexpr double margin = 1.25;
    // Even a fast model must finish a whole chunk before it can append audio.
    const double cushion =
        std::max(realtime_factor * margin <= 1 ? 2.0 : 6.0, next_seconds * realtime_factor * margin);
    return std::max(0.0, (realtime_factor * margin - 1) * remaining_seconds) + cushion;
}
// Fast models need a short rolling lead. Slower models retain their startup
// requirement, up to the existing ten-minute memory bound.
inline double buffer_limit(double remaining_seconds, double realtime_factor, double next_seconds = 0) {
    return std::clamp(buffer_target(remaining_seconds, realtime_factor, next_seconds), 30.0, 600.0);
}
} // namespace ortho
