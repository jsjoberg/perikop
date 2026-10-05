#pragma once
#include "core/model.hpp"
namespace ortho {
struct SpeechUtterance { std::string display_text, speech_text, language; };
SpeechUtterance make_utterance(const std::string& text, const std::string& language, const std::vector<Pronunciation>& lexicon);
std::string reading_introduction(const Reading&);
class SpeechEngine {
public:
    virtual ~SpeechEngine() = default;
    virtual void speak(const SpeechUtterance&) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
};
// This diagnostic engine accepts utterances; it produces no audio.
class StubSpeechEngine final : public SpeechEngine {
public:
    enum class State { Idle, Accepted, Paused };
    void speak(const SpeechUtterance& utterance) override { accepted.push_back(utterance); state = State::Accepted; }
    void pause() override { if (state==State::Accepted) state=State::Paused; }
    void resume() override { if (state==State::Paused) state=State::Accepted; }
    void stop() override { accepted.clear(); state=State::Idle; }
    std::vector<SpeechUtterance> accepted;
    State state = State::Idle;
};
}
