#include "speech/compact_voice.hpp"
#include "speech/kokoro_audio.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/portable_speech.hpp"
#include "speech/tempo.hpp"
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
ortho::TempoStretch stretch;
ortho::PcmProgress delivered;
float speed = 1;
size_t appended = 0;
size_t devices_opened = 0;
bool paused = false, released = false, completed = false;
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
        if (stream && !paused) {
            stretch.render(*stream, samples.data(), samples.size(), speed);
            delivered = {stretch.played(), stretch.waiting(), stretch.finished()};
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
    ++devices_opened;
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
void PcmOutput::set_speed(double value) {
    std::lock_guard lock(audio_mutex);
    speed = float(value);
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
        check(argc == 3, "Usage: ortho-portable-speech-tests TEST-DIRECTORY COMPACT-PACK");
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
            cache.save(revision, "sv", "Slutet.", std::vector<float>(2400, 0.5f));
            cache.save(revision, "sv", "En längre fortsättning som redan finns i ljudcachen.",
                       std::vector<float>(size_t{24000} * 30, 2.0f));
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
        const SpeechCue first{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}};
        const std::vector<SpeechUtterance> reading = {
            {"", "Första.", "sv", first},
            {"", "Andra.", "sv", SpeechCue{0, 0, "Ps", "sv1917", {23, 2}, {23, 2}}},
            {"", "Tredje.", "sv", SpeechCue{1, 1, "John", "sv1917", {1, 1}, {1, 1}}}};
        engine->prepare_batch(reading);
        wait_for(
            [] {
                return status == "Ljudet är förberett, Alice";
            },
            "Cached preparation must finish without playback");
        const auto prepared = engine->preparation();
        check(prepared.state == PreparationState::Ready && prepared.completed == 3 && prepared.total == 3 &&
                  !speech_active(engine->playback().state) && !engine->playback().cue,
              "Preparation reports its own progress without completing a reading");
        {
            std::lock_guard lock(audio_mutex);
            check(!device_entered && !stream, "Preparation must never open an audio device");
        }
        engine->set_voice("bjorn");
        engine->prepare_batch(reading);
        wait_for(
            [] {
                return status.starts_with("Förberedelse:");
            },
            "Another voice cannot reuse Alice's cached audio");
        check(engine->preparation().state == PreparationState::Error &&
                  engine->playback().state != SpeechState::Error,
              "Preparation failures remain separate from playback failures");
        engine->set_voice("alice");
        // Cancel a queued preparation and immediately replace it with playback.
        engine->prepare_batch(std::vector<SpeechUtterance>(10000, reading.front()));
        engine->stop();
        check(engine->preparation().state == PreparationState::Idle,
              "Stop clears preparation and its queued work");
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

        engine->set_speed(0.25);
        engine->speak({"", "Slutet.", "sv", first});
        wait_for(
            [] {
                return appended == 1 && stream && !stream->waiting();
            },
            "Tail test prepares cached audio");
        bool pending_tail = false;
        for (int block = 0; block < 100; ++block) {
            drain(256);
            std::lock_guard lock(audio_mutex);
            if (stream->finished() && !delivered.finished) {
                pending_tail = true;
                break;
            }
        }
        check(pending_tail, "Worker must retain the quarter-speed tail after source consumption");
        const auto before_pause = engine->playback();
        check(before_pause.state == SpeechState::Playing && before_pause.progress < 1,
              "Completion and final cue must wait for audible output");
        engine->pause();
        check(drain(256).empty() && engine->playback().progress == before_pause.progress,
              "Pause preserves the pending tail and its progress");
        engine->set_speed(2);
        engine->resume();
        bool complete = false;
        for (int block = 0; block < 100; ++block) {
            drain(256);
            if (engine->playback().state == SpeechState::Completed) {
                complete = true;
                break;
            }
        }
        check(complete && engine->playback().progress == 1,
              "Resume and a speed change near the end drain audio before completion");
        engine->stop();
        engine->set_speed(1);

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
        // Exercise the production pipeline with the reusable voice and no neural pack.
        std::filesystem::remove_all(voice);
        std::filesystem::copy(std::filesystem::path(argv[2]), data / "voices" / compact_pack_id,
                              std::filesystem::copy_options::recursive);
        const std::string compact_text = "Herren är min herde.";
        engine = create_portable_speech(data / "voices", data, [](const SpeechUpdate& update) {
            std::lock_guard lock(audio_mutex);
            status = update.text;
            changed.notify_all();
        });
        size_t before_devices;
        {
            std::lock_guard lock(audio_mutex);
            before_devices = devices_opened;
        }
        bool missing_voice_rejected = false;
        try {
            engine->prepare_batch({{"", compact_text, "sv", first}});
        } catch (const std::exception&) {
            missing_voice_rejected = true;
        }
        check(missing_voice_rejected && engine->pronunciation(compact_text).empty(),
              "Missing neural voices must not silently select an unintelligible experimental voice");
        engine->set_voice("bjorn-lite");
        engine->prepare_batch({{"", compact_text, "sv", first}});
        wait_for(
            [] {
                return status == "Ljudet är förberett, Björn Bas";
            },
            "The compact experiment must require explicit voice selection");
        {
            std::lock_guard lock(audio_mutex);
            check(devices_opened == before_devices, "Compact preparation must not open a device");
        }
        {
            SpeechCache saved(data / "speech-cache.db");
            const auto audio = saved.load(compact_revision(data / "voices" / compact_pack_id) + "/join-v1",
                                          "sv", compact_text);
            check(audio && !audio->empty(),
                  "The compact voice must cache its actual generated audio under its own revision");
            UserDb preferences(data / "compact-user.db");
            auto settings = preferences.load();
            settings.speech_voice = "bjorn-lite";
            preferences.save(settings);
            check(preferences.load().speech_voice == "bjorn-lite",
                  "Compact voice selection must survive a profile round trip");
        }
        engine->set_voice("bjorn-lite");
        engine->speak_batch(
            {{"", compact_text, "sv", first}, {"", "Mose såg en stjärna.", "sv", reading[2].cue}});
        wait_for(
            [] {
                return stream && appended == 2 && released;
            },
            "Compact speech must reach the real playback worker");
        engine->pause();
        const auto compact_held = engine->playback();
        check(drain().empty() && engine->playback().progress == compact_held.progress,
              "Pause must freeze compact audio and its verse marker");
        engine->set_speed(1.25);
        engine->resume();
        bool saw_second = false, finished = false;
        for (int block = 0; block < 1000; ++block) {
            drain(2400);
            const auto position = engine->playback();
            saw_second = saw_second || (position.cue && position.cue->reading == 1);
            if (position.state == SpeechState::Completed) {
                finished = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(finished && saw_second,
              "Compact audio must finish at changed speed and advance real verse cues");
        engine->stop();
        check(!engine->playback().cue && engine->playback().state == SpeechState::Stopped,
              "Stopping compact playback must clear its cue");
        engine.reset();
        std::filesystem::remove_all(data);
        std::cout << "Portable speech streaming checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
