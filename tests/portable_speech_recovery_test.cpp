#include "speech/espeak_voice.hpp"
#include "speech/kokoro_audio.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/portable_speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {
std::mutex audio_mutex;
std::shared_ptr<ortho::PcmStream> stream;
bool paused = false;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string contents(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(input)), {}};
}
} // namespace
namespace ortho {
// Real inference and cache operations, with a device drained by the test thread.
struct PcmOutput::Impl {};
PcmOutput::PcmOutput() : impl_(std::make_unique<Impl>()) {}
PcmOutput::~PcmOutput() = default;
void PcmOutput::start(bool pause) {
    std::lock_guard lock(audio_mutex);
    stream = std::make_shared<PcmStream>();
    paused = pause;
}
void PcmOutput::append(std::vector<float> samples) {
    std::lock_guard lock(audio_mutex);
    check(stream && stream->append(std::move(samples)), "Recovery playback accepts synthesized PCM");
}
void PcmOutput::complete() {
    std::lock_guard lock(audio_mutex);
    stream->complete();
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
    if (stream)
        stream->release();
}
void PcmOutput::pause(bool pause) {
    std::lock_guard lock(audio_mutex);
    paused = pause;
}
void PcmOutput::set_speed(double) {}
void PcmOutput::stop() {
    std::lock_guard lock(audio_mutex);
    stream.reset();
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
        check(argc == 3, "Expected bundled voices and isolated test directory");
        const std::filesystem::path voices(argv[1]), root(argv[2]);
        std::filesystem::remove_all(root);
        {
            const auto data = root / "cached";
            std::filesystem::create_directories(data);
            const std::string text = "Herren är min herde.";
            // Real inference plays once and leaves its audio in the cache.
            const auto play = [&](const std::filesystem::path& from, const char* failure) {
                auto engine = create_portable_speech(from, data, {});
                engine->speak({"", text, "sv"});
                bool heard = false;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
                while (std::chrono::steady_clock::now() < deadline &&
                       engine->playback().state != SpeechState::Completed) {
                    {
                        std::lock_guard guard(audio_mutex);
                        if (stream) {
                            float samples[6000];
                            const auto count = stream->render(samples, 6000);
                            for (size_t i = 0; i < count; ++i)
                                heard = heard || std::abs(samples[i]) > 1e-5f;
                        }
                    }
                    check(engine->playback().state != SpeechState::Error, failure);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                check(heard && engine->playback().state == SpeechState::Completed, failure);
                engine.reset();
                std::lock_guard guard(audio_mutex);
                stream.reset();
            };
            play(voices, "Real inference reads aloud");
            {
                SpeechCache cache(data / "speech-cache.db");
                const auto samples =
                    cache.load(std::string(kokoro_pack_id) + "/ort1.23.2/alice/v1", "sv", text);
                check(samples && std::any_of(samples->begin(), samples->end(),
                                             [](float sample) {
                                                 return std::abs(sample) > 1e-5f;
                                             }),
                      "Playback persists non-silent audio across engine restarts");
            }
            // A broken voice pack makes any accidental regeneration fail.
            const auto cached_voices = data / "cached-voices";
            std::filesystem::create_directories(cached_voices / kokoro_pack_id);
            std::ofstream(cached_voices / kokoro_pack_id / "kokoro.onnx") << "";
            play(cached_voices, "Cached audio plays after restart without synthesis");
        }
        if (!espeak_installed()) {
            std::cout << "eSpeak NG playback skipped: not installed\n";
        } else {
            // eSpeak needs only the pack's pronunciations: no neural model file at all.
            const auto data = root / "espeak", phonetic_voices = data / "voices";
            std::filesystem::create_directories(phonetic_voices / kokoro_pack_id);
            for (const char* file :
                 {"lexicon.tsv", "custom_lexicon.tsv", "g2p-corpus.tsv", "g2p-config.json", "config.json"})
                std::filesystem::copy_file(voices / kokoro_pack_id / file,
                                           phonetic_voices / kokoro_pack_id / file);
            const std::string text = "Herren är min herde, mig skall intet fattas.";
            const auto catalog = Espeak::voices();
            const auto variant = std::ranges::find_if(catalog, [](const auto& voice) {
                return voice.id.starts_with("espeak:");
            });
            const auto voice = variant == catalog.end() ? std::string(espeak_male_id) : variant->id;
            auto engine = create_portable_speech(phonetic_voices, data, {});
            engine->set_voice(voice);
            engine->speak({"", text, "sv"});
            bool heard = false, done = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline) {
                {
                    std::lock_guard guard(audio_mutex);
                    if (stream && !paused) {
                        float samples[6000];
                        const auto count = stream->render(samples, 6000);
                        for (size_t i = 0; i < count; ++i)
                            heard = heard || std::abs(samples[i]) > 1e-5f;
                    }
                }
                check(engine->playback().state != SpeechState::Error, "eSpeak never needs the neural model");
                if (engine->playback().state == SpeechState::Completed) {
                    done = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            engine.reset();
            {
                std::lock_guard guard(audio_mutex);
                stream.reset();
            }
            check(done && heard, "eSpeak reads aloud with only the pack's pronunciations");
            check(SpeechCache(data / "speech-cache.db")
                      .contains("espeak/" + Espeak().version() + "/" + espeak_voice_name(voice) + "/150/v1",
                                "sv", text),
                  "eSpeak caches its audio under its own revision");
        }
        for (const auto* failure : {"corrupt", "future", "read", "write", "lock", "directory"}) {
            const auto data = root / failure, path = data / "speech-cache.db";
            std::filesystem::create_directories(data);
            UserDb user(data / "user.db");
            user.complete("preserved-progress");
            PronunciationReviewDb review(data / "pronunciation-review.db");
            review.save({"Mose", "corrected", "Mo-se", "preserve this decision"});
            DatabaseHandle lock;
            const std::string kind(failure);
            if (kind == "corrupt")
                std::ofstream(path) << "preserve corrupt cache bytes";
            else if (kind == "directory")
                std::filesystem::create_directory(path);
            else {
                {
                    SpeechCache cache(path);
                }
                auto db = storage::open(path, SQLITE_OPEN_READWRITE);
                if (kind == "future")
                    storage::exec(db.get(), "PRAGMA user_version=99");
                if (kind == "read")
                    storage::exec(db.get(), "DROP TABLE audio");
                if (kind == "write")
                    storage::exec(db.get(), "CREATE TRIGGER fail_write BEFORE INSERT ON audio BEGIN "
                                            "SELECT RAISE(ABORT,'injected disk write failure'); END");
                if (kind == "lock") {
                    storage::exec(db.get(), "BEGIN EXCLUSIVE");
                    lock = std::move(db);
                }
            }
            const auto before = kind == "corrupt" || kind == "future" ? contents(path) : std::string{};
            bool warning = false;
            auto engine = create_portable_speech(voices, data, [&](const SpeechUpdate& update) {
                if (update.text.find("utan ljudcache") != std::string::npos)
                    warning = true; // Worker only; inspected after its join below.
            });
            engine->speak({"", "Herren är min herde.", "sv"});
            bool heard = false, done = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
            while (std::chrono::steady_clock::now() < deadline) {
                {
                    std::lock_guard guard(audio_mutex);
                    if (stream && !paused) {
                        float samples[6000];
                        const auto count = stream->render(samples, 6000);
                        for (size_t i = 0; i < count; ++i)
                            heard = heard || std::abs(samples[i]) > 1e-5f;
                    }
                }
                const auto state = engine->playback().state;
                check(state != SpeechState::Error, "Optional cache failure must not abort real inference");
                if (state == SpeechState::Completed) {
                    done = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            engine.reset();
            check(done && heard && warning,
                  "Uncached speech completes with non-silent audio and a recovery message");
            check(user.completed().contains("preserved-progress"),
                  "Cache recovery preserves reading progress");
            check(review.overrides().size() == 1 && review.overrides().front().spoken == "Mo-se",
                  "Cache recovery preserves pronunciation corrections");
            if (kind == "corrupt" || kind == "future")
                check(contents(path) == before, "Recovery preserves corrupt bytes and future cache schemas");
            std::cout << "Uncached speech recovery passed: " << kind << '\n';
        }
        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
