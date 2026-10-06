#include "core/reading_display.hpp"
#include "speech/portable_speech.hpp"
#include "speech/reading_speech.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <atomic>
#include <stdexcept>
#include <wx/app.h>
#include <wx/dcbuffer.h>
#include <wx/menu.h>
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
void MainFrame::update_bar() {
    const bool reader = scripture_->IsShown(), active = active_playback();
    const wxString follow_label = active ? "Följ uppläsningen" : "Till läsningen";
    const bool marked = reader && scripture_->selection();
    const wxString play_label = !active ? (marked ? "Läs markering" : "Lyssna")
                                : (playback_ui_.state == SpeechState::Paused) ? "Fortsätt"
                                                                              : "Pausa";
    // Only the Swedish text can be read aloud; Greek and English have no voice.
    const bool speakable = active || settings_.primary == "sv";
    if (auto* bar = GetMenuBar()) {
        bar->SetLabel(play_item_, play_label + "\tCtrl+P");
        bar->Enable(play_item_, (reader || active) && speakable);
        bar->Enable(stop_item_, active);
    }
    if (play_->IsEnabled() != speakable) {
        play_->Enable(speakable);
        play_->SetToolTip(speakable ? ui::utf8("Lyssna, pausa eller fortsätt · mellanslag")
                                    : ui::utf8("Uppläsning finns bara på svenska"));
    }
    bool changed = follow_->GetLabel() != follow_label || play_->GetLabel() != play_label;
    follow_->SetLabel(follow_label);
    play_->SetLabel(play_label);
    const std::pair<wxWindow*, bool> visibility[] = {{bar_, reader || active},
                                                     {back_, reader},
                                                     {part_, reader && part_->GetCount() > 1},
                                                     {follow_, active ? !following_audio_ : reader},
                                                     {play_, reader || active},
                                                     {stop_, active}};
    for (const auto& [control, shown] : visibility)
        if (control->IsShown() != shown) {
            control->Show(shown);
            changed = true;
        }
    if (changed) {
        bar_->Layout();
        root_->Layout();
    }
    wxString location;
    if (playback_ui_.cue) {
        const auto& cue = *playback_ui_.cue;
        if (cue.introduction)
            location = "Introduktion";
        else
            location = ui::utf8(passage_label(corpus_, {{cue.book, cue.verse, cue.verse}}));
    }
    // One short line: where the reading is, or what it is waiting for.
    const wxString ready =
        playback_ui_.ready > 0 ? wxString::Format(" %d %%", int(playback_ui_.ready * 100)) : wxString{};
    const wxString idle = scripture_->IsShown() ? reading_title_ : wxString{};
    wxString title, tooltip;
    switch (feedback_state_) {
    case SpeechState::Loading:
        title = "Laddar rösten…";
        break;
    case SpeechState::Buffering:
        title = (playback_ui_.cue ? "Förbereder fortsättningen…" : "Förbereder uppläsningen…") + ready;
        tooltip = "Uppläsningen startar när tillräckligt mycket ljud är klart för att den inte ska stanna.";
        break;
    case SpeechState::Playing:
        title = location;
        break;
    case SpeechState::Paused:
        title = location.empty() ? "Pausad" : "Pausad · " + location;
        break;
    case SpeechState::Error:
        title = "Uppläsningen kunde inte fortsätta";
        tooltip = ui::utf8(speech_message_);
        break;
    case SpeechState::Stopped:
    case SpeechState::Completed:
    case SpeechState::Idle:
        title = idle;
        break;
    }
    if (speech_status_->GetLabel() != title)
        speech_status_->SetLabel(title);
    speech_status_->SetToolTip(tooltip.empty() ? title : tooltip);
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
            part_->SetSelection(int(section));
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
                part_->SetSelection(int(view.second));
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
    bar_->Refresh(false);
}
void MainFrame::paint_playback(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(bar_);
    const auto colors = palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));
    dc.Clear();
    const auto size = bar_->GetClientSize();
    dc.SetPen(wxPen(colors.rule, 1));
    dc.DrawLine(0, 0, size.x, 0);
    // The rule doubles as the progress line while a reading plays.
    if (active_playback() && playback_ui_.progress > 0) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(colors.accent));
        dc.DrawRectangle(0, 0, int(size.x * playback_ui_.progress), FromDIP(2));
    }
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

void MainFrame::create_playback_bar() {
    // Every control lives in one bottom bar, shown in the reader and during
    // playback. Day navigation is in the Kalender menu.
    bar_ = new wxPanel(root_);
    bar_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    bar_->Bind(wxEVT_PAINT, &MainFrame::paint_playback, this);
    auto* bar = new wxBoxSizer(wxHORIZONTAL);
    const auto add = [&](wxWindow* control, int proportion = 0) {
        bar->Add(control, proportion, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    };
    back_ = ui::button(bar_, "‹ Läsningar", [this] {
        show_readings();
    });
    add(back_);
    part_ = new wxChoice(bar_, wxID_ANY);
    part_->SetName("Läsningens del");
    part_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        following_audio_ = false;
        scripture_->follow_playback(false);
        scripture_->open_section(part_->GetSelection());
        refresh_speech();
    });
    add(part_);
    speech_status_ = new wxStaticText(bar_, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                      wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    speech_status_->SetFont(ui_font(11));
    speech_status_->SetMinSize(FromDIP(wxSize(40, -1)));
    bar->Add(speech_status_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
    follow_ = ui::button(bar_, "Följ uppläsningen", [this] {
        if (active_playback())
            follow_speech();
        else {
            scripture_->center_passage();
            scripture_->SetFocus();
        }
    });
    add(follow_);
    play_ = ui::button(bar_, "Lyssna", [this] {
        play_or_pause();
    });
    play_->SetToolTip("Lyssna, pausa eller fortsätt · mellanslag");
    add(play_);
    stop_ = ui::button(bar_, "Stoppa", [this] {
        stop_speech();
    });
    stop_->SetToolTip("Avsluta uppläsningen · Escape");
    bar->Add(stop_, 0, wxALIGN_CENTER_VERTICAL);
    auto* bar_inset = new wxBoxSizer(wxVERTICAL);
    bar_inset->Add(bar, 0, wxEXPAND | wxALL, FromDIP(10));
    bar_->SetSizer(bar_inset);
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
