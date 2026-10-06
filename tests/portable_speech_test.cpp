#include "speech/portable_speech.hpp"
#include "speech/pcm_output.hpp"
#include "speech/pcm_stream.hpp"
#include "storage/database.hpp"
#include "speech_voice_manifest.hpp"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
namespace {
// A manually drained audio device lets the real speech worker run without
// sound, model inference, or timing assumptions about physical playback.
std::mutex audio_mutex;
std::condition_variable changed;
std::shared_ptr<ortho::PcmStream> stream;
size_t appended=0;
bool paused=false;
std::string status;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Predicate> void wait_for(Predicate predicate,const char* message) {
    std::unique_lock lock(audio_mutex);
    check(changed.wait_for(lock,std::chrono::seconds(3),predicate),message);
}
std::vector<float> drain() {
    std::vector<float> samples(6000);
    {
        std::lock_guard lock(audio_mutex);
        if(stream&&!paused)stream->render(samples.data(),samples.size());
    }
    changed.notify_all();
    std::erase(samples,0.0f);
    return samples;
}
}
namespace ortho {
struct PcmOutput::Impl {};
PcmOutput::PcmOutput():impl_(std::make_unique<Impl>()){}
PcmOutput::~PcmOutput()=default;
void PcmOutput::start(bool pause) {
    std::lock_guard lock(audio_mutex);stream=std::make_shared<PcmStream>();paused=pause;appended=0;
}
void PcmOutput::append(std::vector<float> samples) {
    std::lock_guard lock(audio_mutex);
    check(stream&&stream->append(std::move(samples)),"Speech worker overfilled or closed the audio buffer");
    ++appended;changed.notify_all();
}
void PcmOutput::complete(){std::lock_guard lock(audio_mutex);stream->complete();changed.notify_all();}
size_t PcmOutput::buffered() const{std::lock_guard lock(audio_mutex);return stream?stream->buffered():0;}
void PcmOutput::pause(bool pause){std::lock_guard lock(audio_mutex);paused=pause;changed.notify_all();}
void PcmOutput::stop(){std::lock_guard lock(audio_mutex);stream.reset();paused=false;changed.notify_all();}
bool PcmOutput::finished() const{std::lock_guard lock(audio_mutex);return !stream||stream->finished();}
}
int main(int argc,char** argv) {
    try {
        using namespace ortho;
        check(argc==2,"Usage: ortho-portable-speech-tests TEST-DIRECTORY");
        const std::filesystem::path data(argv[1]);
        std::filesystem::remove_all(data);
        const auto voice=data/"voices"/voice_pack_id;
        std::filesystem::create_directories(voice);
        std::ofstream(voice/"tokenizer.json")<<"{}";
        // Cache-only tests: any accidental upfront inference fails because
        // the remaining model files deliberately do not exist.
        const std::string revision=std::string(voice_pack_id)+"/ort1.23.2/utf8proc2.10/seed42/cfg0.5/t0.8/v1";
        {
            SpeechCache cache(data/"speech-cache.db");
            cache.save(revision,"sv","Första.",{1,2,3});
            cache.save(revision,"sv","Andra.",{4,5,6});
            cache.save(revision,"sv","Tredje.",{7,8,9});
            cache.save(revision,"sv","Ny läsning.",{10,11,12});
        }
        auto engine=create_portable_speech(data,[](const SpeechUpdate& update){
            std::lock_guard lock(audio_mutex);status=update.text;changed.notify_all();
        });
        const std::vector<SpeechUtterance> reading={{"","Första.","sv"},{"","Andra.","sv"},{"","Tredje.","sv"}};
        engine->speak_batch(reading);
        wait_for([]{return appended==2&&status.starts_with("Läser");},"Playback must start before the whole reading is ready");
        {
            std::lock_guard lock(audio_mutex);
            check(stream&&stream->buffered()==2&&!stream->finished(),"Lookahead must stop at two unplayed chunks");
        }
        engine->pause();
        check(drain().empty(),"Pause must preserve the playback position");
        {
            std::lock_guard lock(audio_mutex);
            check(appended==2&&stream->buffered()==2&&status=="Pausad","Pause must bound synthesis and report the paused state");
        }
        engine->resume();
        auto heard=drain();
        wait_for([]{return appended==3;},"Synthesis must resume when playback makes room");
        auto last=drain();heard.insert(heard.end(),last.begin(),last.end());
        wait_for([]{return status=="Läsningen är klar";},"Completion must wait for final audio consumption");
        check(heard==std::vector<float>{1,2,3,4,5,6,7,8,9},"The worker must play all chunks in order");

        engine->speak_batch(reading);
        wait_for([]{return appended==2&&!stream->finished();},"Second reading starts with bounded lookahead");
        engine->pause();engine->stop();
        {
            std::lock_guard lock(audio_mutex);
            check(!stream&&status=="Stoppad","Stop must discard all queued audio");
        }
        engine->speak({"","Ny läsning.","sv"});
        wait_for([]{return stream&&appended==1&&status.starts_with("Läser");},"A new reading must start after cancellation");
        check(drain()==std::vector<float>{10,11,12},"Cancelled audio and paused state must not leak into a new reading");
        wait_for([]{return status=="Läsningen är klar";},"The replacement reading must complete");

        // A missing later chunk must not prevent the cached first chunk from
        // reaching playback. The model then fails validation deterministically.
        engine->speak_batch({reading[0],{"","Ej cachad.","sv"}});
        wait_for([]{return status.starts_with("Uppläsning:");},"A later synthesis failure must report an error");
        {
            std::lock_guard lock(audio_mutex);
            check(appended==1&&!stream,"First audio must start before later inference, and errors must stop playback");
        }
        engine.reset();std::filesystem::remove_all(data);
        std::cout<<"Portable speech streaming checks passed\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
