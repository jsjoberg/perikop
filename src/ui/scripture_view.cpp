#include "ui/scripture_view.hpp"
#include <algorithm>
#include <cmath>
#ifdef __APPLE__
#include "ui/native_scroll.hpp"
#endif
namespace ortho {
ScriptureView::ScriptureView(wxWindow* parent, const CorpusDb& corpus)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS | wxVSCROLL),
      follow_timer_(this), wheel_timer_(this), corpus_(corpus) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetName(wxString::FromUTF8("Kontinuerlig skriftläsare"));
    Bind(wxEVT_PAINT, &ScriptureView::paint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        if (layout_width_ != GetClientSize().x)
            invalidate();
        scrollbar();
        e.Skip();
    });
    Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent& e) {
        if (e.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
            e.Skip();
            return;
        }
        release_follow();
#ifdef __APPLE__
        scroll_by(-e.GetWheelRotation()); // fallback; Cocoa monitor preserves fractional deltas
#else
        const double amount=-double(e.GetWheelRotation())/std::max(1,e.GetWheelDelta())*
            (e.IsPageScroll()?GetClientSize().y*0.85:std::max(1,e.GetLinesPerAction())*FromDIP(30));
        target_=std::clamp(target_+amount,0.0,max_offset());
        wheel_timer_.Start(16);
#endif
    });
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            const double distance = target_ - offset_;
            if (std::abs(distance) < 0.5) {
                set_position(target_);
                wheel_timer_.Stop();
            } else
                set_position(offset_ + distance * 0.3);
        },
        wheel_timer_.GetId());
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            advance_playback(0.016);
        },
        follow_timer_.GetId());
    const auto scroll_event = [this](wxScrollWinEvent& e) {
        release_follow();
        wheel_timer_.Stop();
        if (e.GetEventType() == wxEVT_SCROLLWIN_LINEUP)
            scroll_by(-FromDIP(30));
        else if (e.GetEventType() == wxEVT_SCROLLWIN_LINEDOWN)
            scroll_by(FromDIP(30));
        else if (e.GetEventType() == wxEVT_SCROLLWIN_PAGEUP)
            scroll_by(-GetClientSize().y * 0.85);
        else if (e.GetEventType() == wxEVT_SCROLLWIN_PAGEDOWN)
            scroll_by(GetClientSize().y * 0.85);
        else {
            set_position(e.GetPosition());
            target_ = offset_;
        }
    };
    for (const auto type : {wxEVT_SCROLLWIN_LINEUP, wxEVT_SCROLLWIN_LINEDOWN, wxEVT_SCROLLWIN_PAGEUP,
                            wxEVT_SCROLLWIN_PAGEDOWN, wxEVT_SCROLLWIN_THUMBTRACK,
                            wxEVT_SCROLLWIN_THUMBRELEASE, wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM})
        Bind(type, scroll_event);
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        const int key = e.GetKeyCode();
        if (key == WXK_HOME) {
            release_follow();
            set_position(0);
            return;
        }
        if (key == WXK_END) {
            release_follow();
            set_position(max_offset());
            return;
        }
        if (key == WXK_DOWN) {
            scroll_by(FromDIP(30));
            return;
        }
        if (key == WXK_UP) {
            scroll_by(-FromDIP(30));
            return;
        }
        if (key == WXK_PAGEDOWN) {
            scroll_by(GetClientSize().y * 0.85);
            return;
        }
        if (key == WXK_PAGEUP) {
            scroll_by(-GetClientSize().y * 0.85);
            return;
        }
        e.Skip();
    });
    // Dragging across verses marks them; a plain click clears the mark.
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        SetFocus();
        return_pressed_ = return_rect_.Contains(e.GetPosition());
        if (return_pressed_)
            return;
        drag_anchor_ = verse_at(e.GetPosition());
        dragged_ = false;
        if (drag_anchor_ && !HasCapture())
            CaptureMouse();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
        hover_return(!drag_anchor_ && return_rect_.Contains(e.GetPosition()));
        if (!drag_anchor_ || !e.LeftIsDown())
            return;
        const auto verse = verse_at(e.GetPosition());
        if (!verse || (!dragged_ && *verse == *drag_anchor_ && !selection_))
            return;
        dragged_ = true;
        select_verses(*drag_anchor_, *verse);
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (HasCapture())
            ReleaseMouse();
        if (return_pressed_) {
            return_pressed_ = false;
            if (return_rect_.Contains(e.GetPosition()) && returned_)
                returned_();
            return;
        }
        if (drag_anchor_ && !dragged_) {
            if (selection_)
                clear_selection();
            else if (word_clicked_)
                if (auto word = word_at(e.GetPosition()))
                    word_clicked_(*word);
        }
        drag_anchor_.reset();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
        hover_return(false);
        e.Skip();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) {
        drag_anchor_.reset();
    });
#ifdef __APPLE__
    native_scroll_ = install_native_scroll(this, [this](double pixels) {
        scroll_by(pixels);
    });
#endif
}
ScriptureView::~ScriptureView() {
    wheel_timer_.Stop();
    follow_timer_.Stop();
#ifdef __APPLE__
    remove_native_scroll(native_scroll_);
#endif
}
void ScriptureView::rebuild_positions() {
    positions_.assign(heights_.size() + 1, 0);
    for (std::size_t i = 0; i < heights_.size(); ++i)
        positions_[i + 1] = positions_[i] + heights_[i];
}
std::size_t ScriptureView::first_visible() const {
    if (rows_.empty())
        return 0;
    return std::min(rows_.size() - 1,
                    std::size_t(std::upper_bound(positions_.begin(), positions_.end(), offset_) -
                                positions_.begin() - 1));
}
void ScriptureView::prepare_visible() {
    if (rows_.empty())
        return;
    const auto anchor = first_visible();
    const double within = offset_ - positions_[anchor];
    const auto begin = anchor > 8 ? anchor - 8 : 0;
    double covered = 0;
    bool changed = false;
    for (std::size_t i = begin; i < rows_.size() && (i <= anchor || covered < GetClientSize().y * 2 + 600);
         ++i) {
        const int height = row_layout(i).height;
        changed |= heights_[i] != height;
        heights_[i] = height;
        if (i >= anchor)
            covered += height;
    }
    if (changed) {
        rebuild_positions();
        const double adjustment = positions_[anchor] + within - offset_;
        offset_ += adjustment;
        target_ += adjustment;
    }
}
double ScriptureView::content_height() const {
    // The end of a book keeps blank space below it, like the heading space above its first chapter.
    return positions_.empty() ? 0 : positions_.back() + FromDIP(140);
}
double ScriptureView::max_offset() const {
    return std::max(0.0, content_height() - GetClientSize().y);
}
void ScriptureView::scrollbar() {
    const int page = std::max(1, GetClientSize().y);
    const int total = std::max(page, int(std::ceil(content_height())));
    SetScrollbar(wxVERTICAL, int(std::lround(offset_)), page, total, true);
}
void ScriptureView::set_position(double value) {
    prepare_visible();
    const bool to_end = value >= max_offset();
    offset_ = std::clamp(value, 0.0, max_offset());
    prepare_visible();
    // Measured rows can be taller than their estimates; a position past the end stays at the end.
    offset_ = to_end ? max_offset() : std::clamp(offset_, 0.0, max_offset());
    scrollbar();
    Refresh(false);
}
void ScriptureView::scroll_by(double pixels) {
    release_follow();
    set_position(offset_ + pixels);
    target_ = offset_;
}
} // namespace ortho
