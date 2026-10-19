// Headless timing probe of the real speech worker. No audio hardware is opened.
#include "probe_hooks.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/portable_speech.hpp"
#include "speech/tempo.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using Clock = std::chrono::steady_clock;
namespace {
std::mutex trace_mutex, audio_mutex, state_mutex;
std::condition_variable changed;
Clock::time_point epoch;
int scenario = 0, threads = 4;
bool done = false, error = false;
std::shared_ptr<ortho::PcmStream> stream;
ortho::TempoStretch stretch;
ortho::PcmProgress progress;
bool paused = false, first_nonzero = false, first_active = false, gate_released = false;
std::vector<float> captured;
}
void speech_probe_trace(const char* event, double value, double other) {
    const auto now = Clock::now();
    std::lock_guard lock(trace_mutex);
    std::cout << std::setprecision(9) << "{\"scenario\":" << scenario << ",\"event\":\"" << event
              << "\",\"seconds\":" << std::chrono::duration<double>(now-epoch).count()
              << ",\"value\":" << value << ",\"other\":" << other << "}\n" << std::flush;
}
int speech_probe_threads() { return threads; }
namespace ortho {
struct PcmOutput::Impl {};
PcmOutput::PcmOutput() : impl_(std::make_unique<Impl>()) { speech_probe_trace("headless_device_open"); }
PcmOutput::~PcmOutput() = default;
void PcmOutput::start(bool pause) {
    std::lock_guard lock(audio_mutex);
    stream=std::make_shared<PcmStream>(); ::progress={}; paused=pause;
    first_nonzero=first_active=gate_released=false; captured.clear();
    speech_probe_trace("output_start");
}
void PcmOutput::append(std::vector<float> samples) {
    std::lock_guard lock(audio_mutex);
    speech_probe_trace("append",samples.size()/24000.0);
    captured.insert(captured.end(),samples.begin(),samples.end());
    if (!stream || !stream->append(std::move(samples))) throw std::runtime_error("Headless buffer overflow");
}
void PcmOutput::complete() {
    std::lock_guard lock(audio_mutex);
    stream->complete(); speech_probe_trace("output_complete");
}
size_t PcmOutput::buffered() const { std::lock_guard lock(audio_mutex); return stream?stream->buffered():0; }
uint64_t PcmOutput::buffered_frames() const { std::lock_guard lock(audio_mutex); return stream?stream->buffered_frames():0; }
void PcmOutput::release() {
    std::lock_guard lock(audio_mutex);
    stream->release();
    if (!gate_released) { gate_released=true; speech_probe_trace("gate_released"); }
}
void PcmOutput::pause(bool value) { std::lock_guard lock(audio_mutex); paused=value; }
void PcmOutput::set_speed(double) {} // The probe always uses normal speed.
void PcmOutput::stop() { std::lock_guard lock(audio_mutex); stream.reset(); }
bool PcmOutput::finished() const { std::lock_guard lock(audio_mutex); return !stream || ::progress.finished; }
PcmProgress PcmOutput::progress() const { std::lock_guard lock(audio_mutex); return ::progress; }
}
int main(int argc, char** argv) {
    try {
        if (argc!=4) throw std::runtime_error("Usage: speech-latency VOICES-DIR NEW-CACHE-DIR THREADS");
        threads=std::stoi(argv[3]);
        if (threads<1 || threads>32) throw std::runtime_error("Threads must be 1-32");
        const auto path=[](const char* p) { return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(p))); };
        const auto cache=path(argv[2]);
        if (std::filesystem::exists(cache)) throw std::runtime_error("Use a new cache directory; existing data is never deleted");
        std::filesystem::create_directories(cache);
        epoch=Clock::now();
        std::cout << "{\"event\":\"configuration\",\"threads\":" << threads
                  << ",\"audio_hardware\":false,\"speed\":1,\"case_1\":\"cold_model_empty_cache\","
                     "\"case_2\":\"warm_model_new_text\",\"case_3\":\"cached_replay\"}\n";
        auto engine=ortho::create_portable_speech(path(argv[1]),cache,[](const ortho::SpeechUpdate& update) {
            if (update.text=="Läsningen är klar" || update.text.starts_with("Uppläsning:")) {
                std::lock_guard lock(state_mutex);
                error=update.text.starts_with("Uppläsning:"); done=true; changed.notify_all();
                if (error) std::cerr << update.text << '\n';
            }
        });
        engine->set_voice("bjorn");
        std::jthread device([](std::stop_token stop) {
            auto tick=Clock::now();
            while (!stop.stop_requested()) {
                tick+=std::chrono::milliseconds(10);
                {
                    std::lock_guard lock(audio_mutex);
                    if (stream && !paused) {
                        std::vector<float> samples(240);
                        stretch.render(*stream,samples.data(),samples.size(),1);
                        progress={stretch.played(),stretch.waiting(),stretch.finished()};
                        double energy=0; bool nonzero=false;
                        for (float x:samples) { energy+=double(x)*x; nonzero=nonzero || std::abs(x)>1e-5; }
                        if (!first_nonzero && nonzero) { first_nonzero=true; speech_probe_trace("first_nonzero_gt_1e-5"); }
                        if (!first_active && std::sqrt(energy/samples.size())>std::pow(10.,-50./20)) {
                            first_active=true; speech_probe_trace("first_frame_rms_gt_minus50dbfs");
                        }
                    }
                }
                std::this_thread::sleep_until(tick);
            }
        });
        const char* text[]={"I begynnelsen skapade Gud himmel och jord.","Han låter mig vila på gröna ängar.","I begynnelsen skapade Gud himmel och jord."};
        for (int i=0;i<3;++i) {
            {
                std::lock_guard lock(trace_mutex);
                scenario=i+1; epoch=Clock::now();
            }
            { std::lock_guard lock(state_mutex); done=error=false; }
            speech_probe_trace("submit");
            engine->speak({"",text[i],"sv"});
            {
                std::unique_lock lock(state_mutex);
                if (!changed.wait_for(lock,std::chrono::minutes(3),[]{return done;})) throw std::runtime_error("Speech worker timed out");
                if (error) throw std::runtime_error("Speech worker failed");
            }
            speech_probe_trace("completed");
            {
                std::lock_guard lock(audio_mutex);
                const auto file=cache/("case-"+std::to_string(i+1)+".f32");
                std::ofstream f(file,std::ios::binary);
                f.write(reinterpret_cast<const char*>(captured.data()),std::streamsize(captured.size()*sizeof(float)));
            }
        }
        device.request_stop();device.join();
        engine.reset();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
