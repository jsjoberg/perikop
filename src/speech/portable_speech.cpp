#include "speech/portable_speech.hpp"
#include "speech/kokoro_audio.hpp"
#include "speech/kokoro_text.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "speech/playback_timeline.hpp"
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
std::string voice_name(const std::string& voice) {return voice=="bjorn"?"Björn":"Alice";}
// Swedish reads with Alice or Björn; Chatterbox reads Greek and English.
class Kokoro {
    KokoroText text_;
    KokoroAudio audio_;
    static const std::filesystem::path& checked(const std::filesystem::path& path) {
        for(const char* file:{"kokoro.onnx","g2p-encoder.onnx","g2p-decoder.onnx","g2p-config.json","config.json","alice.bin","bjorn.bin","lexicon.tsv","custom_lexicon.tsv"})
            if(!std::filesystem::is_regular_file(path/file))
                throw std::runtime_error("Röstpaketet för Alice och Björn saknas eller är ofullständigt. Installera det för att lyssna offline.");
        return path;
    }
public:
    explicit Kokoro(const std::filesystem::path& path):text_(checked(path)),audio_(path){}
    std::vector<float> generate(const std::string& text,const std::string& voice,const std::function<bool()>& cancelled) {
        const auto tokens=text_.tokens(text);
        // Text without speakable letters reads as a short pause.
        if(tokens.empty())return std::vector<float>(4800,0.0f);
        // The voice context holds 510 tokens. Longer text splits at a word gap (token 16).
        std::vector<float> result;
        for(size_t begin=0;begin<tokens.size();) {
            if(cancelled())throw std::runtime_error("Speech cancelled");
            size_t end=tokens.size(),next=end;
            if(end-begin>510) {
                end=next=begin+510;
                for(auto at=begin+510;at>begin;--at)if(tokens[at]==16){end=at;next=at+1;break;}
            }
            const auto pcm=audio_.generate({tokens.begin()+begin,tokens.begin()+end},voice);
            result.insert(result.end(),pcm.begin(),pcm.end());
            begin=next;
        }
        return result;
    }
};
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
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<SpeechUtterance> queue_;
    std::atomic<uint64_t> generation_{0};
    uint64_t status_sequence_=0;
    bool shutdown_=false,paused_=false,active_=false;
    bool output_started_=false;
    SpeechState state_=SpeechState::Idle;
    double ready_=0,speed_=1;
    std::string voice_="alice";
    PlaybackTimeline timeline_;
    std::string stage_="Förbereder läsningen";
    std::unique_ptr<PcmOutput> output_;
    std::thread worker_;
    void report(uint64_t generation,const std::string& status,SpeechState state=SpeechState::Buffering) {
        std::lock_guard lock(mutex_);
        if(shutdown_||generation!=generation_.load())return;
        stage_=status;
        state_=state;
        if(callback_)callback_({++status_sequence_,paused_?"Pausad":status});
    }
    void run() {
        std::unique_ptr<Model> model;
        std::unique_ptr<Kokoro> kokoro;
        std::unique_ptr<SpeechCache> cache;
        // Measured synthesis speed for this session. Until the model has run,
        // assume it is slower than real time, as on the reference hardware.
        double model_seconds=0,model_audio=0;
        while(true) {
            std::deque<SpeechUtterance> batch;
            uint64_t generation;
            std::string voice;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock,[this]{return shutdown_||!queue_.empty();});
                if(shutdown_)return;
                generation=generation_.load();voice=voice_;batch.swap(queue_);active_=true;output_started_=false;state_=SpeechState::Buffering;ready_=0;
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
                struct Part {std::string language,text;std::optional<SpeechCue> cue;double from,to,weight;};
                std::vector<Part> parts;
                for(const auto& utterance:batch) {
                    auto chunks=speech_chunks(utterance.speech_text);
                    double total=0,used=0;
                    for(const auto& text:chunks)total+=speech_text_weight(text);
                    for(auto& text:chunks) {
                        const auto weight=speech_text_weight(text);
                        parts.push_back({utterance.language,std::move(text),utterance.cue,used/std::max(1.0,total),(used+weight)/std::max(1.0,total),weight});
                        used+=weight;
                    }
                }
                {std::lock_guard lock(mutex_);if(cancelled())continue;timeline_.reset(parts.size());}
                double remaining_weight=0,produced_weight=0,produced_seconds=0;
                std::string playing;
                for(const auto& part:parts)remaining_weight+=part.weight;
                // Generate ahead as fast as possible, also while paused, up to
                // ten minutes of audio or the buffer's slot count.
                const auto full=[&]{return output_->buffered()>=PcmStream::capacity||output_->buffered_frames()>=600*24000;};
                // Called with the mutex held after audio is queued or played.
                // A full buffer cannot grow, so it plays whatever the estimate says.
                const auto update_gate=[&] {
                    const double seconds_per_weight=produced_seconds/std::max(1e-9,produced_weight);
                    const double factor=model_audio>0?model_seconds/model_audio:2.5;
                    const double target=buffer_target(remaining_weight*seconds_per_weight,factor);
                    const double buffered=output_->buffered_frames()/24000.0;
                    ready_=std::min(1.0,buffered/target);
                    if(output_->buffered()&&(buffered>=target||full()))output_->release();
                };
                for(size_t i=0;i<parts.size();++i) {
                    {
                        std::unique_lock lock(mutex_);
                        while(!cancelled()&&i&&full()) {
                            update_gate();condition_.wait_for(lock,100ms);
                        }
                        if(cancelled())break;
                    }
                    const auto& part=parts[i];const auto& language=part.language;const auto& text=part.text;
                    const auto progress=std::to_string(i+1)+"/"+std::to_string(parts.size());
                    const bool swedish=language=="sv";
                    const auto name=swedish?voice_name(voice):std::string("Chatterbox");
                    const auto revision=swedish?std::string(kokoro_pack_id)+"/ort1.23.2/"+voice+"/v1":cache_revision;
                    playing="Läser med "+name+(swedish?"":" · förhandsversion");
                    report(generation,i?"Läser med "+name+" · förbereder del "+progress:"Förbereder läsningen · "+progress);
                    auto samples=cache->load(revision,language,text);
                    if(!samples){
                        if(swedish?!kokoro:!model) {
                            report(generation,"Förbereder röstmodellen · "+name,SpeechState::Loading);
                            if(swedish)kokoro=std::make_unique<Kokoro>(data_/"voices"/kokoro_pack_id);
                            else model=std::make_unique<Model>(data_/"voices"/voice_pack_id);
                        }
                        report(generation,i?"Läser med "+name+" · förbereder del "+progress:"Förbereder läsningen · "+progress);
                        const auto begin=std::chrono::steady_clock::now();
                        samples=swedish?kokoro->generate(text,voice,cancelled):model->generate(text,language,cancelled);
                        if(cancelled())break;
                        model_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
                        model_audio+=samples->size()/24000.0;
                        cache->save(revision,language,text,*samples);
                    }
                    if(i)samples->insert(samples->begin(),4800,0.0f);
                    {
                        std::lock_guard lock(mutex_);
                        if(cancelled())break;
                        if(!output_){output_=std::make_unique<PcmOutput>();output_->set_speed(speed_);}
                        if(!i)output_->start(paused_);
                        timeline_.append(part.cue,samples->size(),i?4800:0,part.from,part.to);
                        produced_seconds+=samples->size()/24000.0;produced_weight+=part.weight;remaining_weight-=part.weight;
                        output_->append(std::move(*samples));
                        output_started_=true;
                        update_gate();
                    }
                    report(generation,playing,SpeechState::Playing);
                }
                {
                    std::lock_guard lock(mutex_);
                    if(cancelled())continue;
                    if(parts.empty())throw std::runtime_error("Ingen text finns att läsa upp.");
                    output_->complete();ready_=1;
                }
                report(generation,playing,SpeechState::Playing);
                {
                    std::unique_lock lock(mutex_);
                    while(!shutdown_&&!cancelled()&&!output_->finished())condition_.wait_for(lock,100ms);
                    if(shutdown_)return;
                    if(cancelled())continue;
                    active_=false;paused_=false;
                }
                report(generation,"Läsningen är klar",SpeechState::Completed);
            } catch(const std::exception& error) {
                {
                    std::lock_guard lock(mutex_);
                    if(cancelled())continue;
                    queue_.clear();active_=false;paused_=false;output_started_=false;
                    if(output_)output_->stop();
                }
                report(generation,std::string("Uppläsning: ")+error.what(),SpeechState::Error);
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
        for(const auto& utterance:utterances) {
            if(utterance.language!="sv"&&utterance.language!="el"&&utterance.language!="en")throw std::runtime_error("Röstpaketet stöder svenska, grekiska och engelska.");
            if(utterance.language=="sv"&&!std::filesystem::exists(data_/"voices"/kokoro_pack_id/"kokoro.onnx"))throw std::runtime_error("Röstpaketet för Alice och Björn saknas. Installera det för att lyssna offline.");
            if(utterance.language!="sv"&&!std::filesystem::exists(data_/"voices"/voice_pack_id/"tokenizer.json"))throw std::runtime_error("Röstpaketet saknas. Installera Chatterbox-röstpaketet för att lyssna offline.");
        }
        {std::lock_guard lock(mutex_);queue_.insert(queue_.end(),utterances.begin(),utterances.end());if(!active_){state_=SpeechState::Buffering;output_started_=false;}active_=true;}
        condition_.notify_all();
    }
    void pause() override {std::lock_guard lock(mutex_);if(!active_)return;paused_=true;if(output_)output_->pause(true);if(callback_)callback_({++status_sequence_,"Pausad"});}
    void resume() override {std::lock_guard lock(mutex_);if(!active_)return;paused_=false;if(output_)output_->pause(false);if(callback_)callback_({++status_sequence_,stage_});condition_.notify_all();}
    void stop() override {std::lock_guard lock(mutex_);++generation_;queue_.clear();active_=false;paused_=false;output_started_=false;state_=SpeechState::Stopped;if(output_)output_->stop();if(callback_)callback_({++status_sequence_,"Stoppad"});condition_.notify_all();}
    void set_speed(double speed) override {std::lock_guard lock(mutex_);speed_=speed;if(output_)output_->set_speed(speed);}
    void set_voice(const std::string& voice) override {std::lock_guard lock(mutex_);voice_=voice;}
    SpeechPlayback playback() const override {
        std::lock_guard lock(mutex_);
        if(!output_started_)return {paused_?SpeechState::Paused:state_,{},0,0};
        const auto position=output_->progress();
        auto state=state_;
        if(paused_)state=SpeechState::Paused;
        else if(active_)state=position.finished?SpeechState::Completed:position.waiting?SpeechState::Buffering:SpeechState::Playing;
        auto result=timeline_.at(position.played,state);
        result.ready=ready_;
        return result;
    }
};
}
std::unique_ptr<SpeechEngine> create_portable_speech(const std::filesystem::path& data,SpeechStatus status) {return std::make_unique<PortableSpeech>(data,std::move(status));}
bool render_speech_probe(const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto begin=std::chrono::steady_clock::now();
    Kokoro model(data/"voices"/kokoro_pack_id);
    const auto loaded=std::chrono::steady_clock::now();
    auto samples=model.generate("Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar.","alice",[]{return false;});
    chatterbox::write_wav_mono16(utf8(output),chatterbox::Audio{24000,samples});
    std::cout<<"Portable Swedish PCM: "<<samples.size()<<" frames, audio_seconds="<<samples.size()/24000.0
             <<", load_seconds="<<std::chrono::duration<double>(loaded-begin).count()
             <<", generation_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-loaded).count()<<'\n';
    return true;
}
}
