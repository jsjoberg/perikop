#include "ui/scripture_view.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace ortho {
void ScriptureView::release_follow() {
    if (!following_)
        return;
    following_ = false;
    follow_target_ = offset_;
    if (release_follow_)
        release_follow_();
}
void ScriptureView::follow_playback(bool follow) {
    following_ = follow;
    follow_target_ = offset_;
    if (follow) {
        wheel_timer_.Stop();
        locate_playback();
        if (guide_) {
            follow_target_ = guide_->y - GetClientSize().y * 0.38;
            if (playback_.state == SpeechState::Paused)
                set_position(follow_target_);
        }
        if (!follow_timer_.IsRunning())
            follow_timer_.Start(16);
    }
}
void ScriptureView::playback(const SpeechPlayback& playback) {
    const bool was_visible = bool(guide_);
    const bool cue_changed = playback_.cue != playback.cue;
    playback_ = playback;
    const bool active = speech_active(playback.state);
    if (active) {
        locate_playback();
        if (guide_ && (!was_visible || (cue_changed && playback.state == SpeechState::Paused)))
            guide_y_ = guide_->y;
    }
    if ((guide_ || guide_alpha_ > 0) && !follow_timer_.IsRunning())
        follow_timer_.Start(16);
    if (guide_ || was_visible)
        Refresh(false);
}
void ScriptureView::locate_playback() {
    if (!playback_.cue || playback_.cue->introduction || playback_.cue->book != displayed_.book) {
        guide_.reset();
        return;
    }
    const auto& cue = *playback_.cue;
    if (located_cue_ != playback_.cue) {
        located_cue_ = playback_.cue;
        speech_row_.reset();
        VerseRef first = cue.verse, last = cue.last;
        if (cue.source != frame_) {
            const auto mapped = corpus_.map_passage(cue.source, frame_, {cue.book, first, last});
            if (mapped.empty()) {
                guide_.reset();
                return;
            }
            first = mapped.front().first;
            last = mapped.front().last;
        }
        speech_range_ = {first, last};
        for (size_t i = 0; i < rows_.size(); ++i)
            if (!rows_[i].heading && rows_[i].ref <= last && first <= rows_[i].last) {
                speech_row_ = i;
                break;
            }
    }
    if (!speech_row_) {
        guide_.reset();
        return;
    }
    const auto index = *speech_row_;
    const auto anchor = first_visible();
    const double old = positions_[anchor];
    bool changed = false;
    for (size_t i = index > 2 ? index - 2 : 0; i <= std::min(index + 2, rows_.size() - 1); ++i) {
        const auto height = row_layout(i).height;
        changed |= heights_[i] != height;
        heights_[i] = height;
    }
    if (changed) {
        rebuild_positions();
        const auto adjustment = positions_[anchor] - old;
        offset_ += adjustment;
        target_ += adjustment;
        follow_target_ += adjustment;
        guide_y_ += adjustment;
    }
    const auto& layout = row_layout(index);
    const bool stacked = stacked_columns();
    double y = positions_[index] + FromDIP(12);
    for (size_t c = 0; c < layout.columns.size(); ++c) {
        const auto& column = layout.columns[c];
        if (stacked && layout.columns.size() > 1)
            y += FromDIP(24);
        if (c == 0) {
            Guide guide{index, c, 0, y, column.text.line_height, {}};
            double weight = 0;
            for (size_t line = 0; line < column.text.lines.size(); ++line) {
                GuideSpan span{line, std::numeric_limits<double>::max(), 0, 0};
                for (const auto& run : column.text.lines[line].runs) {
                    if (!column.main_text(run.tag))
                        continue;
                    const auto& [first, last] = column.verses[run.tag];
                    if (speech_range_.first > last || speech_range_.second < first)
                        continue;
                    span.first = std::min(span.first, run.x);
                    span.last = std::max(span.last, run.x + run.width);
                    if (!run.marker)
                        span.weight += speech_text_weight(run.text.ToStdString(wxConvUTF8));
                }
                if (span.last > span.first) {
                    weight += span.weight;
                    guide.spans.push_back(span);
                }
            }
            if (guide.spans.empty()) {
                guide_.reset();
                return;
            }
            double remaining = std::clamp(playback_.verse_progress, 0.0, 1.0) * weight;
            guide.line = guide.spans.back().line;
            for (const auto& span : guide.spans) {
                if (remaining < span.weight) {
                    guide.line = span.line;
                    break;
                }
                remaining -= span.weight;
            }
            guide.y = y + (guide.line + 0.5) * guide.height;
            guide_ = std::move(guide);
            return;
        }
        if (stacked)
            y += column.text.height();
    }
    guide_.reset();
}
void ScriptureView::advance_playback(double seconds) {
    if (!IsShown())
        return;
    const bool active = speech_active(playback_.state);
    const auto ease = [seconds](double rate) {
        return 1 - std::exp(-std::clamp(seconds, 0.0, 0.1) * rate);
    };
    if (active)
        locate_playback();
    guide_alpha_ += ((active && guide_ ? 1.0 : 0.0) - guide_alpha_) * ease(14);
    // The marker and page hold still during pause and a genuine audio underrun.
    if (guide_ && playback_.state == SpeechState::Playing) {
        guide_y_ += (guide_->y - guide_y_) * ease(16);
        if (following_) {
            const double height = GetClientSize().y, screen = guide_->y - offset_;
            if (screen > height * 0.56 || screen < height * 0.22)
                follow_target_ = guide_->y - height * 0.38;
            follow_target_ = std::clamp(follow_target_, 0.0, max_offset());
            if (std::abs(follow_target_ - offset_) > 0.1) {
                set_position(offset_ + (follow_target_ - offset_) * ease(8));
                target_ = offset_;
            }
        }
    }
    if (guide_alpha_ < 0.01 && (!active || !guide_)) {
        guide_.reset();
        guide_alpha_ = 0;
        follow_timer_.Stop();
    }
    Refresh(false);
}
} // namespace ortho
