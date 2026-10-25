#include "core/reading_position.hpp"
#include "speech/portable_speech.hpp"
#include "speech/reading_speech.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <wx/app.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/weakref.h>
namespace ortho {
void MainFrame::save_reading_position() {
    if (!speech_)
        return;
    try {
        const auto playback = speech_->playback();
        if (speech_active(playback.state) && playback.cue &&
            playback.cue->reading < playback_.readings().size()) {
            const auto& reading = playback_.readings()[playback.cue->reading];
            const auto position =
                reading_position(corpus_, in_swedish(reading), playback.cue->section, playback.cue->verse);
            if (position) {
                user_.position(reading_key(corpus_, in_swedish(reading)), *position);
                if (speech_tracked_)
                    user_.position(speech_tracked_->key, *position);
            }
        } else if (navigation_.reader() && visible_reading_) {
            if (position_reset_ && std::abs(scripture_->scroll_position() - reset_scroll_) < 1)
                return;
            position_reset_ = false;
            const auto position =
                reading_position(corpus_, in_swedish(*visible_reading_), part_, scripture_->current_verse());
            if (position) {
                user_.position(reading_key(corpus_, in_swedish(*visible_reading_)), *position);
                if (tracked_)
                    user_.position(tracked_->key, *position);
            }
        }
    } catch (const std::exception& error) {
        wxLogWarning("Läspositionen kunde inte sparas: %s", ui::utf8(error.what()));
    }
}
bool MainFrame::has_reading_position() const {
    return visible_reading_ &&
           user_.position(reading_key(corpus_, in_swedish(*visible_reading_))).has_value();
}
std::string MainFrame::reading_progress(const Reading& reading, const std::string& key) const {
    auto position = user_.position(key);
    if (!position)
        position = user_.position(reading_key(corpus_, in_swedish(reading)));
    return position ? " · " + std::to_string(position->percent) + " %" : "";
}
void MainFrame::restore_reading_position() {
    if (!visible_reading_)
        return;
    auto position = tracked_ ? user_.position(tracked_->key) : std::optional<ReadingPosition>{};
    if (!position)
        position = user_.position(reading_key(corpus_, in_swedish(*visible_reading_)));
    if (!position)
        return;
    const auto valid =
        reading_position(corpus_, in_swedish(*visible_reading_), position->section, position->verse);
    if (!valid || valid->book != position->book || valid->source != position->source)
        return;
    part_ = position->section;
    scripture_->open_section(part_);
    scripture_->show_verse(position->verse);
    position_reset_ = true;
    reset_scroll_ = scripture_->scroll_position();
    update_bar();
}
bool MainFrame::active_playback() const {
    return playback_.active();
}
void MainFrame::play_or_pause() {
    const auto action = controls().playback_action;
    if (action == PlaybackAction::None)
        return;
    if (action == PlaybackAction::Pause || action == PlaybackAction::Resume)
        toggle_pause();
    else if (action == PlaybackAction::StartSelection) {
        save_navigation();
        save_reading_position();
        const auto marked = scripture_->selections();
        Reading reading{corpus_.new_testament_book(marked.front().book) ? ReadingKind::Gospel
                                                                        : ReadingKind::OldTestament,
                        marked.front(),
                        {marked.begin() + 1, marked.end()},
                        "sv"};
        reading.reference = scripture_->frame();
        tracked_.reset();
        play_speech({reading});
        record_navigation();
    } else {
        play_speech({visible_reading_.value_or(scripture_->reading())});
        // Only listening to the whole start-page item marks it complete.
        speech_tracked_ = tracked_;
    }
}
void MainFrame::listen_to_selection(const Reading& reading) {
    if (reading.base_language != "sv")
        return;
    save_navigation();
    save_reading_position();
    tracked_.reset();
    // A direct selection action always starts at the beginning of the selected passage.
    play_speech({reading}, false);
    record_navigation();
}
void MainFrame::speech_status(const std::string& status) {
    if (playback_.playback().state == SpeechState::Error && speech_->playback().state == SpeechState::Stopped)
        return;
    // A playback poll may already have ended the preview before its error status arrives.
    const bool preview_error =
        (voice_preview_ || voice_preview_failed_) && speech_->playback().state == SpeechState::Error;
    voice_preview_failed_ = false;
    playback_.report(status);
    refresh_speech();
    if (preview_error)
        wxMessageBox(ui::utf8(status), ui::utf8("Prova rösten"), wxOK | wxICON_INFORMATION, this);
}
void MainFrame::toggle_pause() {
    const auto state = speech_->playback();
    if (state.state == SpeechState::Paused && state.cue && !playback_.readings().empty()) {
        const auto& retained = playback_.readings();
        const auto first = std::min(state.cue->reading, retained.size() - 1);
        const std::vector<Reading> readings(retained.begin() + first, retained.end());
        const auto item = speech_tracked_;
        play_speech(readings);
        speech_tracked_ = item;
    } else {
        playback_.toggle_pause(*speech_);
        save_reading_position();
    }
    refresh_speech();
    if (playback_.polling())
        playback_timer_.Start(30);
}
void MainFrame::pause_for_navigation() {
    if (voice_preview_) {
        stop_speech(false);
        return;
    }
    if (speech_) {
        playback_.leave_reader(*speech_);
        save_reading_position();
    }
}
void MainFrame::stop_speech(bool reset) {
    playback_.stop(*speech_);
    if (reset && visible_reading_ && navigation_.reader()) {
        user_.reset_position(reading_key(corpus_, in_swedish(*visible_reading_)));
        if (tracked_)
            user_.reset_position(tracked_->key);
        part_ = 0;
        scripture_->open_section(0);
        scripture_->center_passage();
        position_reset_ = true;
        reset_scroll_ = scripture_->scroll_position();
    }
    scripture_->follow_playback(false);
    refresh_speech();
}
void MainFrame::open_speech_location(size_t reading, size_t section) {
    const auto& readings = playback_.readings();
    if (reading >= readings.size())
        return;
    if (playback_.needs_location({reading, section}, navigation_.reader())) {
        show_reading(readings[reading], true);
        if (section) {
            scripture_->open_section(section);
            part_ = section;
        }
        playback_.location(std::pair{reading, section});
        scripture_->follow_playback();
    }
}
void MainFrame::follow_speech() {
    const auto playback = speech_->playback();
    const size_t reading = playback.cue ? playback.cue->reading : 0,
                 section = playback.cue ? playback.cue->section : 0;
    if (reading >= playback_.readings().size())
        return;
    open_speech_location(reading, section);
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
    if (!voice_preview_ && playback.state == SpeechState::Completed &&
        playback_.playback().state != SpeechState::Completed) {
        for (const auto& reading : playback_.readings())
            user_.reset_position(reading_key(corpus_, in_swedish(reading)));
        position_reset_ = true;
        reset_scroll_ = scripture_->scroll_position();
    }
    if (!voice_preview_ && playback.state == SpeechState::Completed &&
        playback_.playback().state != SpeechState::Completed && speech_tracked_)
        CallAfter([this, item = *speech_tracked_] {
            complete_reading(item);
        });
    if (playback.state != SpeechState::Completed && !speech_active(playback.state))
        speech_tracked_.reset();
    playback_.update(playback);
    const bool active = speech_active(playback.state);
    if (voice_preview_ && !active) {
        voice_preview_ = false;
        voice_preview_failed_ = playback.state == SpeechState::Error;
        if (voice_preview_button_) {
            voice_preview_button_->SetLabel(ui::utf8("Prova rösten"));
            voice_preview_button_->SetName(voice_preview_button_->GetLabel());
            voice_preview_button_->Refresh();
        }
    }
    if (!navigation_.auxiliary() && active && scripture_->follows_playback() && playback.cue)
        open_speech_location(playback.cue->reading, playback.cue->section);
    if (!active) {
        scripture_->follow_playback(false);
    }
    if (!playback_.polling())
        playback_timer_.Stop();
    if (!navigation_.auxiliary())
        scripture_->playback(playback);
    update_bar();
}
void MainFrame::play_speech(const std::vector<Reading>& readings, bool resume) {
    stop_speech(false);
    playback_.forget_location();
    try {
        std::vector<Reading> primary_readings;
        primary_readings.reserve(readings.size());
        for (const auto& reading : readings)
            primary_readings.push_back(in_swedish(reading));
        auto queue = reading_speech(
            corpus_, primary_readings,
            [this](const std::string& language) {
                return speech_lexicon(language);
            },
            [this](const Reading& item, const std::string& book, int chapter) {
                return scripture_->primary_source(item, book, chapter);
            });
        double skipped = 0;
        if (resume && !primary_readings.empty())
            if (const auto position = user_.position(reading_key(corpus_, primary_readings.front())))
                skipped = resume_speech(queue, *position, &corpus_);
        playback_.start(readings, skipped);
        if (!readings.empty()) {
            if (!visible_reading_ || reading_key(corpus_, in_swedish(*visible_reading_)) !=
                                         reading_key(corpus_, primary_readings.front()))
                show_reading(readings.front(), true);
            restore_reading_position();
            playback_.location(std::pair<size_t, size_t>{0, part_});
        }
        speech_->speak_batch(queue);
        scripture_->follow_playback();
        playback_timer_.Start(30);
        refresh_speech();
    } catch (const std::exception& error) {
        stop_speech(false);
        playback_.report(error.what());
        display_playback({SpeechState::Error, {}, 0, 0});
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Uppläsning"), wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::initialize_speech() {
    // The worker copies the shared owner, not wxWeakRef's main-thread tracking data.
    auto weak = std::make_shared<wxWeakRef<MainFrame>>(this);
    auto latest_speech = std::make_shared<std::atomic<uint64_t>>(0);
    speech_ =
        create_portable_speech(resources_ / "speech", [weak, latest_speech](const SpeechUpdate& update) {
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
