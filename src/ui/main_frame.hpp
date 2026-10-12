#pragma once

#include "speech/read_aloud.hpp"
#include <filesystem>
#include <memory>
#include <utility>
#include <wx/accel.h>
#include <wx/frame.h>
#include <wx/timer.h>

class wxBoxSizer;
class wxButton;
class wxEventFilter;
class wxMenu;
class wxMenuBar;
class wxPanel;
class wxScrolledWindow;
class wxStaticText;

namespace ortho {
class UserDb;
class PronunciationReviewDb;
class StudyDb;
class ScriptureView;
class StudyPanel;
class SymbolButton;
class AddressBar;
class MonthCalendar;
class MainFrame final : public wxFrame {
public:
    MainFrame(const CorpusDb&, UserDb&, CivilDate date, const std::filesystem::path& resources,
              const std::filesystem::path& data, bool diagnostic = false);
    ~MainFrame() override;
    void open_reading(const Reading&);
    void open_psalm();
    void review_pronunciation();
    bool smoke_test(const wxString& screenshot_path);
    bool render_benchmark();

private:
    void make_menus();
    // The menu bar's menus, built for the current settings. The ⋮ menu shows them too.
    std::vector<std::pair<wxMenu*, wxString>> menus() const;
    void sync_menu(wxMenu&) const;
    void show_menu();
    // Windows and Linux hide the menu bar; Alt shows it. macOS always shows its own.
    void show_menu_bar(bool);
    void refresh_day();
    void layout_home();
    void show_readings();
    void select_day(CivilDate);
    void navigate(int days);
    void pick_date();
    void browse_bible();
    void about();
    void back();
    void show_page(wxWindow*, const wxString& title);
    void close_page();
    void close_pages();
    void apply_settings(bool persist = true);
    void update_study();
    // The reading as the Swedish left column shows it.
    Reading in_swedish(Reading) const;

    // Something the start page opens, which the reader can mark as read.
    struct Tracked {
        std::string key;
        wxString title;
    };
    void add_plans();
    void open_tracked(const Reading&, Tracked);
    void offer_completion(const Tracked&, bool listened);
    std::string day_key(const Reading&) const;
    std::vector<Pronunciation> speech_lexicon(const std::string&) const;

    void initialize_speech();
    void create_toolbar();
    void paint_toolbar(wxPaintEvent&);
    void address_clicked();
    void select_part(size_t);
    void toggle_pane(const std::string&);
    void play_speech(const std::vector<Reading>&);
    void prepare_day();
    bool preparing_day() const;
    void clear_preparation();
    void play_or_pause();
    void toggle_pause();
    void stop_speech();
    bool active_playback() const;
    void speech_status(const std::string&);
    void refresh_speech();
    void display_playback(const SpeechPlayback&);
    void open_speech_location(size_t reading, size_t section);
    void follow_speech();
    void update_bar();

    const CorpusDb& corpus_;
    UserDb& user_;
    const std::filesystem::path resources_;
    const std::filesystem::path data_;
    bool diagnostic_;
    std::string pronunciation_error_;
    Lectionary lectionary_;
    SelectedDay selected_;
    Settings settings_;
    DayReadings day_;
    std::optional<Reading> visible_reading_;
    // The open start-page item, and the one being read aloud from start to end.
    std::optional<Tracked> tracked_, speech_tracked_;
    wxString reading_title_;

    std::unique_ptr<PronunciationReviewDb> pronunciation_review_;
    std::unique_ptr<StudyDb> study_db_;
    std::unique_ptr<SpeechEngine> speech_;
    wxTimer playback_timer_;
    ReadAloud read_aloud_;
    std::optional<std::pair<size_t, size_t>> speech_view_;
    std::optional<CivilDate> preparation_day_;

    // wxWidgets owns child windows. These pointers are non-owning handles.
    wxPanel* root_ = nullptr;
    wxBoxSizer* reader_sizer_ = nullptr;
    wxWindow* page_ = nullptr;
    wxString page_title_;
    bool return_to_reader_ = false;
    std::vector<std::pair<wxWindow*, wxString>> page_history_;
    std::vector<SymbolButton*> date_buttons_;
    // The start page's month, folded out under the date while a day is chosen.
    MonthCalendar* month_ = nullptr;
    bool month_open_ = false;
    wxScrolledWindow* readings_ = nullptr;
    wxPanel* home_content_ = nullptr;
    // Start-page labels wrap to the column, less the width of what stands beside them.
    struct HomeLabel {
        wxStaticText* label;
        wxString text;
        int beside = 0;
    };
    std::vector<HomeLabel> home_labels_;
    int home_width_ = 0;
    ScriptureView* scripture_ = nullptr;
    StudyPanel* study_ = nullptr;
    wxBoxSizer* entries_ = nullptr;
    wxPanel* bar_ = nullptr;
    SymbolButton* back_ = nullptr;
    SymbolButton* play_ = nullptr;
    SymbolButton* pause_ = nullptr;
    SymbolButton* stop_ = nullptr;
    AddressBar* address_ = nullptr;
    SymbolButton* menu_ = nullptr;
    // The menu bar, also while it is hidden and so not owned by the frame.
    wxMenuBar* menu_bar_ = nullptr;
    wxAcceleratorTable accelerators_;
    std::unique_ptr<wxEventFilter> alt_tap_;
    // Right-column choices: "study" or a language code.
    std::vector<std::pair<std::string, SymbolButton*>> panes_;
    std::vector<wxButton*> plan_tiles_;
    std::vector<wxString> parts_;
    size_t part_ = 0;
};
} // namespace ortho
