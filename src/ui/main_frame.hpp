#pragma once

#include "core/navigation.hpp"
#include "speech/playback_controller.hpp"
#include "ui/reader_controls.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <utility>
#include <wx/accel.h>
#include <wx/frame.h>
#include <wx/timer.h>

class wxBoxSizer;
class wxEventFilter;
class wxMenuBar;
class wxPanel;
class wxScrolledWindow;
class wxStaticText;

namespace ortho {
class UserDb;
class PronunciationReviewDb;
class ScriptureView;
class StudyPanel;
class SymbolButton;
class AddressBar;
class MonthCalendar;
class ReadingRow;
struct ReadingPlan;
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
    // Brings the menu bar's labels and states up to date. The ⋮ menu shows its items.
    void sync_menu();
    void show_menu();
    // Windows and Linux hide the menu bar; Alt shows it. macOS always shows its own.
    void show_menu_bar(bool);
    void refresh_day();
    void layout_home();
    void show_readings();
    // Display a reading without replacing the playback session that requested it.
    void show_reading(const Reading&);
    void select_day(CivilDate);
    void navigate(int days);
    void pick_date();
    void browse_bible();
    void about();
    void back();
    void show_page(wxWindow*, Screen, const wxString& title);
    wxWindow* current_page() const;
    wxString current_page_title() const;
    void present_navigation();
    ReaderControls controls() const;
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
    // The reading plans have their own page, opened from the start page and the Bibel menu.
    void show_plans();
    void fill_plans(wxScrolledWindow*);
    // Theme, calendar, reading order and voice. Text size and speed are in the ⋮ menu.
    void show_settings();
    void fill_settings(wxScrolledWindow*);
    // The start page's plan section: the next part of the plan in progress, and all plans.
    void add_next_plan();
    void open_plan_part(const ReadingPlan&, std::size_t part);
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
    void pause_for_navigation();
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
    // Whether the visible reading has Swedish text to read aloud.
    bool speakable_ = true;
    // The open start-page item, and the one being read aloud from start to end.
    std::optional<Tracked> tracked_, speech_tracked_;
    wxString reading_title_;

    std::unique_ptr<PronunciationReviewDb> pronunciation_review_;
    std::unique_ptr<SpeechEngine> speech_;
    wxTimer playback_timer_;
    PlaybackController playback_;

    // wxWidgets owns child windows. These pointers are non-owning handles.
    wxPanel* root_ = nullptr;
    wxBoxSizer* reader_sizer_ = nullptr;
    NavigationState navigation_;
    struct PageWindow {
        wxWindow* window;
        wxString title;
        Screen screen;
    };
    std::map<NavigationState::ViewId, PageWindow> pages_;
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
    std::vector<ReadingRow*> home_rows_;
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
    std::vector<wxString> parts_;
    size_t part_ = 0;
};
} // namespace ortho
