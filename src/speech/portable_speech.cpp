#include "speech/portable_speech.hpp"
#include "speech/pcm_output.hpp"
#include "storage/database.hpp"
#include "speech_voice_manifest.hpp"
#include <chatterbox/audio.hpp>
#include <chatterbox/backend.hpp>
#include <chatterbox/text.hpp>
#include <chatterbox/tokenizer.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
namespace ortho {
namespace {
using namespace std::chrono_literals;
std::string utf8(const std::filesystem::path& path) { const auto bytes=path.u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()}; }
const std::string cache_revision=std::string(voice_pack_id)+"/ort1.23.2/utf8proc2.10/seed42/cfg0.5/t0.8/v1";
class Model {
    chatterbox::BpeTokenizer tokenizer_;
    std::unique_ptr<chatterbox::Backend> backend_;
    chatterbox::SpeakerEmbedding speaker_;
public:
    explicit Model(const std::filesystem::path& path) {
        for(const auto& [file,size]:voice_pack_files)
            if(!std::filesystem::is_regular_file(path/file)||std::filesystem::file_size(path/file)!=size)
                throw std::runtime_error("Röstpaketet saknas eller är ofullständigt. Installera Chatterbox-röstpaketet för att lyssna offline.");
        chatterbox::BundleManifest manifest;
        manifest.root=path;manifest.variant="multilingual";
        tokenizer_=chatterbox::BpeTokenizer::from_file(utf8(path/"tokenizer.json"));
        backend_=chatterbox::create_backend(manifest);
        auto reference=chatterbox::read_wav_mono(utf8(path/"default_voice.wav"));
        if(reference.sample_rate!=24000)reference=chatterbox::resample_linear(reference,24000);
        speaker_=backend_->encode_speaker(reference);
    }
    std::vector<float> generate(const std::string& text,const std::string& language,const std::function<bool()>& cancelled) {
        chatterbox::GenerationOptions options;
        options.seed=42;options.max_new_tokens=750;options.cancelled=cancelled;options.language_id=language;
        const auto normalized=chatterbox::normalize_punctuation(text,chatterbox::PunctuationMode::Multilingual);
        const auto prepared=chatterbox::prepare_multilingual_text(normalized,language,chatterbox::TextPreprocessingAssets{});
        auto result=backend_->generate(tokenizer_.encode(prepared),speaker_,options);
        if(cancelled())throw std::runtime_error("Speech cancelled");
        if(result.sample_rate!=24000||result.samples.size()<1000||
           std::ranges::any_of(result.samples,[](float x){return !std::isfinite(x);})||
           !std::ranges::any_of(result.samples,[](float x){return std::abs(x)>0.0001f;}))
            throw std::runtime_error("Röstmodellen gav inget giltigt ljud.");
        return std::move(result.samples);
    }
};
class PortableSpeech final:public SpeechEngine {
    std::filesystem::path data_;
    SpeechStatus callback_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<SpeechUtterance> queue_;
    std::atomic<uint64_t> generation_{0};
    uint64_t status_sequence_=0;
    bool shutdown_=false,paused_=false,active_=false;
    std::string stage_="Förbereder läsningen";
    std::unique_ptr<PcmOutput> output_;
    std::thread worker_;
    void report(uint64_t generation,const std::string& status) {
        std::lock_guard lock(mutex_);
        if(shutdown_||generation!=generation_.load())return;
        stage_=status;
        if(callback_)callback_({++status_sequence_,paused_?"Pausad":status});
    }
    void run() {
        std::unique_ptr<Model> model;
        std::unique_ptr<SpeechCache> cache;
        while(true) {
            std::deque<SpeechUtterance> batch;
            uint64_t generation;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock,[this]{return shutdown_||!queue_.empty();});
                if(shutdown_)return;
                generation=generation_.load();batch.swap(queue_);active_=true;
            }
            const auto cancelled=[this,generation]{return generation_.load()!=generation;};
            try {
                if(!cache)cache=std::make_unique<SpeechCache>(data_/"speech-cache.db");
                {
                    std::lock_guard lock(mutex_);
                    if(cancelled())continue;
                    // Retain any additional utterances enqueued for this reading.
                    batch.insert(batch.end(),queue_.begin(),queue_.end());queue_.clear();
                }
                std::vector<std::pair<std::string,std::string>> parts;
                for(const auto& utterance:batch)for(auto& text:speech_chunks(utterance.speech_text))parts.emplace_back(utterance.language,std::move(text));
                for(size_t i=0;i<parts.size();++i) {
                    {
                        std::unique_lock lock(mutex_);
                        // Bound lookahead, including while paused. Generate the next
                        // chunk as soon as playback makes room, rather than the whole reading.
                        while(!cancelled()&&i&&output_->buffered()>=2)condition_.wait_for(lock,25ms);
                        if(cancelled())break;
                    }
                    const auto& [language,text]=parts[i];
                    const auto progress=std::to_string(i+1)+"/"+std::to_string(parts.size());
                    report(generation,i?"Läser med Chatterbox · förbereder del "+progress:"Förbereder läsningen · "+progress);
                    auto samples=cache->load(cache_revision,language,text);
                    if(!samples){
                        if(!model){report(generation,"Förbereder röstmodellen · Chatterbox");model=std::make_unique<Model>(data_/"voices"/voice_pack_id);}
                        report(generation,i?"Läser med Chatterbox · förbereder del "+progress:"Förbereder läsningen · "+progress);
                        samples=model->generate(text,language,cancelled);
                        if(cancelled())break;
                        cache->save(cache_revision,language,text,*samples);
                    }
                    if(i)samples->insert(samples->begin(),4800,0.0f);
                    {
                        std::lock_guard lock(mutex_);
                        if(cancelled())break;
                        if(!output_)output_=std::make_unique<PcmOutput>();
                        if(!i)output_->start(paused_);
                        output_->append(std::move(*samples));
                    }
                    report(generation,"Läser med Chatterbox · förhandsversion");
                }
                {
                    std::lock_guard lock(mutex_);
                    if(cancelled())continue;
                    if(parts.empty())throw std::runtime_error("Ingen text finns att läsa upp.");
                    output_->complete();
                }
                report(generation,"Läser med Chatterbox · förhandsversion");
                {
                    std::unique_lock lock(mutex_);
                    while(!shutdown_&&!cancelled()&&!output_->finished())condition_.wait_for(lock,100ms);
                    if(shutdown_)return;
                    if(cancelled())continue;
                    active_=false;paused_=false;
                }
                report(generation,"Läsningen är klar");
            } catch(const std::exception& error) {
                {
                    std::lock_guard lock(mutex_);
                    if(cancelled())continue;
                    queue_.clear();active_=false;paused_=false;
                    if(output_)output_->stop();
                }
                report(generation,std::string("Uppläsning: ")+error.what());
            }
        }
    }
