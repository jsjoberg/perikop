#pragma once
#include <memory>
#include <vector>
namespace ortho {
// The platform supplies an audio device only. Voice inference stays in common C++.
class PcmOutput {
public:
    PcmOutput();
    ~PcmOutput();
    void play(std::vector<float> samples);
    void pause(bool);
    void stop();
    bool finished() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
