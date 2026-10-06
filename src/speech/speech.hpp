#pragma once
#include "core/model.hpp"
namespace ortho {
enum class SpeechState { Idle, Loading, Buffering, Playing, Paused, Stopped, Completed, Error };
struct SpeechCue {
    size_t reading=0,section=0;
    std::string book,source;
    VerseRef verse,last;
    bool introduction=false;
    bool operator==(const SpeechCue&) const = default;
};
struct SpeechPlayback {
    SpeechState state=SpeechState::Idle;
    std::optional<SpeechCue> cue;
    double verse_progress=0,progress=0;
    // While buffering: share of the audio needed before playback can run.
    double ready=0;
};
struct SpeechUtterance { std::string display_text, speech_text, language; std::optional<SpeechCue> cue={}; };
SpeechUtterance make_utterance(const std::string& text, const std::string& language, const std::vector<Pronunciation>& lexicon);
std::string reading_introduction(const Reading&);
std::vector<std::string> speech_chunks(const std::string&);
double speech_text_weight(const std::string&);
std::string pronunciation_key(const std::string&);
bool contains_speech_word(const std::string& text,const std::string& word);
class SpeechEngine {
public:
    virtual ~SpeechEngine() = default;
    virtual void speak(const SpeechUtterance&) = 0;
    virtual void speak_batch(const std::vector<SpeechUtterance>& utterances) {for(const auto& utterance:utterances)speak(utterance);}
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual SpeechPlayback playback() const { return {}; }
    // Playback speed; 1 is the voice's own pace. Applies to audio already generated.
    virtual void set_speed(double) {}
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