public:
    PortableSpeech(std::filesystem::path data,SpeechStatus status):data_(std::move(data)),callback_(std::move(status)),worker_([this]{run();}){}
    ~PortableSpeech() override {
        {std::lock_guard lock(mutex_);shutdown_=true;++generation_;queue_.clear();callback_={};if(output_)output_->stop();}
        condition_.notify_all();worker_.join();
    }
    void speak(const SpeechUtterance& utterance) override {
        speak_batch({utterance});
    }
    void speak_batch(const std::vector<SpeechUtterance>& utterances) override {
        if(utterances.empty())throw std::runtime_error("Ingen text finns att läsa upp.");
        for(const auto& utterance:utterances)if(utterance.language!="sv"&&utterance.language!="el"&&utterance.language!="en")throw std::runtime_error("Röstpaketet stöder svenska, grekiska och engelska.");
        if(!std::filesystem::exists(data_/"voices"/voice_pack_id/"tokenizer.json"))throw std::runtime_error("Röstpaketet saknas. Installera Chatterbox-röstpaketet för att lyssna offline.");
        {std::lock_guard lock(mutex_);queue_.insert(queue_.end(),utterances.begin(),utterances.end());active_=true;}
        condition_.notify_all();
    }
    void pause() override {std::lock_guard lock(mutex_);if(!active_)return;paused_=true;if(output_)output_->pause(true);if(callback_)callback_({++status_sequence_,"Pausad"});}
    void resume() override {std::lock_guard lock(mutex_);if(!active_)return;paused_=false;if(output_)output_->pause(false);if(callback_)callback_({++status_sequence_,stage_});condition_.notify_all();}
    void stop() override {std::lock_guard lock(mutex_);++generation_;queue_.clear();active_=false;paused_=false;if(output_)output_->stop();if(callback_)callback_({++status_sequence_,"Stoppad"});condition_.notify_all();}
};
}
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& data,SpeechStatus status) {return std::make_unique<PortableSpeech>(data,std::move(status));}
bool render_speech_probe(const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto begin=std::chrono::steady_clock::now();
    Model model(data/"voices"/voice_pack_id);
    const auto loaded=std::chrono::steady_clock::now();
    auto samples=model.generate("Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar.","sv",[]{return false;});
    chatterbox::write_wav_mono16(utf8(output),chatterbox::Audio{24000,samples});
    std::cout<<"Portable Swedish PCM: "<<samples.size()<<" frames, audio_seconds="<<samples.size()/24000.0
             <<", load_seconds="<<std::chrono::duration<double>(loaded-begin).count()
             <<", generation_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-loaded).count()<<'\n';
    return true;
}
}
