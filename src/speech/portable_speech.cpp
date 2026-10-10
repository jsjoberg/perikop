#include "speech/portable_speech.hpp"
#include "speech/kokoro_audio.hpp"
#include "speech/kokoro_text.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/playback_timeline.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
namespace ortho {
namespace {
using namespace std::chrono_literals;
std::string voice_name(const std::string& voice) {
    return voice == "bjorn" ? "Björn" : "Alice";
}
// 16-bit mono PCM WAV at 24 kHz, for the speech probe.
void write_wav(const std::filesystem::path& path, const std::vector<float>& samples) {
    std::ofstream output(path, std::ios::binary);
    const auto put = [&](uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i)
            output.put(char((value >> (8 * i)) & 0xff));
    };
    const auto data = uint32_t(samples.size() * 2);
    output.write("RIFF", 4);
    put(36 + data, 4);
    output.write("WAVEfmt ", 8);
    put(16, 4);
    put(1, 2);
    put(1, 2);
    put(24000, 4);
    put(48000, 4);
    put(2, 2);
    put(16, 2);
    output.write("data", 4);
    put(data, 4);
    for (float sample : samples)
        put(uint32_t(uint16_t(int16_t(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767)))), 2);
    if (!output)
        throw std::runtime_error("Cannot write the speech probe audio.");
}
// Read-aloud is Swedish only, with Alice or Björn.
class Kokoro {
    KokoroText text_;
    KokoroAudio audio_;
    static std::filesystem::path checked(const std::filesystem::path& path) {
        for (const char* file :
             {"kokoro.onnx", "g2p-encoder.onnx", "g2p-decoder.onnx", "g2p-config.json", "config.json",
              "alice.bin", "bjorn.bin", "lexicon.tsv", "custom_lexicon.tsv", "g2p-corpus.tsv"})
            if (!std::filesystem::is_regular_file(path / file))
                throw std::runtime_error("Röstpaketet för Alice och Björn saknas eller är ofullständigt. "
                                         "Installera det för att lyssna offline.");
        return path;
    }

public:
    explicit Kokoro(const std::filesystem::path& path) : text_(checked(path)), audio_(path) {}
    std::vector<float> generate(const std::string& text, const std::string& voice,
                                const std::function<bool()>& cancelled) {
        const auto tokens = text_.tokens(text);
        // Text without speakable letters reads as a short pause.
        if (tokens.empty())
            return std::vector<float>(4800, 0.0f);
        // The voice context holds 510 tokens. Longer text splits at a word gap (token 16).
        std::vector<float> result;
        for (size_t begin = 0; begin < tokens.size();) {
            if (cancelled())
                throw std::runtime_error("Speech cancelled");
            size_t end = tokens.size(), next = end;
            if (end - begin > 510) {
                end = next = begin + 510;
                for (auto at = begin + 510; at > begin; --at)
                    if (tokens[at] == 16) {
                        end = at;
                        next = at + 1;
                        break;
                    }
            }
            const auto pcm = audio_.generate({tokens.begin() + begin, tokens.begin() + end}, voice);
            result.insert(result.end(), pcm.begin(), pcm.end());
            begin = next;
        }
        return result;
    }
};
class PortableSpeech final : public SpeechEngine {
    // The bundled voice pack, and the user data folder that holds the speech cache.
    std::filesystem::path pack_, data_;
    SpeechStatus callback_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<SpeechUtterance> queue_;
    std::atomic<uint64_t> generation_{0};
    uint64_t status_sequence_ = 0;
    bool shutdown_ = false, paused_ = false, active_ = false;
    bool output_started_ = false;
    SpeechState state_ = SpeechState::Idle;
    double ready_ = 0, speed_ = 1;
    std::string voice_ = "alice";
    PlaybackTimeline timeline_;
    std::string stage_ = "Förbereder läsningen";
    std::unique_ptr<PcmOutput> output_;
    // Word lookups use their own front end, so they never wait for synthesis.
    std::mutex lookup_mutex_;
    std::unique_ptr<KokoroText> lookup_;
    struct Lookup {
        std::string text;
        std::function<void(std::string)> deliver;
    };
    std::mutex lookup_queue_mutex_;
    std::condition_variable lookup_condition_;
    std::optional<Lookup> lookup_request_;
    std::atomic<bool> lookup_shutdown_{false};
    std::thread lookup_worker_;
    std::thread worker_;
    void run_lookups() {
        while (true) {
            Lookup request;
            {
                std::unique_lock lock(lookup_queue_mutex_);
                lookup_condition_.wait(lock, [this] {
                    return lookup_shutdown_ || lookup_request_.has_value();
                });
                if (lookup_shutdown_)
                    return;
                if (!lookup_request_)
                    continue;
                request = std::move(*lookup_request_);
                lookup_request_.reset();
            }
            auto result = pronunciation(request.text);
            if (!lookup_shutdown_)
                request.deliver(std::move(result));
        }
    }
    void shutdown_workers() {
        {
            std::lock_guard lock(lookup_queue_mutex_);
            lookup_shutdown_ = true;
        }
        lookup_condition_.notify_all();
        {
            std::lock_guard lock(mutex_);
            shutdown_ = true;
            ++generation_;
            queue_.clear();
            callback_ = {};
            if (output_)
                output_->stop();
        }
        condition_.notify_all();
        if (lookup_worker_.joinable())
            lookup_worker_.join();
        if (worker_.joinable())
            worker_.join();
    }
    void report(uint64_t generation, const std::string& status, SpeechState state = SpeechState::Buffering) {
        std::lock_guard lock(mutex_);
        if (shutdown_ || generation != generation_.load())
            return;
        stage_ = status;
        state_ = state;
        if (callback_)
            callback_({++status_sequence_, paused_ ? "Pausad" : status});
    }
    void run() {
        std::unique_ptr<Kokoro> kokoro;
        std::unique_ptr<SpeechCache> cache;
        // Measured synthesis speed for this session. Until the model has run,
        // assume it is slower than real time, as on the reference hardware.
        double model_seconds = 0, model_audio = 0;
        while (true) {
            std::deque<SpeechUtterance> batch;
            uint64_t generation;
            std::string voice;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [this] {
                    return shutdown_ || !queue_.empty();
                });
                if (shutdown_)
                    return;
                generation = generation_.load();
                voice = voice_;
                batch.swap(queue_);
                active_ = true;
                output_started_ = false;
                state_ = SpeechState::Buffering;
                ready_ = 0;
            }
            const auto cancelled = [this, generation] {
                return generation_.load() != generation;
            };
            try {
                if (!cache)
                    cache = std::make_unique<SpeechCache>(data_ / "speech-cache.db");
                {
                    std::lock_guard lock(mutex_);
                    if (cancelled())
                        continue;
                    // Retain any additional utterances enqueued for this reading.
                    batch.insert(batch.end(), queue_.begin(), queue_.end());
                    queue_.clear();
                }
                struct Part {
                    std::string language, text;
                    std::optional<SpeechCue> cue;
                    double from, to, weight;
                };
                std::vector<Part> parts;
                for (const auto& utterance : batch) {
                    auto chunks = speech_chunks(utterance.speech_text);
                    double total = 0, used = 0;
                    for (const auto& text : chunks)
                        total += speech_text_weight(text);
                    for (auto& text : chunks) {
                        const auto weight = speech_text_weight(text);
                        parts.push_back({utterance.language, std::move(text), utterance.cue,
                                         used / std::max(1.0, total), (used + weight) / std::max(1.0, total),
                                         weight});
                        used += weight;
                    }
                }
                {
                    std::lock_guard lock(mutex_);
                    if (cancelled())
                        continue;
                    timeline_.reset(parts.size());
                }
                double remaining_weight = 0, produced_weight = 0, produced_seconds = 0;
                std::string playing;
                for (const auto& part : parts)
                    remaining_weight += part.weight;
                // Keep a rolling lead; slower models can prepare up to ten minutes.
                // Pause suspends preparation between chunks as well as audio playback.
                const auto estimated_remaining = [&] {
                    return remaining_weight * produced_seconds / std::max(1e-9, produced_weight);
                };
                const auto realtime_factor = [&] {
                    return (model_audio > 0 ? model_seconds / model_audio : 2.5) * speed_;
                };
                const auto full = [&] {
                    const auto limit = buffer_limit(estimated_remaining(), realtime_factor());
                    return output_->buffered() >= PcmStream::capacity ||
                           output_->buffered_frames() / 24000.0 >= limit;
                };
                // Called with the mutex held after audio is queued or played.
                // A full buffer cannot grow, so it plays whatever the estimate says.
                const auto update_gate = [&] {
                    const double target = buffer_target(estimated_remaining(), realtime_factor());
                    const double buffered = output_->buffered_frames() / 24000.0;
                    ready_ = std::min(1.0, buffered / target);
                    if (output_->buffered() && (buffered >= target || full()))
                        output_->release();
                };
                for (size_t i = 0; i < parts.size(); ++i) {
                    {
                        std::unique_lock lock(mutex_);
                        while (!cancelled() && (paused_ || (i && full()))) {
                            if (paused_)
                                condition_.wait(lock, [&] {
                                    return cancelled() || !paused_;
                                });
                            else {
                                update_gate();
                                condition_.wait_for(lock, 100ms);
                            }
                        }
                        if (cancelled())
                            break;
                    }
                    const auto& part = parts[i];
                    const auto& language = part.language;
                    const auto& text = part.text;
                    const auto progress = std::to_string(i + 1) + "/" + std::to_string(parts.size());
                    const auto name = voice_name(voice);
                    const auto revision = std::string(kokoro_pack_id) + "/ort1.23.2/" + voice + "/v1";
                    playing = "Läser med " + name;
                    report(generation, i ? "Läser med " + name + " · förbereder del " + progress
                                         : "Förbereder läsningen · " + progress);
                    auto samples = cache->load(revision, language, text);
                    if (!samples) {
                        if (!kokoro) {
                            report(generation, "Förbereder röstmodellen · " + name, SpeechState::Loading);
                            kokoro = std::make_unique<Kokoro>(pack_);
                        }
                        report(generation, i ? "Läser med " + name + " · förbereder del " + progress
                                             : "Förbereder läsningen · " + progress);
                        const auto begin = std::chrono::steady_clock::now();
                        samples = kokoro->generate(text, voice, cancelled);
                        if (cancelled())
                            break;
                        model_seconds +=
                            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
                        model_audio += samples->size() / 24000.0;
                        cache->save(revision, language, text, *samples);
                    }
                    if (i)
                        samples->insert(samples->begin(), 4800, 0.0f);
                    bool need_output = false;
                    {
                        std::lock_guard lock(mutex_);
                        if (cancelled())
                            break;
                        need_output = !output_;
                    }
                    // Opening a device can wait for the OS. Controls and playback polling
                    // must remain available while this happens.
                    auto prepared_output = need_output ? std::make_unique<PcmOutput>() : nullptr;
                    {
                        std::lock_guard lock(mutex_);
                        if (cancelled())
                            break;
                        if (prepared_output) {
                            output_ = std::move(prepared_output);
                            output_->set_speed(speed_);
                        }
                        if (!i)
                            output_->start(paused_);
                        timeline_.append(part.cue, samples->size(), i ? 4800 : 0, part.from, part.to);
                        produced_seconds += samples->size() / 24000.0;
                        produced_weight += part.weight;
                        remaining_weight -= part.weight;
                        output_->append(std::move(*samples));
                        output_started_ = true;
                        update_gate();
                    }
                    report(generation, playing, SpeechState::Playing);
                }
                {
                    std::lock_guard lock(mutex_);
                    if (cancelled())
                        continue;
                    if (parts.empty())
                        throw std::runtime_error("Ingen text finns att läsa upp.");
                    output_->complete();
                    ready_ = 1;
                }
                report(generation, playing, SpeechState::Playing);
                {
                    std::unique_lock lock(mutex_);
                    while (!shutdown_ && !cancelled() && !output_->finished())
                        condition_.wait_for(lock, 100ms);
                    if (shutdown_)
                        return;
                    if (cancelled())
                        continue;
                    active_ = false;
                    paused_ = false;
                }
                report(generation, "Läsningen är klar", SpeechState::Completed);
            } catch (const std::exception& error) {
                {
                    std::lock_guard lock(mutex_);
                    if (cancelled())
                        continue;
                    queue_.clear();
                    active_ = false;
                    paused_ = false;
                    output_started_ = false;
                    if (output_)
                        output_->stop();
                }
                report(generation, std::string("Uppläsning: ") + error.what(), SpeechState::Error);
            }
        }
    }

