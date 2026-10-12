#pragma once

#include "speech/speech.hpp"
#include <chrono>

namespace ortho {
// The queue being read aloud and how its progress is described. The window
// keeps the engine, the timer, and which reading the view shows.
class ReadAloud {
public:
    using Clock = std::chrono::steady_clock;
    struct Status {
        std::string title;
        std::string tooltip = {};
    };
    // Readings as opened, retained for playback navigation.
    void start(std::vector<Reading> readings);
    void update(const SpeechPlayback&, Clock::time_point now = Clock::now());
    // The engine's latest message, shown when playback fails.
    void report(std::string message) {
        message_ = std::move(message);
    }
    const std::vector<Reading>& readings() const {
        return readings_;
    }
    const SpeechPlayback& playback() const {
        return playback_;
    }
    // The state to show: a brief buffering gap during playback still reads as playing.
    SpeechState feedback() const {
        return feedback_;
    }
    // One short line for the address field; nothing when no reading is in progress.
    std::optional<Status> status(const CorpusDb&) const;

private:
    std::vector<Reading> readings_;
    SpeechPlayback playback_;
    SpeechState feedback_ = SpeechState::Idle;
    Clock::time_point buffering_since_;
    bool debounce_buffering_ = false;
    std::string message_;
};
} // namespace ortho
