// Original MIT implementation of prepared pitch-synchronous overlap-add.
// Input sound is the compact engine's output; controls contain no teacher PCM.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
uint32_t get(std::istream& in,unsigned bytes) {
    uint32_t value=0;
    for(unsigned i=0;i<bytes;++i) {
        const int c=in.get();
        if(c==std::char_traits<char>::eof())throw std::runtime_error("Truncated input");
        value|=uint32_t(static_cast<unsigned char>(c))<<(8*i);
    }
    return value;
}
void put(std::ostream& out,uint32_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i)out.put(char(value>>(8*i)));
}
std::vector<double> read_wave(const char* path) {
    std::ifstream in(path,std::ios::binary);
    if(!in || get(in,4)!=0x46464952)throw std::runtime_error("Expected RIFF input");
    get(in,4);
    if(get(in,4)!=0x45564157)throw std::runtime_error("Expected WAVE input");
    bool format=false;
    while(in.peek()!=std::char_traits<char>::eof()) {
        const auto chunk=get(in,4),bytes=get(in,4);
        if(bytes>24000*600*2)throw std::runtime_error("WAV chunk too large");
        if(chunk==0x20746d66) {
            if(bytes<16 || get(in,2)!=1 || get(in,2)!=1 || get(in,4)!=24000)
                throw std::runtime_error("Expected mono 24 kHz PCM16");
            get(in,4);get(in,2);
            if(get(in,2)!=16)throw std::runtime_error("Expected PCM16");
            in.ignore(bytes-16+(bytes&1));format=true;
        } else if(chunk==0x61746164) {
            if(!format || !bytes || bytes%2)throw std::runtime_error("Invalid PCM data");
            std::vector<unsigned char> raw(bytes);
            if(!in.read(reinterpret_cast<char*>(raw.data()),bytes))throw std::runtime_error("Truncated PCM data");
            std::vector<double> pcm(bytes/2);
            for(size_t i=0;i<pcm.size();++i)
                pcm[i]=static_cast<int16_t>(uint16_t(raw[2*i])|(uint16_t(raw[2*i+1])<<8))/32768.0;
            return pcm;
        } else in.ignore(bytes+(bytes&1));
        if(!in)throw std::runtime_error("Truncated WAV chunk");
    }
    throw std::runtime_error("WAV data missing");
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::runtime_error("Usage: grain-render COMPACT.wav PLAN.grains OUTPUT.wav");
        const auto started=std::chrono::steady_clock::now();
        const auto pcm=read_wave(argv[1]);
        std::ifstream plan(argv[2],std::ios::binary);
        if(!plan || get(plan,4)!=0x31504742 || get(plan,4)!=24000)
            throw std::runtime_error("Invalid grain plan");
        const auto length=get(plan,4),grains=get(plan,4);
        if(!length || length>24000*600 || !grains || grains>1000000)
            throw std::runtime_error("Invalid grain plan size");
        std::vector<double> out(length,0),normalizer(length,0);
        const auto loaded=std::chrono::steady_clock::now();
        constexpr double pi=3.14159265358979323846;
        std::array<std::vector<double>,1001> windows;
        const auto window_for=[&](uint32_t radius)->const std::vector<double>& {
            auto& window=windows[radius];
            if(window.empty()) {
                window.resize(radius+1);
                for(uint32_t i=0;i<=radius;++i)window[i]=.5+.5*std::cos(pi*i/radius);
            }
            return window;
        };
        for(uint32_t grain=0;grain<grains;++grain) {
            const auto source=get(plan,4),destination=get(plan,4),left=get(plan,2),right=get(plan,2);
            const auto bits=get(plan,4);float gain;
            static_assert(sizeof(gain)==sizeof(bits));std::memcpy(&gain,&bits,sizeof(gain));
            if(source>=pcm.size() || destination>=length || !left || left>1000 || !right || right>1000 ||
               !std::isfinite(gain) || gain<0 || gain>2)
                throw std::runtime_error("Invalid grain extent or gain");
            const int begin=std::max({-int(left),-int(source),-int(destination)});
            const int end=std::min({int(right),int(pcm.size()-1-source),int(length-1-destination)});
            const auto& left_window=window_for(left);
            const auto& right_window=window_for(right);
            for(int delta=begin;delta<=end;++delta) {
                const double window=delta<=0?left_window[size_t(-delta)]:right_window[size_t(delta)];
                const auto si=static_cast<size_t>(int(source)+delta),di=static_cast<size_t>(int(destination)+delta);
                out[di]+=pcm[si]*window*gain;normalizer[di]+=window;
            }
        }
        if(plan.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing plan bytes");
        size_t uncovered=0;
        for(size_t i=0;i<out.size();++i) {
            if(normalizer[i]>1e-8)out[i]/=normalizer[i];else ++uncovered;
            if(!std::isfinite(out[i]) || std::abs(out[i])>1)throw std::runtime_error("Invalid output PCM");
        }
        const auto rendered=std::chrono::steady_clock::now();
        std::ofstream file(argv[3],std::ios::binary);
        file.write("RIFF",4);put(file,36+uint32_t(out.size()*2),4);file.write("WAVEfmt ",8);
        put(file,16,4);put(file,1,2);put(file,1,2);put(file,24000,4);put(file,48000,4);put(file,2,2);put(file,16,2);
        file.write("data",4);put(file,uint32_t(out.size()*2),4);
        for(const auto sample:out)put(file,uint16_t(int16_t(std::lround(sample*32767))),2);
        if(!file)throw std::runtime_error("Cannot write grain output");
        std::cout<<"{\"load_seconds\":"<<std::chrono::duration<double>(loaded-started).count()
                 <<",\"render_seconds\":"<<std::chrono::duration<double>(rendered-loaded).count()
                 <<",\"audio_seconds\":"<<out.size()/24000.0<<",\"grains\":"<<grains
                 <<",\"uncovered_samples\":"<<uncovered<<"}\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
