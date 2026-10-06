#pragma once
#include <memory>
#include <cstdint>
#include <vector>
namespace ortho {
struct PcmProgress {uint64_t played=0;bool waiting=true,finished=false;};
// The platform supplies an audio device only. Voice inference stays in common C++.
class PcmOutput {
public:
    PcmOutput();
    ~PcmOutput();
    void start(bool paused);
    void append(std::vector<float> samples);
    void complete();
    size_t buffered() const;
    uint64_t buffered_frames() const;
    void release();
    void pause(bool);
    void stop();
    bool finished() const;
    PcmProgress progress() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
