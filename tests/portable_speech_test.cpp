#include "speech/kokoro_audio.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/portable_speech.hpp"
#include "storage/database.hpp"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {
// A manually drained audio device lets the real speech worker run without
// sound, model inference, or timing assumptions about physical playback.
std::mutex audio_mutex;
std::condition_variable changed;
std::shared_ptr<ortho::PcmStream> stream;
size_t appended = 0;
bool paused = false, released = false;
bool first_chunk_released = false;
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
        if (stream && !paused)
            stream->render(samples.data(), samples.size());
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
    paused = pause;
    appended = 0;
    released = false;
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
void PcmOutput::set_speed(double) {}
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
    return !stream || stream->finished();
}
PcmProgress PcmOutput::progress() const {
    std::lock_guard lock(audio_mutex);
    return stream ? PcmProgress{stream->played(), stream->waiting(), stream->finished()} : PcmProgress{};
}
} // namespace ortho
int main(int argc, char** argv) {
    try {
        using namespace ortho;
        check(argc == 2, "Usage: ortho-portable-speech-tests TEST-DIRECTORY");
        const std::filesystem::path data(argv[1]);
        std::filesystem::remove_all(data);
        const auto voice = data / "voices" / kokoro_pack_id;
        std::filesystem::create_directories(voice);
        std::ofstream(voice / "kokoro.onnx") << "";
        // Cache-only tests: any accidental upfront inference fails because
        // the remaining model files deliberately do not exist.
        const std::string revision = std::string(kokoro_pack_id) + "/ort1.23.2/alice/v1";
        {
            SpeechCache cache(data / "speech-cache.db");
            cache.save(revision, "sv", "Första.", {1, 2, 3});
            cache.save(revision, "sv", "Andra.", {4, 5, 6});
            cache.save(revision, "sv", "Tredje.", {7, 8, 9});
            cache.save(revision, "sv", "Ny läsning.", {10, 11, 12});
            cache.save(revision, "sv", "Start.", std::vector<float>(24000, 1.0f));
            cache.save(revision, "sv", "En längre fortsättning som redan finns i ljudcachen.",
                       std::vector<float>(24000 * 30, 2.0f));
            check(cache.contains(revision, "sv", "Start.") && !cache.contains(revision, "sv", "Ej cachad.") &&
                      !cache.contains(revision, "en", "Start.") &&
                      !cache.contains(revision + "-other", "sv", "Start."),
                  "Cache availability must match the exact text, language, model and voice");
        }
        auto engine = create_portable_speech(data / "voices", data, [](const SpeechUpdate& update) {
            std::lock_guard lock(audio_mutex);
            status = update.text;
            if (appended == 1 && status == "Läser med Alice")
                first_chunk_released = released;
            changed.notify_all();
        });
        const SpeechCue first{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}, false};
        const std::vector<SpeechUtterance> reading = {
            {"", "Första.", "sv", first},
            {"", "Andra.", "sv", SpeechCue{0, 0, "Ps", "sv1917", {23, 2}, {23, 2}, false}},
            {"", "Tredje.", "sv", SpeechCue{1, 1, "John", "sv1917", {1, 1}, {1, 1}, false}}};
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
                  "Pause must suspend preparation of later chunks");
        }
        engine->resume();
        wait_for(
            [] {
                return appended == 3;
            },
            "Resume must wake speech preparation");
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
                return stream && appended == 1 && status.starts_with("Läser");
            },
            "A new reading must start after cancellation");
        check(drain() == std::vector<float>{10, 11, 12},
              "Cancelled audio and paused state must not leak into a new reading");
        wait_for(
            [] {
                return status == "Läsningen är klar";
            },
            "The replacement reading must complete");

        engine->speak_batch(
            {{"", "Start.", "sv"}, {"", "En längre fortsättning som redan finns i ljudcachen.", "sv"}});
        wait_for(
            [] {
                return appended == 2 && stream && !stream->waiting();
            },
            "Cached audio must start without waiting for the whole reading");
        {
            std::lock_guard lock(audio_mutex);
            check(first_chunk_released,
                  "A cached reading must release the first chunk before loading later audio");
        }
        engine->stop();

        // A cached first chunk must not start playback that later, slower
        // synthesis would stall. The model then fails validation deterministically.
        engine->speak_batch({{"", "Start.", "sv"}, {"", "Ej cachad.", "sv"}});
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
        std::filesystem::remove_all(data);
        std::cout << "Portable speech streaming checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
