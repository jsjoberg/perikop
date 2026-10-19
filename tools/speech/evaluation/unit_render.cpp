// Original MIT code. Experimental native renderer for offline-selected speech.
// No neural model, text frontend, or signal-processing dependency at playback.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
uint64_t get(std::istream& in, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i=0; i<bytes; ++i) {
        const auto c=in.get();
        if (c==std::char_traits<char>::eof()) throw std::runtime_error("Truncated speech plan");
        value |= uint64_t(static_cast<unsigned char>(c)) << (8*i);
    }
    return value;
}
void put(std::ostream& out, uint32_t value, unsigned bytes) {
    for (unsigned i=0; i<bytes; ++i) out.put(char(value>>(8*i)));
}
void fade(std::vector<float>& pcm, size_t begin) {
    const size_t count=std::min<size_t>(120,(pcm.size()-begin)/2);
    for (size_t i=0;i<count;++i) {
        pcm[begin+i] *= float(i)/float(count);
        pcm[pcm.size()-1-i] *= float(i)/float(count);
    }
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=4) throw std::runtime_error("Usage: unit-render BANK.u8 PLAN OUTPUT.wav");
        const auto started=std::chrono::steady_clock::now();
        std::ifstream bank(argv[1],std::ios::binary),plan(argv[2],std::ios::binary);
        if (!bank || !plan) throw std::runtime_error("Cannot read voice bank or plan");
        const auto bank_size=std::filesystem::file_size(argv[1]);
        if (get(plan,4)!=0x31535542) throw std::runtime_error("Invalid plan format");
        const auto spans=get(plan,4);
        if (spans>65536) throw std::runtime_error("Speech plan too large");
        std::array<float,256> amplitudes;
        for (size_t i=0;i<256;++i) {
            const double u=double(i)/127.5-1;
            amplitudes[i]=float(std::copysign(std::expm1(std::abs(u)*std::log(256))/255,u));
        }
        const auto loaded=std::chrono::steady_clock::now();
        std::vector<float> pcm;
        pcm.reserve(24000*30);
        size_t clause=0,joins=0;
        for (uint64_t span=0;span<spans;++span) {
            const auto offset=get(plan,8),count=get(plan,4),overlap=get(plan,4);
            if (count>24000*120 || overlap>count || overlap>pcm.size()-clause || pcm.size()+count>24000*600)
                throw std::runtime_error("Invalid speech span size");
            if (offset==std::numeric_limits<uint64_t>::max()) {
                if (overlap) throw std::runtime_error("Invalid silence span");
                fade(pcm,clause);
                pcm.resize(pcm.size()+count,0.f);
                clause=pcm.size();
                continue;
            }
            if (offset>bank_size || count>bank_size-offset) throw std::runtime_error("Span outside voice bank");
            std::vector<unsigned char> codes(count);
            bank.seekg(static_cast<std::streamoff>(offset));
            if (!bank.read(reinterpret_cast<char*>(codes.data()),static_cast<std::streamsize>(count)))
                throw std::runtime_error("Cannot read waveform span");
            if (overlap) ++joins;
            for (size_t i=0;i<overlap;++i) {
                const float t=float(i+1)/float(overlap+1);
                pcm[pcm.size()-overlap+i]=pcm[pcm.size()-overlap+i]*(1-t)+amplitudes[codes[i]]*t;
            }
            for (size_t i=overlap;i<count;++i) pcm.push_back(amplitudes[codes[i]]);
        }
        if (plan.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Trailing plan data");
        fade(pcm,clause);
        const auto rendered=std::chrono::steady_clock::now();
        std::ofstream out(argv[3],std::ios::binary);
        out.write("RIFF",4);put(out,36+uint32_t(pcm.size()*2),4);out.write("WAVEfmt ",8);
        put(out,16,4);put(out,1,2);put(out,1,2);put(out,24000,4);put(out,48000,4);put(out,2,2);put(out,16,2);
        out.write("data",4);put(out,uint32_t(pcm.size()*2),4);
        for (const auto v:pcm) {
            if (!std::isfinite(v) || std::abs(v)>1) throw std::runtime_error("Invalid PCM");
            put(out,uint16_t(int16_t(std::lround(v*32767))),2);
        }
        if (!out) throw std::runtime_error("Cannot write WAV");
        std::cout<<"{\"load_seconds\":"<<std::chrono::duration<double>(loaded-started).count()
                 <<",\"render_seconds\":"<<std::chrono::duration<double>(rendered-loaded).count()
                 <<",\"audio_seconds\":"<<pcm.size()/24000.0<<",\"joins\":"<<joins<<"}\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
