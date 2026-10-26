#include "speech/portable_speech.hpp"
#include "speech/espeak_voice.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/playback_timeline.hpp"
#include "speech/swedish_text.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
namespace ortho {
namespace {
using namespace std::chrono_literals;
std::string voice_name(const std::string& voice) {
    return "eSpeak (" + espeak_voice_name(voice) + ")";
}
// eSpeak uses Perikop's Swedish pronunciations and its built-in rules for other languages.
class EspeakSynthesis {
    std::filesystem::path data_;
    std::unique_ptr<SwedishText> text_;
    std::unique_ptr<Espeak> espeak_;

public:
    explicit EspeakSynthesis(std::filesystem::path data) : data_(std::move(data)) {}
    std::vector<float> operator()(const std::string& text, const std::string& language,
                                  const std::string& voice, double speed,
                                  const std::function<bool()>& cancelled) {
        if (!espeak_)
            espeak_ = std::make_unique<Espeak>();
        // Speed scales eSpeak's own rate, around the chosen 150 words per minute.
        const int rate = std::clamp(int(std::lround(150 * speed)), 80, 450);
        if (language != "sv")
            return espeak_->speak_text(text, {espeak_voice_name(voice), rate}, cancelled);
        if (!text_)
            text_ = std::make_unique<SwedishText>(data_);
        return espeak_->speak(text_->tokens(text), {espeak_voice_name(voice), rate}, cancelled);
    }
};
class PortableSpeech final : public SpeechEngine {
    // The bundled Swedish pronunciation data.
    std::filesystem::path data_;
    SpeechSynthesis synthesis_;
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
    std::map<std::string, std::string> voices_ = Settings{}.speech_voices;
    PlaybackTimeline timeline_;
    std::string stage_ = "Förbereder läsningen";
    std::unique_ptr<PcmOutput> output_;
    // Word lookups use their own front end, so they never wait for synthesis.
    std::mutex lookup_mutex_;
    std::unique_ptr<SwedishText> lookup_;
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
            callback_({++status_sequence_, paused_ ? "Pausad" : stage_});
    }
    void run() {
        // Measured synthesis speed for this session. Until a text has been read,
        // assume it is slower than real time.
        double synthesis_seconds = 0, synthesis_audio = 0;
        while (true) {
            std::deque<SpeechUtterance> batch;
            uint64_t generation;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [this] {
                    return shutdown_ || !queue_.empty();
                });
                if (shutdown_)
                    return;
                generation = generation_.load();
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
                        if (cancelled())
                            break;
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
                double next_weight = 0;
                std::string playing;
                for (const auto& part : parts)
                    remaining_weight += part.weight;
                // Keep a rolling lead. Pause suspends synthesis between chunks as well as audio playback.
                const auto estimated_remaining = [&] {
                    return std::max(0.0, remaining_weight) * produced_seconds /
                           std::max(1e-9, produced_weight);
                };
                const auto realtime_factor = [&] {
                    return synthesis_audio > 0 ? synthesis_seconds / synthesis_audio : 2.5;
                };
                const auto estimated_next = [&] {
                    return next_weight * produced_seconds / std::max(1e-9, produced_weight);
                };
                const auto full = [&] {
                    const auto limit =
                        buffer_limit(estimated_remaining(), realtime_factor(), estimated_next());
                    return output_->buffered() >= PcmStream::capacity ||
                           output_->buffered_frames() / 24000.0 >= limit;
                };
                // Called with the mutex held after audio is queued or played.
                // A full buffer cannot grow, so it plays whatever the estimate says.
                const auto update_gate = [&] {
                    const double target =
                        buffer_target(estimated_remaining(), realtime_factor(), estimated_next());
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
                    const auto& text = part.text;
                    const auto progress = std::to_string(i + 1) + "/" + std::to_string(parts.size());
                    std::string voice;
                    {
                        std::lock_guard lock(mutex_);
                        voice = voices_.at(part.language);
                    }
                    const auto name = voice_name(voice);
                    playing = "Läser med " + name;
                    report(generation, i ? "Läser med " + name + ", förbereder del " + progress
                                         : "Förbereder läsningen, " + progress);
                    double speed;
                    {
                        std::lock_guard lock(mutex_);
                        speed = speed_;
                    }
                    const auto begin = std::chrono::steady_clock::now();
                    auto samples = synthesis_(text, part.language, voice, speed, cancelled);
                    if (cancelled())
                        break;
                    synthesis_seconds +=
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
                    synthesis_audio += samples.size() / 24000.0;
                    if (i)
                        samples.insert(samples.begin(), 4800, 0.0f);
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
                        if (prepared_output)
                            output_ = std::move(prepared_output);
                        if (!i)
                            output_->start(paused_);
                        timeline_.append(part.cue, samples.size(), i ? 4800 : 0, part.from, part.to);
                        produced_seconds += samples.size() / 24000.0;
                        produced_weight += part.weight;
                        remaining_weight -= part.weight;
                        next_weight = i + 1 < parts.size() ? parts[i + 1].weight : 0;
                        output_->append(std::move(samples));
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
                }
                {
                    std::lock_guard lock(mutex_);
                    if (cancelled())
                        continue;
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
    void submit(const std::vector<SpeechUtterance>& utterances) {
        if (utterances.empty())
            throw std::runtime_error("Ingen text finns att läsa upp.");
        for (const auto& utterance : utterances)
            if (!speech_language_supported(utterance.language))
                throw std::runtime_error("Uppläsning saknas för språket " + utterance.language + ".");
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

public:
    PortableSpeech(std::filesystem::path data, SpeechStatus status, SpeechSynthesis synthesis)
        : data_(std::move(data)),
          synthesis_(synthesis ? std::move(synthesis)
                               : [espeak = std::make_shared<EspeakSynthesis>(data_)](
                                     const std::string& text, const std::string& language, const std::string& voice,
                                     double speed, const std::function<bool()>& cancelled) {
                                     return (*espeak)(text, language, voice, speed, cancelled);
                                 }),
          callback_(std::move(status)) {
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
        submit(utterances);
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
    }
    void set_voice(const std::string& voice, const std::string& language) override {
        std::lock_guard lock(mutex_);
        voices_.at(language) = voice;
    }
    std::string pronunciation(const std::string& text) override {
        std::lock_guard lock(lookup_mutex_);
        try {
            if (!lookup_)
                lookup_ = std::make_unique<SwedishText>(data_);
            return lookup_->phonemes(text);
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
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& speech_data,
                                                     SpeechStatus status, SpeechSynthesis synthesis) {
    return std::make_unique<PortableSpeech>(speech_data, std::move(status), std::move(synthesis));
}
} // namespace ortho
