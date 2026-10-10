#pragma once
#include "core/model.hpp"
#include <functional>
#include <memory>
#include <utility>
namespace ortho {
enum class SpeechState { Idle, Loading, Buffering, Playing, Paused, Stopped, Completed, Error };
constexpr bool speech_active(SpeechState state) {
    return state == SpeechState::Loading || state == SpeechState::Buffering ||
           state == SpeechState::Playing || state == SpeechState::Paused;
}
struct SpeechCue {
    size_t reading = 0, section = 0;
    std::string book, source;
    VerseRef verse, last;
    bool introduction = false;
    bool operator==(const SpeechCue&) const = default;
};
struct SpeechPlayback {
    SpeechState state = SpeechState::Idle;
    std::optional<SpeechCue> cue;
    double verse_progress = 0, progress = 0;
    // While buffering: share of the audio needed before playback can run.
    double ready = 0;
};
struct SpeechUtterance {
    std::string display_text, speech_text, language;
    std::optional<SpeechCue> cue = {};
};
// Compile pronunciation phrases once and reuse them for every verse in a batch.
class PronunciationMatcher {
public:
    PronunciationMatcher(std::string language, std::vector<Pronunciation> lexicon);
    SpeechUtterance utterance(const std::string& text) const;

private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
SpeechUtterance make_utterance(const std::string& text, const std::string& language,
                               const std::vector<Pronunciation>& lexicon);
// The spoken announcement of every passage of a reading, each with its book's Swedish name.
// A repeated book or chapter is not named again.
std::string reading_introduction(const std::vector<std::pair<std::string, Passage>>& parts);
std::vector<std::string> speech_chunks(const std::string&);
double speech_text_weight(const std::string&);
std::string pronunciation_key(const std::string&);
bool contains_speech_word(const std::string& text, const std::string& word);
class SpeechEngine {
public:
    virtual ~SpeechEngine() = default;
    virtual void speak(const SpeechUtterance&) = 0;
    virtual void speak_batch(const std::vector<SpeechUtterance>& utterances) {
        for (const auto& utterance : utterances)
            speak(utterance);
    }
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual SpeechPlayback playback() const {
        return {};
    }
    // Playback speed; 1 is the voice's own pace. Applies to audio already generated.
    virtual void set_speed(double) {}
    // Swedish voice: "alice" or "bjorn". Applies from the next reading.
    virtual void set_voice(const std::string&) {}
    // IPA the Swedish voice uses for speech text; empty when it is unavailable.
    virtual std::string pronunciation(const std::string&) {
        return {};
    }
    // Delivery may run on a worker. UI callers must dispatch results to the UI thread.
    // A newer request may replace a pending lookup without delivering its result.
    // NOLINTNEXTLINE(performance-unnecessary-value-param): worker implementations retain the callback.
    virtual void pronunciation_async(const std::string& text, std::function<void(std::string)> deliver) {
        deliver(pronunciation(text));
    }
};
// This diagnostic engine accepts utterances; it produces no audio.
class StubSpeechEngine final : public SpeechEngine {
public:
    enum class State { Idle, Accepted, Paused };
    void speak(const SpeechUtterance& utterance) override {
        accepted.push_back(utterance);
        state = State::Accepted;
    }
    void pause() override {
        if (state == State::Accepted)
            state = State::Paused;
    }
    void resume() override {
        if (state == State::Paused)
            state = State::Accepted;
    }
    void stop() override {
        accepted.clear();
        state = State::Idle;
    }
    std::vector<SpeechUtterance> accepted;
    State state = State::Idle;
};
} // namespace ortho
