#pragma once
#include "speech/speech.hpp"
#include <algorithm>
#include <cstdint>
namespace ortho {
// Verse boundaries use real PCM offsets. Movement inside a verse is an
// estimate from text weight; the model does not supply word alignment.
class PlaybackTimeline {
    struct Part {std::optional<SpeechCue> cue;uint64_t begin,end;double from,to;};
    std::vector<Part> parts_;
    uint64_t frames_=0;
    size_t total_=0;
public:
    void reset(size_t total){parts_.clear();frames_=0;total_=total;}
    void append(std::optional<SpeechCue> cue,size_t frames,size_t gap,double from,double to) {
        parts_.push_back({std::move(cue),frames_+gap,frames_+frames,from,to});frames_+=frames;
    }
    SpeechPlayback at(uint64_t played,SpeechState state) const {
        SpeechPlayback result;result.state=state;
        if(parts_.empty()||played==0)return result;
        size_t index=0;
        while(index+1<parts_.size()&&played>parts_[index+1].begin)++index;
        const auto& part=parts_[index];
        const double fraction=std::clamp(double(played>part.begin?played-part.begin:0)/std::max<uint64_t>(1,part.end-part.begin),0.0,1.0);
        result.cue=part.cue;result.verse_progress=part.from+(part.to-part.from)*fraction;
        result.progress=total_?std::clamp((index+fraction)/total_,0.0,1.0):0;
        return result;
    }
};
}
