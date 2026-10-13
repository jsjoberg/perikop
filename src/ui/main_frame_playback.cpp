#include "speech/portable_speech.hpp"
#include "speech/reading_speech.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/weakref.h>
namespace ortho {
bool MainFrame::active_playback() const {
    return speech_active(read_aloud_.playback().state);
}
bool MainFrame::preparing_day() const {
    return preparation_day_ && speech_ && speech_->preparation().state == PreparationState::Running;
}
void MainFrame::clear_preparation() {
    if (preparing_day())
        speech_->stop();
    preparation_day_.reset();
}
void MainFrame::prepare_day() {
    if (page_ || active_playback() || preparing_day())
        return;
    stop_speech();
    try {
        const auto day =
            lectionary_.readings_with_variants(selected_.date(), settings_.calendar, settings_.tradition);
        std::vector<Reading> readings;
        const auto add = [&](const Reading& reading) {
            if (reading.can_open())
                readings.push_back(in_swedish(reading));
        };
        for (const auto& reading : day.readings)
            add(reading);
        for (const auto& variant : day.variants)
            if (variant.reading)
                add(*variant.reading);
        const auto queue = reading_speech(corpus_, readings, [this](const std::string& language) {
            return speech_lexicon(language);
        });
        speech_->prepare_batch(queue);
        preparation_day_ = selected_.date();
        refresh_speech();
    } catch (const std::exception& error) {
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Förbered dagens ljud"), wxOK | wxICON_INFORMATION,
                     this);
    }
}
void MainFrame::play_or_pause() {
    if (page_)
        return;
    if (active_playback())
        toggle_pause();
    else if (scripture_->IsShown() && !scripture_->selections().empty()) {
        const auto marked = scripture_->selections();
        Reading reading{corpus_.new_testament_book(marked.front().book) ? ReadingKind::Gospel
                                                                        : ReadingKind::OldTestament,
                        marked.front(),
                        {marked.begin() + 1, marked.end()},
                        "sv"};
        reading.reference = scripture_->frame();
        play_speech({reading});
    } else if (scripture_->IsShown()) {
        play_speech({visible_reading_.value_or(scripture_->reading())});
        // Listening to the whole of a start-page item offers to mark it as read.
        speech_tracked_ = tracked_;
    }
}
void MainFrame::speech_status(const std::string& status) {
    if (read_aloud_.playback().state == SpeechState::Error &&
        speech_->playback().state == SpeechState::Stopped)
        return;
    // A stop that bypassed stop_speech, such as a word played in the study panel, ended the preparation.
    if (preparation_day_ && speech_->preparation().state == PreparationState::Idle)
        preparation_day_.reset();
    if (!preparation_day_)
        read_aloud_.report(status);
    refresh_speech();
}
void MainFrame::toggle_pause() {
    if (speech_->playback().state == SpeechState::Paused) {
        speech_->resume();
        playback_timer_.Start(30);
    } else
        speech_->pause();
    refresh_speech();
}
void MainFrame::stop_speech() {
    speech_->stop();
    preparation_day_.reset();
    scripture_->follow_playback(false);
    refresh_speech();
}
void MainFrame::open_speech_location(size_t reading, size_t section) {
    const auto& readings = read_aloud_.readings();
    if (reading >= readings.size())
        return;
    if (!speech_view_ || *speech_view_ != std::pair{reading, section} || !scripture_->IsShown()) {
        open_reading(readings[reading]);
        if (section) {
            scripture_->open_section(section);
            part_ = section;
        }
        speech_view_ = std::pair{reading, section};
        scripture_->follow_playback();
    }
}
void MainFrame::follow_speech() {
    const auto playback = speech_->playback();
    const size_t reading = playback.cue ? playback.cue->reading : 0,
                 section = playback.cue ? playback.cue->section : 0;
    if (reading >= read_aloud_.readings().size())
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
    if (playback.state == SpeechState::Completed && read_aloud_.playback().state != SpeechState::Completed &&
        speech_tracked_)
        CallAfter([this, item = *speech_tracked_] {
            offer_completion(item, true);
        });
    if (playback.state != SpeechState::Completed && !speech_active(playback.state))
        speech_tracked_.reset();
    read_aloud_.update(playback);
    const bool active = speech_active(playback.state);
    if (!page_ && active && scripture_->follows_playback() && playback.cue)
        open_speech_location(playback.cue->reading, playback.cue->section);
    if (!active) {
        scripture_->follow_playback(false);
        playback_timer_.Stop();
    } else if (playback.state == SpeechState::Paused)
        playback_timer_.Stop();
    if (!page_)
        scripture_->playback(playback);
    update_bar();
}
void MainFrame::play_speech(const std::vector<Reading>& readings) {
    stop_speech();
    speech_view_.reset();
    try {
        std::vector<Reading> primary_readings;
        primary_readings.reserve(readings.size());
        for (const auto& reading : readings)
            primary_readings.push_back(in_swedish(reading));
        const auto queue = reading_speech(corpus_, primary_readings, [this](const std::string& language) {
            return speech_lexicon(language);
        });
        read_aloud_.start(readings);
        if (!readings.empty()) {
            open_reading(readings.front());
            speech_view_ = std::pair<size_t, size_t>{0, 0};
        }
        speech_->speak_batch(queue);
        scripture_->follow_playback();
        playback_timer_.Start(30);
        refresh_speech();
    } catch (const std::exception& error) {
        stop_speech();
        read_aloud_.report(error.what());
        display_playback({SpeechState::Error, {}, 0, 0});
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Uppläsning"), wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::initialize_speech() {
    // The worker copies the shared owner, not wxWeakRef's main-thread tracking data.
    auto weak = std::make_shared<wxWeakRef<MainFrame>>(this);
    auto latest_speech = std::make_shared<std::atomic<uint64_t>>(0);
    try {
        auto review = std::make_unique<PronunciationReviewDb>(data_ / "pronunciation-review.db");
        review->overrides(); // Check the table before enabling personal corrections.
        pronunciation_review_ = std::move(review);
    } catch (const std::exception& e) {
        const auto path_bytes = (data_ / "pronunciation-review.db").u8string();
        const std::string path(reinterpret_cast<const char*>(path_bytes.data()), path_bytes.size());
        pronunciation_error_ = "Uttalskorrigeringar kunde inte läsas. Läsning och uppläsning fungerar med "
                               "grunduttalen. Databasen har bevarats. Säkerhetskopiera filen innan du "
                               "försöker återställa den: " +
                               path + "\n\n" + e.what();
        std::cerr << pronunciation_error_ << '\n';
        if (!diagnostic_)
            CallAfter([this] {
                wxMessageBox(ui::utf8(pronunciation_error_), ui::utf8("Uttalskorrigeringar"),
                             wxOK | wxICON_INFORMATION, this);
            });
    }
    speech_ = create_portable_speech(
        resources_ / "voices", data_, [weak, latest_speech](const SpeechUpdate& update) {
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
