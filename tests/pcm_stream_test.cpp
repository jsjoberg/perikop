#include "speech/pcm_stream.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main() {
    try {
        ortho::PcmStream stream;
        float output[4];
        check(!stream.finished(),"An empty buffer must wait for synthesis, not finish playback");
        check(stream.append({1,2,3}),"First chunk accepted");
        stream.render(output,2);
        check(output[0]==1&&output[1]==2,"Playback starts before later chunks or completion");
        stream.render(output,4);
        check(output[0]==3&&output[1]==0&&output[2]==0&&output[3]==0,"Underrun supplies silence after the available samples");
        check(!stream.finished(),"Underrun must not end an unfinished reading");
        check(stream.append({4,5})&&stream.append({6,7}),"Two chunks of lookahead accepted");
        std::vector<float> pending{8,9};
        check(!stream.append(std::move(pending))&&pending==std::vector<float>{8,9},"Full buffer preserves the pending chunk");
        stream.render(output,4);
        check(std::vector<float>(output,output+4)==std::vector<float>{4,5,6,7},"Resume after underrun preserves order across chunk boundaries");
        check(stream.append(std::move(pending)),"Consumed slots can be reused");
        stream.complete();
        check(!stream.finished()&&!stream.append({10}),"Completion drains queued audio and rejects new chunks");
        stream.render(output,4);
        check(output[0]==8&&output[1]==9&&output[2]==0&&stream.finished(),"Finish only after the final samples play");

        // Exercise publication and slot reuse with actual concurrent producer
        // and consumer threads, including partial and multi-chunk callbacks.
        ortho::PcmStream concurrent;
        constexpr size_t chunks=20000;
        std::atomic<bool> abort{false};
        std::jthread producer([&]{
            size_t sample=1;
            for(size_t i=0;i<chunks&&!abort.load();++i) {
                std::vector<float> samples(1+i%17);
                for(auto& value:samples)value=static_cast<float>(sample++);
                while(!concurrent.append(std::move(samples))&&!abort.load())std::this_thread::yield();
            }
            concurrent.complete();
        });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        size_t expected=1;
        bool ordered=true,timed_out=false;
        while(!concurrent.finished()) {
            float block[31];concurrent.render(block,31);
            for(auto value:block)if(value!=0){if(value!=static_cast<float>(expected))ordered=false;++expected;}
            if(std::chrono::steady_clock::now()>deadline){abort=true;timed_out=true;break;}
            std::this_thread::yield();
        }
        producer.join();
        size_t total=0;for(size_t i=0;i<chunks;++i)total+=1+i%17;
        check(!timed_out&&ordered&&expected==total+1,"Concurrent streaming must play every sample exactly once in order");
        std::cout<<"PCM streaming checks passed\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
