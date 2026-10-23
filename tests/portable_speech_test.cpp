#include "speech/espeak_voice.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/portable_speech.hpp"
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {
// A manually drained audio device and a scripted synthesizer let the real speech worker run
// without sound, eSpeak, or timing assumptions about physical playback.
std::mutex audio_mutex;
std::condition_variable changed;
std::shared_ptr<ortho::PcmStream> stream;
ortho::PcmProgress delivered;
size_t appended = 0;
double synthesized_speed = 0;
bool paused = false, released = false, completed = false;
bool hold_device = false, device_entered = false;
std::string status;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Predicate> void wait_for(Predicate predicate, const char* message) {
    std::unique_lock lock(audio_mutex);
    check(changed.wait_for(lock, std::chrono::seconds(3), predicate), message);
}
std::vector<float> drain(size_t frames = 6000) {
    std::vector<float> samples(frames);
    {
        std::lock_guard lock(audio_mutex);
        if (stream && !paused) {
            stream->render(samples.data(), samples.size());
            delivered = {stream->played(), stream->waiting(), stream->finished()};
        }
    }
    changed.notify_all();
    std::erase(samples, 0.0f);
    return samples;
}
} // namespace
namespace ortho {
struct PcmOutput::Impl {};
PcmOutput::PcmOutput() : impl_(std::make_unique<Impl>()) {
    std::unique_lock lock(audio_mutex);
    device_entered = true;
    changed.notify_all();
    changed.wait(lock, [] {
        return !hold_device;
    });
}
PcmOutput::~PcmOutput() = default;
void PcmOutput::start(bool pause) {
    std::lock_guard lock(audio_mutex);
    stream = std::make_shared<PcmStream>();
    delivered = {};
    paused = pause;
    appended = 0;
    released = false;
    completed = false;
}
void PcmOutput::append(std::vector<float> samples) {
    std::lock_guard lock(audio_mutex);
    check(stream && stream->append(std::move(samples)),
          "Speech worker overfilled or closed the audio buffer");
    ++appended;
    changed.notify_all();
}
void PcmOutput::complete() {
    std::lock_guard lock(audio_mutex);
    stream->complete();
    completed = true;
    changed.notify_all();
}
size_t PcmOutput::buffered() const {
    std::lock_guard lock(audio_mutex);
    return stream ? stream->buffered() : 0;
}
uint64_t PcmOutput::buffered_frames() const {
    std::lock_guard lock(audio_mutex);
    return stream ? stream->buffered_frames() : 0;
}
void PcmOutput::release() {
    std::lock_guard lock(audio_mutex);
    released = true;
    if (stream)
        stream->release();
    changed.notify_all();
}
void PcmOutput::pause(bool pause) {
    std::lock_guard lock(audio_mutex);
    paused = pause;
    changed.notify_all();
}
void PcmOutput::stop() {
    std::lock_guard lock(audio_mutex);
    stream.reset();
    paused = false;
    changed.notify_all();
}
bool PcmOutput::finished() const {
    std::lock_guard lock(audio_mutex);
    return !stream || delivered.finished;
}
PcmProgress PcmOutput::progress() const {
    std::lock_guard lock(audio_mutex);
    return stream ? delivered : PcmProgress{};
}
} // namespace ortho
int main(int argc, char** argv) {
    try {
        using namespace ortho;
        check(argc == 2, "Usage: ortho-portable-speech-tests SPEECH-DATA");
        const std::filesystem::path speech_data(argv[1]);
        // Known audio per text; any other text fails synthesis deterministically.
        const std::map<std::string, std::vector<float>> audio = {
            {"Första.", {1, 2, 3}},
            {"Andra.", {4, 5, 6}},
            {"Tredje.", {7, 8, 9}},
            {"Ny läsning.", {10, 11, 12}},
            {"Start.", std::vector<float>(24000, 1.0f)},
            {"En längre fortsättning.", std::vector<float>(size_t{24000} * 30, 2.0f)}};
        const auto synthesis = [&audio](const std::string& text, const std::string&, double speed,
                                        const std::function<bool()>&) {
            {
                std::lock_guard lock(audio_mutex);
                synthesized_speed = speed;
            }
            const auto found = audio.find(text);
            if (found == audio.end())
                throw std::runtime_error("No scripted audio for " + text);
            return found->second;
        };
        auto engine = create_portable_speech(
            speech_data,
            [](const SpeechUpdate& update) {
                std::lock_guard lock(audio_mutex);
                status = update.text;
                changed.notify_all();
            },
            synthesis);
        const SpeechCue first{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}};
        const std::vector<SpeechUtterance> reading = {
            {"", "Första.", "sv", first},
            {"", "Andra.", "sv", SpeechCue{0, 0, "Ps", "sv1917", {23, 2}, {23, 2}}},
            {"", "Tredje.", "sv", SpeechCue{1, 1, "John", "sv1917", {1, 1}, {1, 1}}}};
        {
            std::lock_guard lock(audio_mutex);
            hold_device = true;
        }
        engine->speak_batch(reading);
        wait_for(
            [] {
                return device_entered;
            },
            "The worker must begin opening the audio device");
        auto controls = std::async(std::launch::async, [&] {
            engine->pause();
            return engine->playback().state;
        });
        const bool responsive = controls.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
        {
            std::lock_guard lock(audio_mutex);
            hold_device = false;
        }
        changed.notify_all();
        const auto held_state = controls.get();
        check(responsive && held_state == SpeechState::Paused,
              "Pause and playback polling must not wait for audio device initialization");
        wait_for(
            [] {
                return appended == 1;
            },
            "An in-flight chunk may finish while paused");
        {
            std::unique_lock lock(audio_mutex);
            check(!changed.wait_for(lock, std::chrono::milliseconds(150),
                                    [] {
                                        return appended > 1;
                                    }),
                  "Pause must suspend synthesis of later chunks");
        }
        engine->resume();
        wait_for(
            [] {
                return appended == 3;
            },
            "Resume must wake speech synthesis");
        engine->stop();

        // The lookup worker must deliver results without holding up its caller.
        std::promise<bool> lookup_result;
        auto lookup_done = lookup_result.get_future();
        const auto caller = std::this_thread::get_id();
        engine->pronunciation_async("begynnelsen", [&](const auto&) {
            lookup_result.set_value(std::this_thread::get_id() != caller);
        });
        check(lookup_done.wait_for(std::chrono::seconds(3)) == std::future_status::ready && lookup_done.get(),
              "Pronunciation lookup must run on a worker");
        engine->speak_batch(reading);
        wait_for(
            [] {
                return appended == 3 && stream && !stream->waiting();
            },
            "Synthesis must run ahead of playback and release finished audio");
        check(!engine->playback().cue, "Queued audio must not move the marker before playback");
        auto heard = drain(1);
        auto position = engine->playback();
        check(position.cue == first && position.verse_progress == 1.0 / 3 &&
                  position.state == SpeechState::Playing,
              "The worker must expose actual playback progress");
        engine->pause();
        check(drain().empty(), "Pause must preserve the playback position");
        auto held = engine->playback();
        check(held.cue == position.cue && held.verse_progress == position.verse_progress &&
                  held.state == SpeechState::Paused,
              "Pause freezes the marker on the audible verse");
        {
            std::lock_guard lock(audio_mutex);
            check(appended == 3 && status == "Pausad", "Pause must report the paused state");
        }
        engine->resume();
        // finished() reads the snapshot that drain() takes, so the worker must mark the end of the
        // stream before the last drain; it may do so after the test has already seen all chunks.
        wait_for(
            [] {
                return completed;
            },
            "The worker must mark the end of the reading");
        // Each later chunk begins with a 0.2 s pause, so drain twice.
        for (int i = 0; i < 2; ++i) {
            auto rest = drain();
            heard.insert(heard.end(), rest.begin(), rest.end());
        }
        wait_for(
            [] {
                return status == "Läsningen är klar";
            },
            "Completion must wait for final audio consumption");
        check(heard == std::vector<float>{1, 2, 3, 4, 5, 6, 7, 8, 9},
              "The worker must play all chunks in order");

        engine->speak_batch(reading);
        wait_for(
            [] {
                return appended == 3 && !stream->finished();
            },
            "Second reading is generated ahead");
        engine->pause();
        engine->stop();
        {
            std::lock_guard lock(audio_mutex);
            check(!stream && status == "Stoppad", "Stop must discard all queued audio");
        }
        check(!engine->playback().cue && engine->playback().state == SpeechState::Stopped,
              "Stop clears the marker and playback state");
        engine->speak({"", "Ny läsning.", "sv"});
        wait_for(
            [] {
                // The first playing report can precede release of a short reading.
                return stream && appended == 1 && !stream->waiting() && status.starts_with("Läser");
            },
            "A new reading must start after cancellation");
        check(drain() == std::vector<float>{10, 11, 12},
              "Cancelled audio and paused state must not leak into a new reading");
        wait_for(
            [] {
                return status == "Läsningen är klar";
            },
            "The replacement reading must complete");

        engine->speak_batch({{"", "Start.", "sv"}, {"", "En längre fortsättning.", "sv"}});
        wait_for(
            [] {
                return appended == 2 && stream && !stream->waiting();
            },
            "Fast synthesis must start without waiting for the whole reading");
        engine->stop();

        // Speed reaches the synthesizer for text read after the change.
        engine->set_speed(1.5);
        engine->speak({"", "Ny läsning.", "sv"});
        wait_for(
            [] {
                return appended == 1 && synthesized_speed == 1.5;
            },
            "A speed change must apply to the next synthesis");
        engine->stop();
        engine->set_speed(1);

        // A short first chunk must not start playback before enough audio exists;
        // the second text then fails synthesis deterministically.
        engine->speak_batch({{"", "Start.", "sv"}, {"", "Ej skriptad.", "sv"}});
        wait_for(
            [] {
                return status.starts_with("Uppläsning:");
            },
            "A later synthesis failure must report an error");
        {
            std::lock_guard lock(audio_mutex);
            check(appended == 1 && !released && !stream,
                  "Playback must wait for enough audio, and errors must stop playback");
        }
        engine.reset();

        // The real eSpeak voice, where the user has installed it, with only the bundled pronunciations.
        if (!espeak_installed()) {
            check(!std::getenv("PERIKOP_REQUIRE_ESPEAK"), "CI requires an installed eSpeak NG library");
            std::cout << "eSpeak NG playback skipped: not installed\n";
        } else {
            {
                std::lock_guard lock(audio_mutex);
                status.clear();
            }
            auto voice = create_portable_speech(speech_data, [](const SpeechUpdate& update) {
                std::lock_guard lock(audio_mutex);
                status = update.text;
                changed.notify_all();
            });
            voice->set_voice(espeak_female_id);
            voice->speak({"", "Herren är min herde, mig skall intet fattas.", "sv"});
            wait_for(
                [] {
                    return stream && appended == 1 && !stream->waiting();
                },
                "eSpeak must read with the bundled pronunciations");
            const auto spoken = drain(24000);
            check(std::ranges::any_of(spoken,
                                      [](float sample) {
                                          return std::abs(sample) > 0.01f;
                                      }),
                  "eSpeak audio is audible");
            voice.reset();
        }
        std::cout << "Portable speech streaming checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
