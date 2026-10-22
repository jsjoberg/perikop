#pragma once

#include "core/navigation.hpp"
#include "core/updater.hpp"
#include "speech/playback_controller.hpp"
#include "ui/reader_controls.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <utility>
#include <wx/accel.h>
#include <wx/frame.h>
#include <wx/timer.h>
#include <wx/weakref.h>

class wxBoxSizer;
class wxEventFilter;
class wxMenuBar;
class wxDialog;
class wxMenu;
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
    MainFrame(CorpusDb&, UserDb&, CivilDate date, const std::filesystem::path& resources,
              const std::filesystem::path& data, std::unique_ptr<UpdateBackend> updates,
              bool diagnostic = false);
    ~MainFrame() override;
    void open_reading(const Reading&);
    void open_psalm();
    void review_pronunciation();
    bool smoke_test(const wxString& screenshot_path);
    bool render_benchmark();

private:
#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;
#endif
    void make_menus();
    // Brings the menu bar's labels and states up to date. The ⋮ menu shows its items.
    void sync_menu();
    void show_menu();
    bool smoke_menu();
    wxMenu* bookmark_menu();
    void refresh_bookmarks();
    void add_bookmarks(const Reading&);
    void open_bookmark(const Bookmark&);
    void remove_bookmark(const Bookmark&);
    void listen_to_selection(const Reading&);
    void refresh_day();
    void layout_home();
    void show_readings(bool record = true);
    // Display a reading without replacing the playback session that requested it.
    void show_reading(const Reading&);
    void select_day(CivilDate);
    void navigate(int days);
    void pick_date();
    void browse_bible();
    void about();
    void back();
    void forward();
    void save_navigation();
    void record_navigation();
    void restore_navigation();
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
    void layout_plans(wxScrolledWindow*);
    // Theme, calendar, reading order and voice. Text size and speed are in the ⋮ menu.
    void show_settings();
    void fill_settings(wxScrolledWindow*);
    void preview_voice();
    void add_content_loader(wxWindow*, wxBoxSizer*);
    void import_content();
    void show_updates();
    // The update dialog follows the updater's state.
    void show_update_status();
    void update_clicked();
    // The start page's plan section: the next part of the plan in progress, and all plans.
    void add_next_plan();
    void open_plan_part(const ReadingPlan&, std::size_t part);
    void open_tracked(const Reading&, Tracked);
    void complete_reading(const Tracked&);
    std::string day_key(const Reading&) const;
    std::vector<Pronunciation> speech_lexicon(const std::string&) const;

    void initialize_speech();
    void create_toolbar();
    void paint_toolbar(wxPaintEvent&);
    void address_clicked();
    void select_part(size_t);
    void toggle_pane(const std::string&);
    void play_speech(const std::vector<Reading>&, bool resume = true);
    void play_or_pause();
    void toggle_pause();
    void pause_for_navigation();
    void stop_speech(bool reset = true);
    void save_reading_position();
    void restore_reading_position();
    std::string reading_progress(const Reading&, const std::string& key) const;
    bool has_reading_position() const;
    bool active_playback() const;
    void speech_status(const std::string&);
    void refresh_speech();
    void display_playback(const SpeechPlayback&);
    void open_speech_location(size_t reading, size_t section);
    void follow_speech();
    void update_bar();

    CorpusDb& corpus_;
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
    // Hold a restored verse or Stop reset until the reader moves or starts again.
    bool position_reset_ = false;
    double reset_scroll_ = 0;
    wxString reading_title_;

    std::unique_ptr<PronunciationReviewDb> pronunciation_review_;
    std::unique_ptr<SpeechEngine> speech_;
    std::unique_ptr<Updater> updater_;
    wxTimer playback_timer_;
    PlaybackController playback_;
    bool voice_preview_ = false;
    // A preview that ended in an error, kept until its queued status shows the error.
    bool voice_preview_failed_ = false;
    wxWeakRef<wxWindow> voice_preview_button_;

    // wxWidgets owns child windows. These pointers are non-owning handles.
    wxPanel* root_ = nullptr;
    wxBoxSizer* reader_sizer_ = nullptr;
    NavigationState navigation_;
    NavigationHistory history_;
    struct Visit {
        NavigationState navigation;
        CivilDate date;
        std::optional<Reading> reading;
        std::optional<Tracked> tracked;
        std::size_t part = 0;
        double scroll = 0;
        wxPoint day_scroll;
        bool month_open = false;
        std::vector<Passage> selections;
    };
    std::map<NavigationHistory::Id, Visit> visits_;
    bool restoring_navigation_ = false;
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
    SymbolButton* forward_ = nullptr;
    SymbolButton* play_ = nullptr;
    SymbolButton* stop_ = nullptr;
    AddressBar* address_ = nullptr;
    SymbolButton* menu_ = nullptr;
    // Command definitions for the overflow menu and shortcuts; macOS also attaches this bar.
    wxMenuBar* menu_bar_ = nullptr;
    std::unique_ptr<wxEventFilter> navigation_input_;
    // Right-column choices: word study or the preferred secondary edition.
    std::vector<std::pair<std::string, SymbolButton*>> panes_;
    std::vector<wxString> parts_;
    size_t part_ = 0;
    // Update action dialog; controls go when it closes.
    wxWeakRef<wxDialog> update_dialog_;
    wxWeakRef<wxWindow> update_button_, update_cancel_, update_text_;
};
} // namespace ortho
