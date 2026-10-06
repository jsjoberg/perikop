#pragma once
#include "ui/scripture_view.hpp"
#include "speech/speech.hpp"
#include "speech/portable_speech.hpp"
#include <wx/frame.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <chrono>
namespace ortho {
class MainFrame final : public wxFrame {
public:
    MainFrame(const CorpusDb&,UserDb&,CivilDate date,const std::filesystem::path& resources);
    ~MainFrame() override;
    void open_reading(const Reading&);
    void open_psalm();
    bool smoke_test(const wxString& screenshot_path);
private:
    void refresh_day();
    void show_readings();
    void pick_date();
    void apply_settings(bool persist=true);
    void play_speech(const std::vector<Reading>&);
    void speech_status(const std::string&);
    void refresh_speech();
    void display_playback(const SpeechPlayback&);
    void follow_speech();
    void toggle_pause();
    void play_or_pause();
    bool active_playback() const;
    void update_bar();
    void make_menus();
    void stop_speech();
    void paint_playback(wxPaintEvent&);
    std::unique_ptr<SpeechEngine> speech_;
    wxTimer playback_timer_;
    SpeechPlayback playback_ui_;
    SpeechState feedback_state_=SpeechState::Idle;
    std::chrono::steady_clock::time_point buffering_since_;
    bool debounce_buffering_=false;
    std::string speech_message_;
    std::vector<Reading> speech_readings_;
    std::optional<Reading> visible_reading_;
    std::optional<std::pair<size_t,size_t>> speech_view_;
    bool following_audio_=false;
    wxPanel* bar_;
    wxButton *back_,*previous_,*calendar_button_,*next_,*follow_,*play_,*stop_,*bible_;
    bool paused_=false;
    void navigate(int days);
    void browse_bible();
    const CorpusDb& corpus_;
    UserDb& user_;
    AntiochianLectionary lectionary_;
    SelectedDay selected_;
    Settings settings_;
    DayReadings day_;
    wxPanel* root_;
    wxScrolledWindow* readings_;
    ScriptureView* scripture_;
    wxString reading_title_;
    wxStaticText* speech_status_;
    wxChoice* part_;
    wxBoxSizer* entries_;
};
}
