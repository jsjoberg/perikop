#pragma once

#include "speech/speech.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include <chrono>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>

namespace ortho {
class MainFrame final : public wxFrame {
public:
    MainFrame(const CorpusDb&, UserDb&, CivilDate date, const std::filesystem::path& resources);
    ~MainFrame() override;
    void open_reading(const Reading&);
    void open_psalm();
    void review_pronunciation();
    bool smoke_test(const wxString& screenshot_path);

private:
    void make_menus();
    void refresh_day();
    void show_readings();
    void navigate(int days);
    void pick_date();
    void browse_bible();
    void apply_settings(bool persist = true);
    void update_study();
    Reading in_primary(Reading) const;
    std::vector<Pronunciation> speech_lexicon(const std::string&) const;

    void initialize_speech();
    void create_playback_bar();
    void play_speech(const std::vector<Reading>&);
    void play_or_pause();
    void toggle_pause();
    void stop_speech();
    bool active_playback() const;
    void speech_status(const std::string&);
    void refresh_speech();
    void display_playback(const SpeechPlayback&);
    void follow_speech();
    void update_bar();
    void paint_playback(wxPaintEvent&);

    const CorpusDb& corpus_;
    UserDb& user_;
    const std::filesystem::path resources_;
    AntiochianLectionary lectionary_;
    SelectedDay selected_;
    Settings settings_;
    DayReadings day_;
    std::optional<Reading> visible_reading_;
    wxString reading_title_;

    std::unique_ptr<PronunciationReviewDb> pronunciation_review_;
    std::unique_ptr<StudyDb> study_db_;
    std::unique_ptr<SpeechEngine> speech_;
    wxTimer playback_timer_;
    SpeechPlayback playback_ui_;
    SpeechState feedback_state_ = SpeechState::Idle;
    std::chrono::steady_clock::time_point buffering_since_;
    bool debounce_buffering_ = false;
    std::string speech_message_;
    std::vector<Reading> speech_readings_;
    std::optional<std::pair<size_t, size_t>> speech_view_;
    bool following_audio_ = false;

    // wxWidgets owns child windows. These pointers are non-owning handles.
    wxPanel* root_ = nullptr;
    wxScrolledWindow* readings_ = nullptr;
    ScriptureView* scripture_ = nullptr;
    StudyPanel* study_ = nullptr;
    wxBoxSizer* entries_ = nullptr;
    wxPanel* bar_ = nullptr;
    wxButton* back_ = nullptr;
    wxButton* follow_ = nullptr;
    wxButton* play_ = nullptr;
    wxButton* stop_ = nullptr;
    wxStaticText* speech_status_ = nullptr;
    wxChoice* part_ = nullptr;
    int play_item_ = 0;
    int stop_item_ = 0;
};
} // namespace ortho