public:
    PortableSpeech(const std::filesystem::path& voices, std::filesystem::path data, SpeechStatus status)
        : pack_(voices / kokoro_pack_id), data_(std::move(data)), callback_(std::move(status)) {
        // Also join the first worker if starting the second one fails.
        try {
            lookup_worker_ = std::thread([this] {
                run_lookups();
            });
            worker_ = std::thread([this] {
                run();
            });
        } catch (...) {
            shutdown_workers();
            throw;
        }
    }
    ~PortableSpeech() override {
        shutdown_workers();
    }
    void speak(const SpeechUtterance& utterance) override {
        speak_batch({utterance});
    }
    void speak_batch(const std::vector<SpeechUtterance>& utterances) override {
        if (utterances.empty())
            throw std::runtime_error("Ingen text finns att läsa upp.");
        for (const auto& utterance : utterances)
            if (utterance.language != "sv")
                throw std::runtime_error("Uppläsning finns bara på svenska.");
        if (!std::filesystem::exists(pack_ / "kokoro.onnx"))
            throw std::runtime_error("Den här versionen av Perikop byggdes utan rösterna Alice och Björn.");
        {
            std::lock_guard lock(mutex_);
            queue_.insert(queue_.end(), utterances.begin(), utterances.end());
            if (!active_) {
                state_ = SpeechState::Buffering;
                output_started_ = false;
            }
            active_ = true;
        }
        condition_.notify_all();
    }
    void pause() override {
        std::lock_guard lock(mutex_);
        if (!active_)
            return;
        paused_ = true;
        if (output_)
            output_->pause(true);
        if (callback_)
            callback_({++status_sequence_, "Pausad"});
    }
    void resume() override {
        std::lock_guard lock(mutex_);
        if (!active_)
            return;
        paused_ = false;
        if (output_)
            output_->pause(false);
        if (callback_)
            callback_({++status_sequence_, stage_});
        condition_.notify_all();
    }
    void stop() override {
        std::lock_guard lock(mutex_);
        ++generation_;
        queue_.clear();
        active_ = false;
        paused_ = false;
        output_started_ = false;
        state_ = SpeechState::Stopped;
        if (output_)
            output_->stop();
        if (callback_)
            callback_({++status_sequence_, "Stoppad"});
        condition_.notify_all();
    }
    void set_speed(double speed) override {
        std::lock_guard lock(mutex_);
        speed_ = speed;
        if (output_)
            output_->set_speed(speed);
        condition_.notify_all();
    }
    void set_voice(const std::string& voice) override {
        std::lock_guard lock(mutex_);
        voice_ = voice;
    }
    std::string pronunciation(const std::string& text) override {
        std::lock_guard lock(lookup_mutex_);
        try {
            if (!lookup_)
                lookup_ = std::make_unique<KokoroText>(pack_);
            return lookup_->ipa(text);
        } catch (const std::exception&) {
            return {};
        }
    }
    void pronunciation_async(const std::string& text, std::function<void(std::string)> deliver) override {
        {
            std::lock_guard lock(lookup_queue_mutex_);
            // Clicking a new word replaces pending work for the previous one.
            lookup_request_ = Lookup{text, std::move(deliver)};
        }
        lookup_condition_.notify_one();
    }
    SpeechPlayback playback() const override {
        std::lock_guard lock(mutex_);
        if (!output_started_)
            return {paused_ ? SpeechState::Paused : state_, {}, 0, 0};
        const auto position = output_->progress();
        auto state = state_;
        if (paused_)
            state = SpeechState::Paused;
        else if (active_)
            state = position.finished  ? SpeechState::Completed
                    : position.waiting ? SpeechState::Buffering
                                       : SpeechState::Playing;
        auto result = timeline_.at(position.played, state);
        result.ready = ready_;
        return result;
    }
};
} // namespace
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& voices,
                                                     const std::filesystem::path& data, SpeechStatus status) {
    return std::make_unique<PortableSpeech>(voices, data, std::move(status));
}
bool render_speech_probe(const std::filesystem::path& voices, const std::filesystem::path& output) {
    const auto begin = std::chrono::steady_clock::now();
    Kokoro model(voices / kokoro_pack_id);
    const auto loaded = std::chrono::steady_clock::now();
    auto samples = model.generate(
        "Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar.", "alice", [] {
            return false;
        });
    write_wav(output, samples);
    std::cout << "Portable Swedish PCM: " << samples.size()
              << " frames, audio_seconds=" << samples.size() / 24000.0
              << ", load_seconds=" << std::chrono::duration<double>(loaded - begin).count()
              << ", generation_seconds="
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - loaded).count() << '\n';
    return true;
}
} // namespace ortho
