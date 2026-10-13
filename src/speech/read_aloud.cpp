#include "speech/read_aloud.hpp"
#include "core/reading_display.hpp"

namespace ortho {
void ReadAloud::start(std::vector<Reading> readings) {
    readings_ = std::move(readings);
}
void ReadAloud::update(const SpeechPlayback& playback, Clock::time_point now) {
    if (playback.state == SpeechState::Buffering && playback_.state != SpeechState::Buffering) {
        buffering_since_ = now;
        debounce_buffering_ = playback_.state == SpeechState::Playing && playback.cue.has_value();
    } else if (playback.state != SpeechState::Buffering)
        debounce_buffering_ = false;
    playback_ = playback;
    // The view freezes tracking immediately, but the status does not flash buffering
    // text for a brief gap between audio callbacks. Startup feedback remains immediate.
    feedback_ = playback.state == SpeechState::Buffering && debounce_buffering_ &&
                        now - buffering_since_ < std::chrono::milliseconds(180)
                    ? SpeechState::Playing
                    : playback.state;
}
std::optional<ReadAloud::Status> ReadAloud::status(const CorpusDb& corpus) const {
    std::string location;
    if (playback_.cue) {
        const auto& cue = *playback_.cue;
        location = passage_label(corpus, {{cue.book, cue.verse, cue.verse}});
    }
    const std::string ready =
        playback_.ready > 0 ? " " + std::to_string(int(playback_.ready * 100)) + " %" : std::string{};
    switch (feedback_) {
    case SpeechState::Loading:
        return Status{"Laddar rösten…"};
    case SpeechState::Buffering:
        return Status{
            (playback_.cue ? "Förbereder fortsättningen…" : "Förbereder uppläsningen…") + ready,
            "Uppläsningen startar när tillräckligt mycket ljud är klart för att den inte ska stanna."};
    case SpeechState::Playing:
        return Status{location};
    case SpeechState::Paused:
        return Status{location.empty() ? std::string("Pausad") : "Pausad, " + location};
    case SpeechState::Error:
        return Status{"Uppläsningen kunde inte fortsätta", message_};
    case SpeechState::Stopped:
    case SpeechState::Completed:
    case SpeechState::Idle:
        break;
    }
    return std::nullopt;
}
} // namespace ortho
