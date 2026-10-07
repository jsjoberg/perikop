#include "core/reading_display.hpp"
#include "speech/portable_speech.hpp"
#include "speech/reading_speech.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <atomic>
#include <stdexcept>
#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/weakref.h>
namespace ortho {
bool MainFrame::active_playback() const {
    return speech_active(playback_ui_.state);
}
void MainFrame::play_or_pause() {
    if (active_playback())
        toggle_pause();
    else if (settings_.primary != "sv")
        return;
    else if (const auto marked = scripture_->IsShown() ? scripture_->selection() : std::nullopt) {
        Reading reading{new_testament_book(marked->book) ? ReadingKind::Gospel : ReadingKind::OldTestament,
                        *marked,
                        passage_label(corpus_, {*marked}),
                        {},
                        settings_.primary};
        reading.reference = scripture_->frame();
        play_speech({reading});
    } else if (scripture_->IsShown())
        play_speech({visible_reading_.value_or(scripture_->reading())});
}
void MainFrame::speech_status(const std::string& status) {
    if (playback_ui_.state == SpeechState::Error && speech_->playback().state == SpeechState::Stopped)
        return;
    speech_message_ = status;
    refresh_speech();
}
void MainFrame::toggle_pause() {
    if (speech_->playback().state == SpeechState::Paused)
        speech_->resume();
    else
        speech_->pause();
    refresh_speech();
}
void MainFrame::stop_speech() {
    speech_->stop();
    following_audio_ = false;
    scripture_->follow_playback(false);
    refresh_speech();
}
void MainFrame::follow_speech() {
    if (speech_readings_.empty())
        return;
    const auto playback = speech_->playback();
    const size_t reading = playback.cue ? playback.cue->reading : 0,
                 section = playback.cue ? playback.cue->section : 0;
    if (reading >= speech_readings_.size())
        return;
    if (!speech_view_ || *speech_view_ != std::pair{reading, section} || !scripture_->IsShown()) {
        open_reading(speech_readings_[reading]);
        if (section) {
            scripture_->open_section(section);
            part_ = section;
        }
        speech_view_ = std::pair{reading, section};
    }
    following_audio_ = true;
    scripture_->playback(playback);
    scripture_->follow_playback();
    scripture_->SetFocus();
    refresh_speech();
}
void MainFrame::refresh_speech() {
    if (speech_)
        display_playback(speech_->playback());
}
void MainFrame::display_playback(const SpeechPlayback& playback) {
    const auto now = std::chrono::steady_clock::now();
    if (playback.state == SpeechState::Buffering && playback_ui_.state != SpeechState::Buffering) {
        buffering_since_ = now;
        debounce_buffering_ = playback_ui_.state == SpeechState::Playing && playback.cue.has_value();
    } else if (playback.state != SpeechState::Buffering)
        debounce_buffering_ = false;
    playback_ui_ = playback;
    const auto state = playback.state;
    // Freeze tracking immediately, but do not flash buffering text for a
    // brief gap between audio callbacks. Startup feedback remains immediate.
    feedback_state_ = state == SpeechState::Buffering && debounce_buffering_ &&
                              now - buffering_since_ < std::chrono::milliseconds(180)
                          ? SpeechState::Playing
                          : state;
    const bool active = speech_active(state);
    if (active && following_audio_ && playback.cue && playback.cue->reading < speech_readings_.size()) {
        const auto view = std::pair{playback.cue->reading, playback.cue->section};
        if (!speech_view_ || *speech_view_ != view) {
            open_reading(speech_readings_[view.first]);
            if (view.second) {
                scripture_->open_section(view.second);
                part_ = view.second;
            }
            speech_view_ = view;
            following_audio_ = true;
            scripture_->follow_playback();
        }
    }
    if (!active) {
        following_audio_ = false;
        scripture_->follow_playback(false);
        playback_timer_.Stop();
    }
    scripture_->playback(playback);
    update_bar();
}
void MainFrame::play_speech(const std::vector<Reading>& readings) {
    stop_speech();
    speech_readings_ = readings;
    speech_view_.reset();
    try {
        std::vector<Reading> primary_readings;
        primary_readings.reserve(readings.size());
        for (const auto& reading : readings)
            primary_readings.push_back(in_primary(reading));
        const auto queue = reading_speech(corpus_, primary_readings, [this](const std::string& language) {
            return speech_lexicon(language);
        });
        speech_introductions_.assign(readings.size(), {});
        for (const auto& utterance : queue)
            if (utterance.cue && utterance.cue->introduction)
                speech_introductions_[utterance.cue->reading] = ui::utf8(utterance.display_text);
        if (!readings.empty()) {
            open_reading(readings.front());
            speech_view_ = std::pair<size_t, size_t>{0, 0};
        }
        speech_->speak_batch(queue);
        following_audio_ = true;
        scripture_->follow_playback();
        playback_timer_.Start(30);
        refresh_speech();
    } catch (const std::exception& error) {
        stop_speech();
        speech_message_ = error.what();
        display_playback({SpeechState::Error, {}, 0, 0});
        wxMessageBox(ui::utf8(error.what()), "Uppläsning", wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::initialize_speech() {
    // The worker copies the shared owner, not wxWeakRef's main-thread tracking data.
    auto weak = std::make_shared<wxWeakRef<MainFrame>>(this);
    auto latest_speech = std::make_shared<std::atomic<uint64_t>>(0);
    const auto data_path = ui::filesystem_path(wxStandardPaths::Get().GetUserLocalDataDir());
    pronunciation_review_ = std::make_unique<PronunciationReviewDb>(data_path / "pronunciation-review.db");
    speech_ = create_portable_speech(data_path, [weak, latest_speech](const SpeechUpdate& update) {
        auto known = latest_speech->load();
        while (known < update.sequence && !latest_speech->compare_exchange_weak(known, update.sequence)) {
        }
        if (update.sequence < known)
            return;
        auto deliver = [weak, latest_speech, update] {
            if (*weak && latest_speech->load() == update.sequence)
                (*weak)->speech_status(update.text);
        };
        // Delivery must run after the engine releases its mutex, including
        // Pause and Stop callbacks made on the main thread.
        wxTheApp->CallAfter(deliver);
    });
    speech_->set_speed(settings_.speech_rate / 100.0);
    speech_->set_voice(settings_.speech_voice);
}
} // namespace ortho
