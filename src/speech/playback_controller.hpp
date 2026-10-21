#pragma once

#include "speech/speech.hpp"
#include <chrono>

namespace ortho {
// Playback commands, queue, and progress. Native windows and timers stay in the UI.
class PlaybackController {
public:
    using Clock = std::chrono::steady_clock;
    using Location = std::pair<size_t, size_t>;
    struct Status {
        std::string title;
        std::string tooltip = {};
    };
    // Readings as opened, retained for playback navigation.
    void start(std::vector<Reading> readings);
    bool select_reading(SpeechEngine&);
    void leave_reader(SpeechEngine&);
    void toggle_pause(SpeechEngine&);
    void stop(SpeechEngine&);
    const std::optional<Location>& location() const {
        return location_;
    }
    void location(Location location) {
        location_ = location;
    }
    void forget_location() {
        location_.reset();
    }
    bool needs_location(Location location, bool reader) const {
        return !reader || location_ != location;
    }
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
    bool active() const {
        return speech_active(playback_.state);
    }
    bool polling() const {
        return active() && playback_.state != SpeechState::Paused;
    }
    // The state to show: a brief buffering gap during playback still reads as playing.
    SpeechState feedback() const {
        return feedback_;
    }
    // One short line for the address field; nothing when no reading is in progress.
    std::optional<Status> status(const CorpusDb&) const;

private:
    std::vector<Reading> readings_;
    std::optional<Location> location_;
    SpeechPlayback playback_;
    SpeechState feedback_ = SpeechState::Idle;
    Clock::time_point buffering_since_;
    bool debounce_buffering_ = false;
    std::string message_;
};
} // namespace ortho
