#pragma once
#include "ui/scripture_view.hpp"
#include "speech/speech.hpp"
#include <wx/frame.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
namespace ortho {
class MainFrame final : public wxFrame {
public:
    MainFrame(const CorpusDb&,UserDb&,CivilDate date);
    void open_reading(const Reading&);
    void open_psalm();
    bool smoke_test(const wxString& screenshot_path);
private:
    void refresh_day();
    void show_readings();
    void pick_date();
    void apply_settings(bool persist=true);
    void preview_speech(const std::vector<Reading>&);
    void navigate(int days);
    const CorpusDb& corpus_;
    UserDb& user_;
    FixtureLectionary lectionary_;
    SelectedDay selected_;
    Settings settings_;
    DayReadings day_;
    wxPanel *root_,*reader_header_,*footer_;
    wxScrolledWindow* readings_;
    ScriptureView* scripture_;
    wxButton *date_,*listen_all_;
    wxStaticText *annotation_,*reader_label_,*speech_status_;
    wxChoice *calendar_,*theme_,*parallel_;
    wxBoxSizer* entries_;
};
}
