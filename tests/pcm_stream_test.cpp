#include "speech/pcm_stream.hpp"
#include "speech/playback_timeline.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        ortho::PcmStream stream;
        float output[4];
        check(!stream.finished(), "An empty buffer must wait for synthesis, not finish playback");
        check(stream.append({1, 2, 3}), "First chunk accepted");
        check(stream.buffered_frames() == 3, "Buffered audio is counted in frames");
        stream.render(output, 2);
        check(output[0] == 0 && output[1] == 0 && stream.played() == 0 && stream.waiting(),
              "Output stays held until the producer releases it");
        stream.release();
        stream.render(output, 2);
        check(output[0] == 1 && output[1] == 2, "Playback starts before later chunks or completion");
        check(stream.played() == 2 && !stream.waiting() && stream.buffered_frames() == 1,
              "Progress follows consumed PCM, not queued audio");
        stream.render(output, 4);
        check(output[0] == 3 && output[1] == 0 && output[2] == 0 && output[3] == 0,
              "Underrun supplies silence after the available samples");
        check(!stream.finished(), "Underrun must not end an unfinished reading");
        check(stream.played() == 3 && stream.waiting() && stream.held(),
              "Underrun holds playback so it can rebuffer");
        check(stream.retained_frames() == 3,
              "The audio callback must leave buffer reclamation to the producer");
        check(stream.append({4, 5}) && stream.append({6, 7}), "Chunks queue during an underrun");
        check(stream.retained_frames() == 4,
              "The producer must release played audio instead of retaining the ring's history");
        stream.render(output, 4);
        check(output[0] == 0 && stream.played() == 3, "Rebuffering does not play a trickle of audio");
        stream.release();
        stream.render(output, 4);
        check(std::vector<float>(output, output + 4) == std::vector<float>{4, 5, 6, 7},
              "Resume after underrun preserves order across chunk boundaries");
        check(stream.append({8, 9}), "Consumed slots can be reused");
        stream.complete();
        check(!stream.finished() && !stream.append({10}),
              "Completion drains queued audio and rejects new chunks");
        stream.render(output, 4);
        check(output[0] == 8 && output[1] == 9 && output[2] == 0 && stream.finished(),
              "Finish only after the final samples play");
        check(stream.played() == 9 && !stream.waiting(), "Final padding silence must not advance playback");

        ortho::PcmStream full;
        for (size_t i = 0; i < ortho::PcmStream::capacity; ++i)
            check(full.append({1}), "Deep lookahead accepted");
        std::vector<float> pending{2};
        // NOLINTNEXTLINE(bugprone-use-after-move): append moves only when it succeeds.
        check(!full.append(std::move(pending)) && pending == std::vector<float>{2},
              "Full buffer preserves the pending chunk");
        full.release();
        full.render(output, 1);
        check(full.append(std::move(pending)), "A played slot is reused");

        check(ortho::buffer_target(0, 2.3) == 6, "Nothing left to generate needs only the cushion");
        check(std::abs(ortho::buffer_target(100, 2.3) - 193.5) < 1e-9,
              "Slow synthesis buffers enough to finish before playback catches up");
        check(ortho::buffer_target(100, 0.5) == 6,
              "Faster-than-real-time synthesis starts after the cushion");
        check(ortho::buffer_limit(1000, 0.5) == 30,
              "Fast synthesis must use a rolling lead instead of preparing ten minutes ahead");
        check(std::abs(ortho::buffer_limit(100, 2.3) - 193.5) < 1e-9 && ortho::buffer_limit(1000, 2.3) == 600,
              "Slow synthesis must retain enough startup audio within the memory bound");
        check(ortho::buffer_limit(100, 0.7) < ortho::buffer_limit(100, 0.7 * 2),
              "Faster playback must increase preparation when synthesis cannot keep up");

        ortho::PlaybackTimeline timeline;
        const ortho::SpeechCue first{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}, false};
        const ortho::SpeechCue second{0, 1, "Ps", "sv1917", {23, 2}, {23, 2}, false};
        timeline.reset(3);
        timeline.append(first, 10, 0, 0, 1);
        timeline.append(second, 14, 4, 0, 0.5);
        timeline.append(second, 14, 4, 0.5, 1);
        check(!timeline.at(0, ortho::SpeechState::Buffering).cue, "No marker before the first audio sample");
        auto position = timeline.at(5, ortho::SpeechState::Playing);
        check(position.cue == first && position.verse_progress == 0.5,
              "Marker maps actual playback to its spoken verse");
        position = timeline.at(12, ortho::SpeechState::Buffering);
        check(position.cue == first && position.verse_progress == 1,
              "An inter-chunk gap holds the preceding verse");
        position = timeline.at(15, ortho::SpeechState::Playing);
        check(position.cue == second && position.verse_progress == 0.05,
              "The next verse starts only when its audio plays");
        position = timeline.at(26, ortho::SpeechState::Paused);
        check(position.cue == second && position.verse_progress == 0.5 &&
                  position.state == ortho::SpeechState::Paused,
              "Pause retains the current cue and split-verse position");
        position = timeline.at(38, ortho::SpeechState::Completed);
        check(position.cue == second && position.verse_progress == 1 && position.progress == 1,
              "The final sample completes the reading progress");
        timeline.reset(1);
        check(!timeline.at(5, ortho::SpeechState::Buffering).cue, "Restart discards old verse cues");

        // Exercise publication and slot reuse with actual concurrent producer
        // and consumer threads, including partial and multi-chunk callbacks.
        ortho::PcmStream concurrent;
        constexpr size_t chunks = 20000;
        std::atomic<bool> abort{false};
        std::thread producer([&] {
            size_t sample = 1;
            for (size_t i = 0; i < chunks && !abort.load(); ++i) {
                std::vector<float> samples(1 + i % 17);
                for (auto& value : samples)
                    value = static_cast<float>(sample++);
                // NOLINTNEXTLINE(bugprone-use-after-move): append moves only when it succeeds.
                while (!concurrent.append(std::move(samples)) && !abort.load())
                    std::this_thread::yield();
                concurrent.release();
            }
            concurrent.complete();
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        size_t expected = 1;
        bool ordered = true, timed_out = false;
        while (!concurrent.finished()) {
            float block[31];
            concurrent.render(block, 31);
            for (auto value : block)
                if (value != 0) {
                    if (value != static_cast<float>(expected))
                        ordered = false;
                    ++expected;
                }
            if (std::chrono::steady_clock::now() > deadline) {
                abort = true;
                timed_out = true;
                break;
            }
            std::this_thread::yield();
        }
        producer.join();
        size_t total = 0;
        for (size_t i = 0; i < chunks; ++i)
            total += 1 + i % 17;
        check(!timed_out && ordered && expected == total + 1,
              "Concurrent streaming must play every sample exactly once in order");
        std::cout << "PCM streaming checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
