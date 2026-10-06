#include "ui/main_frame.hpp"
#include <wx/sizer.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/calctrl.h>
#include <wx/textctrl.h>
#include <wx/dialog.h>
#include <wx/statline.h>
#include <wx/image.h>
#include <wx/app.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
#include <wx/fontenum.h>
#include <wx/statbmp.h>
#include <wx/weakref.h>
#include <wx/graphics.h>
#include <wx/stdpaths.h>
#include <limits>
#include <iostream>
#include <algorithm>
#include <functional>
#include <atomic>
#include <cmath>
#include <chrono>
namespace ortho {
namespace {
wxString u(const std::string& s){return wxString::FromUTF8(s);}
wxButton* button(wxWindow* parent,const wxString& label,const std::function<void()>& action) {
    auto* control=new wxButton(parent,wxID_ANY,label,wxDefaultPosition,wxDefaultSize,wxBU_EXACTFIT);
    control->SetFont(ui_font());control->Bind(wxEVT_BUTTON,[action](wxCommandEvent&){action();});return control;
}
wxStaticText* label(wxWindow* parent,const wxString& text,int points=11) {
    auto* control=new wxStaticText(parent,wxID_ANY,text);control->SetFont(ui_font(points));return control;
}
void recolor(wxWindow* window,const Palette& colors) {
    window->SetBackgroundColour(colors.paper);window->SetForegroundColour(colors.ink);
    for(auto* child:window->GetChildren())recolor(child,colors);
    window->Refresh();
}
wxString kind_label(ReadingKind kind) {
    switch(kind){
    case ReadingKind::MorningPsalm:return "MORGON";
    case ReadingKind::Epistle:return "EPISTEL";
    case ReadingKind::Gospel:return "EVANGELIUM";
    case ReadingKind::OldTestament:return "GAMLA TESTAMENTET";
    case ReadingKind::Vespers:return "VESPER";
    case ReadingKind::EveningPsalm:return "KVÄLL";
    }
    return {};
}
}
MainFrame::MainFrame(const CorpusDb& corpus,UserDb& user,CivilDate date,const std::filesystem::path& resources)
    :wxFrame(nullptr,wxID_ANY,"Ortodox läsare",wxDefaultPosition,wxSize(1120,900)),
    playback_timer_(this),corpus_(corpus),user_(user),lectionary_(corpus),selected_(date),settings_(user.load()) {
    SetMinSize(FromDIP(wxSize(520,480)));root_=new wxPanel(this);root_->SetFont(ui_font());
    auto* outer=new wxBoxSizer(wxVERTICAL);
    auto* top=new wxBoxSizer(wxHORIZONTAL);
    const auto icon_path=(resources/"icons/orthodox-cross.png").string();
    wxImage image(u(icon_path),wxBITMAP_TYPE_PNG);
    if(image.IsOk()) {
        const auto small=wxBitmap(image.Copy().Rescale(36,36,wxIMAGE_QUALITY_HIGH));
        const auto large=wxBitmap(image.Copy().Rescale(72,72,wxIMAGE_QUALITY_HIGH));
        auto* cross=new wxStaticBitmap(root_,wxID_ANY,wxBitmapBundle::FromBitmaps(small,large));
        cross->SetToolTip("Ortodox läsare · kors i svenska färger");
        top->Add(cross,0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(12));
    }
    top->Add(button(root_,"‹",[this]{navigate(-1);}),0,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(6));
    date_=button(root_,u(date_swedish(selected_.date())),[this]{pick_date();});date_->SetFont(ui_font(13));
    date_->SetToolTip("Välj ett civilt datum");top->Add(date_,1,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(6));
    top->Add(button(root_,"›",[this]{navigate(1);}),0,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(6));
    calendar_=new wxChoice(root_,wxID_ANY,wxDefaultPosition,wxDefaultSize,{"Nya kalendern","Gamla kalendern"});
    calendar_->SetSelection(settings_.calendar==CalendarStyle::Old?1:0);
    calendar_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){settings_.calendar=calendar_->GetSelection()?CalendarStyle::Old:CalendarStyle::New;show_readings();refresh_day();apply_settings();});
    top->Add(calendar_,0,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(6));outer->Add(top,0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,FromDIP(24));
    annotation_=label(root_,"",9);outer->Add(annotation_,0,wxALIGN_CENTER|wxTOP|wxBOTTOM,FromDIP(8));
    outer->Add(new wxStaticLine(root_),0,wxEXPAND);
    reader_header_=new wxPanel(root_);auto* reader_tools=new wxBoxSizer(wxHORIZONTAL);
    reader_tools->Add(button(reader_header_,"‹ Läsningar",[this]{show_readings();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(20));
    reader_label_=new wxStaticText(reader_header_,wxID_ANY,"",wxDefaultPosition,wxDefaultSize,wxST_ELLIPSIZE_END);
    reader_label_->SetFont(ui_font(10));reader_label_->SetMinSize(FromDIP(wxSize(60,-1)));
    reader_label_->SetToolTip("Mässingsstrecket i marginalen markerar den föreskrivna läsningen.");
    reader_tools->Add(reader_label_,1,wxALIGN_CENTER_VERTICAL);
    reader_tools->Add(button(reader_header_,"Till läsningen",[this]{following_audio_=false;scripture_->follow_playback(false);scripture_->center_passage();scripture_->SetFocus();refresh_speech();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(10));
    reader_tools->Add(button(reader_header_,"Lyssna",[this]{play_speech({visible_reading_.value_or(scripture_->reading())});}),0,wxALIGN_CENTER_VERTICAL);
    reader_header_->SetSizer(reader_tools);outer->Add(reader_header_,0,wxEXPAND|wxALL,FromDIP(20));
    part_=new wxChoice(reader_header_,wxID_ANY);part_->SetName("Läsningens del");
    part_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){following_audio_=false;scripture_->follow_playback(false);scripture_->open_section(part_->GetSelection());refresh_speech();});
    reader_tools->Insert(2,part_,0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(10));part_->Hide();
    scripture_=new ScriptureView(root_,corpus);outer->Add(scripture_,1,wxEXPAND);
    readings_=new wxScrolledWindow(root_,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL|wxBORDER_NONE);
    readings_->SetScrollRate(0,FromDIP(12));entries_=new wxBoxSizer(wxVERTICAL);readings_->SetSizer(entries_);
    outer->Add(readings_,1,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(68));
    footer_=new wxPanel(root_);auto* footer=new wxWrapSizer(wxHORIZONTAL,wxREMOVE_LEADING_SPACES);
    footer->Add(button(footer_,"Läsningar",[this]{show_readings();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(12));
    footer->Add(button(footer_,"Bibel",[this]{browse_bible();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(12));
    footer->Add(button(footer_,"Kalender",[this]{pick_date();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(12));

    theme_=new wxChoice(footer_,wxID_ANY,wxDefaultPosition,wxDefaultSize,{"System","Ljust","Mörkt"});
    theme_->SetSelection(int(settings_.theme));theme_->SetName("Tema");theme_->SetToolTip("Tema");
    theme_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){settings_.theme=static_cast<Theme>(theme_->GetSelection());apply_settings();});
    footer->Add(theme_,0,wxALIGN_CENTER_VERTICAL);footer_->SetSizer(footer);outer->Add(footer_,0,wxEXPAND|wxALL,FromDIP(20));
    // Reading preferences share one compact, wrapping footer with navigation.
    parallel_=new wxChoice(footer_,wxID_ANY,wxDefaultPosition,wxDefaultSize,{"Huvudtext","+ Grekiska","+ Engelska","Alla tre språk"});
    parallel_->SetSelection(settings_.parallel.empty()?0:settings_.parallel=="el"?1:settings_.parallel=="en"?2:3);
    parallel_->SetName("Parallell text");
    parallel_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){static const char* modes[]={"","el","en","el,en"};settings_.parallel=modes[parallel_->GetSelection()];apply_settings();});
    footer->Add(parallel_,0,wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT,FromDIP(12));
    footer->Add(button(footer_,"A−",[this]{settings_.font_size=std::max(14,settings_.font_size-1);apply_settings();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(6));
    footer->Add(button(footer_,"A+",[this]{settings_.font_size=std::min(28,settings_.font_size+1);apply_settings();}),0,wxALIGN_CENTER_VERTICAL);
    speech_panel_=new wxPanel(root_);speech_panel_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    speech_panel_->Bind(wxEVT_PAINT,&MainFrame::paint_playback,this);
    auto* speech_bar=new wxBoxSizer(wxVERTICAL);
    speech_status_=new wxStaticText(speech_panel_,wxID_ANY,"Välj Lyssna för att höra texten",wxDefaultPosition,wxDefaultSize,wxST_ELLIPSIZE_END|wxST_NO_AUTORESIZE);
    speech_status_->SetFont(ui_font(11));speech_status_->SetMinSize(FromDIP(wxSize(100,-1)));
    speech_detail_=new wxStaticText(speech_panel_,wxID_ANY,"Texten följer med när du lyssnar",wxDefaultPosition,wxDefaultSize,wxST_ELLIPSIZE_END|wxST_NO_AUTORESIZE);
    speech_detail_->SetFont(ui_font(9));
    // Keep both status lines and controls stable when buffering or following changes.
    auto* status_row=new wxBoxSizer(wxVERTICAL);
    status_row->Add(speech_status_,0,wxEXPAND);
    status_row->Add(speech_detail_,0,wxEXPAND|wxTOP,FromDIP(3));
    auto* inset=new wxBoxSizer(wxHORIZONTAL);
    inset->AddSpacer(FromDIP(28));inset->Add(status_row,1);
    auto* controls=new wxBoxSizer(wxHORIZONTAL);
    pause_=button(speech_panel_,"Pausa",[this]{toggle_pause();});
    stop_=button(speech_panel_,"Stoppa",[this]{stop_speech();});
    follow_=button(speech_panel_,"Följ uppläsningen",[this]{follow_speech();});
    pause_->SetToolTip("Pausa eller fortsätt · mellanslag");stop_->SetToolTip("Avsluta uppläsningen · Escape");
    follow_->SetToolTip("Återgå till texten som läses och följ den automatiskt");
    controls->Add(pause_,0,wxRIGHT,FromDIP(8));controls->Add(stop_,0,wxRIGHT,FromDIP(12));controls->AddStretchSpacer();controls->Add(follow_);
    // Reserve the follow button's space so browsing never changes the text viewport.
    controls->GetItem(follow_)->SetFlag(wxRESERVE_SPACE_EVEN_IF_HIDDEN);
    follow_->Hide();speech_body_=new wxBoxSizer(wxHORIZONTAL);
    speech_body_->Add(inset,1,wxEXPAND|wxRIGHT,FromDIP(16));
    speech_body_->Add(controls,0,wxALIGN_CENTER_VERTICAL);
    speech_bar->Add(speech_body_,0,wxEXPAND|wxALL,FromDIP(14));
    speech_panel_->Bind(wxEVT_SIZE,[this,inset,controls](wxSizeEvent& event){
        const bool narrow=event.GetSize().x<FromDIP(700);
        const int orientation=narrow?wxVERTICAL:wxHORIZONTAL;
        if(speech_body_->GetOrientation()!=orientation) {
            speech_body_->SetOrientation(orientation);
            auto* labels=speech_body_->GetItem(inset);auto* buttons=speech_body_->GetItem(controls);
            labels->SetProportion(narrow?0:1);labels->SetFlag(wxEXPAND|(narrow?wxBOTTOM:wxRIGHT));labels->SetBorder(FromDIP(narrow?10:16));
            buttons->SetFlag(narrow?int(wxEXPAND):int(wxALIGN_CENTER_VERTICAL));
            root_->Layout();
        }
        event.Skip();
    });
    pause_->Disable();stop_->Disable();
    speech_panel_->SetSizer(speech_bar);
    outer->Insert(outer->GetItemCount()-1,speech_panel_,0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,FromDIP(20));
    scripture_->on_release_follow([this]{following_audio_=false;refresh_speech();});
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){refresh_speech();},playback_timer_.GetId());
    Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& event){
        const bool active=playback_ui_.state==SpeechState::Playing||playback_ui_.state==SpeechState::Paused||playback_ui_.state==SpeechState::Buffering||playback_ui_.state==SpeechState::Loading;
        if(active&&event.GetKeyCode()==WXK_ESCAPE){stop_speech();return;}
        if(active&&event.GetKeyCode()==WXK_SPACE&&wxWindow::FindFocus()==scripture_){toggle_pause();return;}
        event.Skip();
    });
    // The worker copies the shared owner, not wxWeakRef's main-thread tracking data.
    auto weak=std::make_shared<wxWeakRef<MainFrame>>(this);
    auto latest_speech=std::make_shared<std::atomic<uint64_t>>(0);
    // ToUTF8 returns a scoped view. Keep its wxString owner alive until the path is copied.
    const auto data_dir=wxStandardPaths::Get().GetUserLocalDataDir();
    const auto data_utf8=data_dir.ToUTF8();
    const auto data_path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(data_utf8.data()),data_utf8.length()));
    speech_=create_portable_speech(data_path,[weak,latest_speech](const SpeechUpdate& update){
        auto known=latest_speech->load();
        while(known<update.sequence&&!latest_speech->compare_exchange_weak(known,update.sequence)){}
        if(update.sequence<known)return;
        auto deliver=[weak,latest_speech,update]{if(*weak&&latest_speech->load()==update.sequence)(*weak)->speech_status(update.text);};
        // Delivery must run after the engine releases its mutex, including
        // Pause and Stop callbacks made on the main thread.
        wxTheApp->CallAfter(deliver);
    });
    root_->SetSizer(outer);auto* frame_sizer=new wxBoxSizer(wxVERTICAL);frame_sizer->Add(root_,1,wxEXPAND);SetSizer(frame_sizer);
    refresh_day();show_readings();apply_settings(false);
    const auto work=wxGetClientDisplayRect();
    SetSize(std::min(1120,work.width-48),std::min(900,work.height-48));Center();
    Bind(wxEVT_SYS_COLOUR_CHANGED,[this](wxSysColourChangedEvent& e){if(settings_.theme==Theme::System)apply_settings(false);e.Skip();});
}
void MainFrame::navigate(int days) {
    try{selected_.move(days);show_readings();refresh_day();}
    catch(const std::exception& e){wxMessageBox(u(e.what()),"Datum",wxOK|wxICON_INFORMATION,this);}
}
void MainFrame::refresh_day() {
    day_=lectionary_.readings_for(selected_.date(),settings_.calendar);
    date_->SetLabel(u(date_swedish(selected_.date())));
    annotation_->SetLabel(settings_.calendar==CalendarStyle::New?"Antiochia":"Antiochia · gamla kalendern");
    annotation_->SetToolTip(u("Kalenderkälla: Antiochian Orthodox Christian Archdiocese of North America\n"+day_.day.annotation+"\nFast kalender: "+day_.day.fixed_cycle.value_or("")+"\n"+day_.day.paschal_cycle.value_or("")));
    entries_->Clear(true);entries_->AddSpacer(FromDIP(38));
    if(day_.readings.empty()) {
        entries_->Add(label(readings_,"Ingen daglig bibelläsning är föreskriven",18),0,wxBOTTOM,FromDIP(20));
        entries_->Add(button(readings_,"Öppna Bibeln",[this]{browse_bible();}),0);
    } else {
        const auto info=day_.day.annotation.find(" · ",day_.day.annotation.find(" · ")+3);
        if(info!=std::string::npos) {
            auto* feast=label(readings_,u(day_.day.annotation.substr(info+3)),14);
            feast->Wrap(std::max(200,GetClientSize().x-FromDIP(150)));entries_->Add(feast,0,wxBOTTOM,FromDIP(28));
        }
        for(const auto& reading:day_.readings) {
            auto* section=label(readings_,kind_label(reading.kind),10);entries_->Add(section,0,wxBOTTOM,FromDIP(10));
            auto* line=new wxWrapSizer(wxHORIZONTAL,wxREMOVE_LEADING_SPACES);
            auto* open=button(readings_,u(reading.label),[this,reading]{open_reading(reading);});open->SetFont(body_font(20));
            line->Add(open,0,wxALIGN_CENTER_VERTICAL);line->Add(button(readings_,"Lyssna",[this,reading]{play_speech({reading});}),0,wxALIGN_CENTER_VERTICAL|wxLEFT,FromDIP(16));
            entries_->Add(line,0,wxEXPAND|wxBOTTOM,FromDIP(36));
        }
        entries_->AddSpacer(FromDIP(10));
        listen_all_=button(readings_,"Lyssna på läsningarna",[this]{play_speech(day_.readings);});
        entries_->Add(listen_all_,0,wxALIGN_CENTER|wxBOTTOM,FromDIP(16));

    }
    readings_->FitInside();readings_->Scroll(0,0);apply_settings(false);Layout();
}
void MainFrame::show_readings() {
    following_audio_=false;if(scripture_)scripture_->follow_playback(false);
    scripture_->Hide();reader_header_->Hide();readings_->Show();root_->Layout();
}
void MainFrame::open_psalm() { open_reading({ReadingKind::MorningPsalm,{"Ps",{23,1},{23,6}},"Psalm 23"}); }
void MainFrame::open_reading(const Reading& selected) {
    visible_reading_=selected;following_audio_=false;speech_view_.reset();scripture_->follow_playback(false);
    // Lectionary references use their reference edition's numbering; open them in the reader's.
    const auto reading=corpus_.localize(selected);
    readings_->Hide();reader_header_->Show();scripture_->Show();
    reader_label_->SetLabel(u(reading.label));
    part_->Clear();const auto segments=reading.segments();
    for(std::size_t i=0;i<segments.size();++i)part_->Append(wxString::Format("Del %d · ",int(i+1))+u(corpus_.book_name(segments[i].book)));
    part_->SetSelection(0);part_->Show(segments.size()>1);
    root_->Layout();scripture_->open(reading);scripture_->SetFocus();
}
void MainFrame::browse_bible() {
    wxDialog dialog(this,wxID_ANY,"Öppna Bibeln",wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    auto books=corpus_.books();wxArrayString names;
    for(const auto& book:books)names.Add(u(book.name));
    auto* book=new wxChoice(&dialog,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(320,-1)),names);book->SetSelection(0);
    auto* edition=new wxChoice(&dialog,wxID_ANY,wxDefaultPosition,wxDefaultSize,{"Svenska 1917 med apokryfer","Grekiska · LXX / Patriarkal 1904","English · King James","English · World English Bible med deuterokanon"});edition->SetSelection(0);
    const auto update_books=[&] {
        const auto previous=book->GetSelection()>=0?books[book->GetSelection()].code:std::string{};
        books.clear();book->Clear();
        const auto language=edition->GetSelection()==0?"sv":edition->GetSelection()==1?"el":"en";
        for(const auto& candidate:corpus_.books()) {
            const auto source=edition->GetSelection()==3?"en-web":edition->GetSelection()==2?"en-kjv":source_for_language(language,candidate.code);
            if(corpus_.coordinates(source,candidate.code).empty())continue;
            books.push_back(candidate);book->Append(u(candidate.name));
        }
        auto found=std::find_if(books.begin(),books.end(),[&](const auto& candidate){return candidate.code==previous;});
        book->SetSelection(found==books.end()?0:int(found-books.begin()));
    };
    edition->Bind(wxEVT_CHOICE,[&](wxCommandEvent&){update_books();});update_books();
    auto* chapter=new wxTextCtrl(&dialog,wxID_ANY,"1");
    auto* verse=new wxTextCtrl(&dialog,wxID_ANY,"1");
    auto* sizer=new wxBoxSizer(wxVERTICAL);
    sizer->Add(label(&dialog,"Bok",11),0,wxLEFT|wxTOP,FromDIP(18));sizer->Add(book,0,wxALL,FromDIP(18));
    sizer->Add(label(&dialog,"Utgåva",11),0,wxLEFT,FromDIP(18));sizer->Add(edition,0,wxEXPAND|wxALL,FromDIP(18));
    sizer->Add(label(&dialog,"Kapitel",11),0,wxLEFT,FromDIP(18));sizer->Add(chapter,0,wxEXPAND|wxALL,FromDIP(18));
    sizer->Add(label(&dialog,"Vers",11),0,wxLEFT,FromDIP(18));sizer->Add(verse,0,wxEXPAND|wxALL,FromDIP(18));
    sizer->Add(dialog.CreateButtonSizer(wxOK|wxCANCEL),0,wxALIGN_RIGHT|wxALL,FromDIP(18));dialog.SetSizerAndFit(sizer);
    recolor(&dialog,palette(settings_.theme));
    while(dialog.ShowModal()==wxID_OK) {
        long ch=0,v=0;const auto& selected=books[book->GetSelection()];
        const auto coordinate=verse->GetValue().ToStdString();
        const auto suffix_start=coordinate.find_first_not_of("0123456789");
        const auto suffix=suffix_start==std::string::npos?std::string{}:coordinate.substr(suffix_start);
        const bool valid_suffix=suffix.size()<=2&&std::all_of(suffix.begin(),suffix.end(),[](char c){return c>='a'&&c<='z';});
        if(!chapter->GetValue().ToLong(&ch)||!u(coordinate.substr(0,suffix_start)).ToLong(&v)||!valid_suffix||ch<1||v<1||ch>999||v>999) {
            wxMessageBox("Ange kapitel och vers, till exempel 1 eller 50a.","Bibelställe",wxOK|wxICON_INFORMATION,&dialog);continue;
        }
        const VerseRef ref{int(ch),int(v),suffix};
        const std::string language=edition->GetSelection()==0?"sv":edition->GetSelection()==1?"el":"en";
        const auto source=edition->GetSelection()==3?"en-web":edition->GetSelection()==2?"en-kjv":source_for_language(language,selected.code);
        if(!corpus_.verse(source,selected.code,ref)) {
            wxMessageBox("Bibelstället saknas i de bundna utgåvorna.","Bibelställe",wxOK|wxICON_INFORMATION,&dialog);continue;
        }
        Reading reading{new_testament_book(selected.code)?ReadingKind::Gospel:ReadingKind::OldTestament,
            {selected.code,ref,ref},selected.name+" "+std::to_string(ch)+":"+std::to_string(v)+suffix,{},language};
        reading.source_override=source;open_reading(reading);break;
    }
}
void MainFrame::pick_date() {
    const auto current=selected_.date();
    wxDialog dialog(this,wxID_ANY,"Välj civilt datum",wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    auto* sizer=new wxBoxSizer(wxVERTICAL);
    auto* calendar=new wxCalendarCtrl(&dialog,wxID_ANY,
        wxDateTime(unsigned(current.day()),static_cast<wxDateTime::Month>(unsigned(current.month())-1),int(current.year())),
        wxDefaultPosition,wxDefaultSize,wxCAL_MONDAY_FIRST|wxCAL_SHOW_HOLIDAYS);
    sizer->Add(calendar,0,wxALL,FromDIP(20));

    sizer->Add(button(&dialog,"Aktuellt datum",[calendar]{calendar->SetDate(wxDateTime::Today());}),0,wxALIGN_CENTER|wxBOTTOM,FromDIP(12));
    sizer->Add(dialog.CreateButtonSizer(wxOK|wxCANCEL),0,wxALIGN_RIGHT|wxALL,FromDIP(12));
    dialog.SetSizerAndFit(sizer);recolor(&dialog,palette(settings_.theme));
    if(dialog.ShowModal()!=wxID_OK)return;
    const auto value=calendar->GetDate();
    selected_.select(std::chrono::year{value.GetYear()}/(int(value.GetMonth())+1)/value.GetDay());
    show_readings();refresh_day();
}
void MainFrame::apply_settings(bool persist) {
#if wxCHECK_VERSION(3,3,0)
    static std::optional<Theme> native_theme;
    if(native_theme!=settings_.theme) {
        native_theme=settings_.theme;
        wxTheApp->SetAppearance(static_cast<wxApp::Appearance>(settings_.theme));
    }
#endif
    if(persist) { try{user_.save(settings_);}catch(const std::exception& e){wxMessageBox(u(e.what()),"Inställningar kunde inte sparas",wxOK|wxICON_ERROR,this);} }
    recolor(root_,palette(settings_.theme));annotation_->SetForegroundColour(palette(settings_.theme).muted);
    speech_detail_->SetForegroundColour(palette(settings_.theme).muted);
    scripture_->apply(settings_);root_->Layout();
}
MainFrame::~MainFrame(){playback_timer_.Stop();speech_.reset();}
void MainFrame::speech_status(const std::string& status) {
    if(playback_ui_.state==SpeechState::Error&&speech_->playback().state==SpeechState::Stopped)return;
    speech_message_=status;refresh_speech();
}
void MainFrame::toggle_pause() {
    if(speech_->playback().state==SpeechState::Paused)speech_->resume();else speech_->pause();
    refresh_speech();
}
void MainFrame::stop_speech() {
    speech_->stop();following_audio_=false;scripture_->follow_playback(false);refresh_speech();
}
void MainFrame::follow_speech() {
    if(speech_readings_.empty())return;
    const auto playback=speech_->playback();
    const size_t reading=playback.cue?playback.cue->reading:0,section=playback.cue?playback.cue->section:0;
    if(reading>=speech_readings_.size())return;
    if(!speech_view_||*speech_view_!=std::pair{reading,section}||!scripture_->IsShown()) {
        open_reading(speech_readings_[reading]);
        if(section){scripture_->open_section(section);part_->SetSelection(int(section));}
        speech_view_=std::pair{reading,section};
    }
    following_audio_=true;scripture_->playback(playback);scripture_->follow_playback();scripture_->SetFocus();
    refresh_speech();
}
void MainFrame::refresh_speech() {
    if(speech_)display_playback(speech_->playback());
}
void MainFrame::display_playback(const SpeechPlayback& playback) {
    const auto now=std::chrono::steady_clock::now();
    if(playback.state==SpeechState::Buffering&&playback_ui_.state!=SpeechState::Buffering) {
        buffering_since_=now;debounce_buffering_=playback_ui_.state==SpeechState::Playing&&playback.cue.has_value();
    } else if(playback.state!=SpeechState::Buffering)debounce_buffering_=false;
    playback_ui_=playback;
    const auto state=playback.state;
    // Freeze tracking immediately, but do not flash a spinner for a brief
    // gap between audio callbacks. Startup feedback remains immediate.
    feedback_state_=state==SpeechState::Buffering&&debounce_buffering_&&now-buffering_since_<std::chrono::milliseconds(180)?SpeechState::Playing:state;
    const bool active=state==SpeechState::Playing||state==SpeechState::Paused||state==SpeechState::Buffering||state==SpeechState::Loading;
    if(active&&following_audio_&&playback.cue&&playback.cue->reading<speech_readings_.size()) {
        const auto view=std::pair{playback.cue->reading,playback.cue->section};
        if(!speech_view_||*speech_view_!=view) {
            open_reading(speech_readings_[view.first]);
            if(view.second){scripture_->open_section(view.second);part_->SetSelection(int(view.second));}
            speech_view_=view;following_audio_=true;scripture_->follow_playback();
        }
    }
    if(!active){following_audio_=false;scripture_->follow_playback(false);playback_timer_.Stop();}
    scripture_->playback(playback);
    wxString location;
    if(playback.cue) {
        const auto& cue=*playback.cue;
        if(cue.introduction)location="Introduktion";
        else location=u(corpus_.book_name(cue.book))+wxString::Format(" %d:%d",cue.verse.chapter,cue.verse.verse)+u(cue.verse.suffix);
    }
    wxString title,detail;
    switch(feedback_state_) {
    case SpeechState::Loading:title="Laddar rösten…";detail="Första starten kan ta en stund";break;
    case SpeechState::Buffering:
        title=playback.cue?"Väntar på ljud…":"Förbereder uppläsningen…";
        detail=playback.cue?"Markören fortsätter när ljudet är klart":"Ljudet startar när den första delen är klar";break;
    case SpeechState::Playing:title=location.empty()?"Läser":("Läser · "+location);detail=following_audio_?"Texten följer uppläsningen":"Du bläddrar själv · ljudet fortsätter";break;
    case SpeechState::Paused:title=location.empty()?"Pausad":("Pausad · "+location);detail="Fortsätt där du pausade";break;
    case SpeechState::Stopped:title="Uppläsningen är stoppad";detail="Välj Lyssna för att börja om";break;
    case SpeechState::Completed:title="Läsningen är klar";detail="Du har nått slutet av läsningen";break;
    case SpeechState::Error:title="Uppläsningen kunde inte fortsätta";detail=u(speech_message_);break;
    case SpeechState::Idle:title="Välj Lyssna för att höra texten";detail="Texten följer med när du lyssnar";break;
    }
    if(feedback_state_==SpeechState::Buffering&&!location.empty())detail=location+" · "+detail;
    if(speech_status_->GetLabel()!=title)speech_status_->SetLabel(title);
    if(speech_detail_->GetLabel()!=detail)speech_detail_->SetLabel(detail);
    speech_status_->SetToolTip(title);speech_detail_->SetToolTip(detail);
    paused_=state==SpeechState::Paused;
    const wxString pause_label=paused_?"Fortsätt":"Pausa";
    bool layout=pause_->GetLabel()!=pause_label;
    pause_->SetLabel(pause_label);pause_->Enable(active);stop_->Enable(active);
    const bool follow=active&&!following_audio_;
    layout|=follow_->IsShown()!=follow;follow_->Show(follow);
    if(layout){speech_panel_->Layout();root_->Layout();}
    speech_detail_->SetForegroundColour(palette(settings_.theme).muted);
    speech_panel_->Refresh(false);
}
void MainFrame::paint_playback(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(speech_panel_);const auto colors=palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));dc.Clear();
    const auto size=speech_panel_->GetClientSize();
    dc.SetPen(wxPen(colors.rule,1));dc.DrawLine(0,0,size.x,0);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if(!gc)return;
    gc->SetPen(*wxTRANSPARENT_PEN);gc->SetBrush(wxBrush(colors.accent));
    if(playback_ui_.progress>0)gc->DrawRoundedRectangle(0,0,size.x*playback_ui_.progress,FromDIP(2),FromDIP(1));
    const double x=FromDIP(21),y=FromDIP(27),r=FromDIP(5);
    if(feedback_state_==SpeechState::Loading||feedback_state_==SpeechState::Buffering) {
        const double time=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto path=gc->CreatePath();path.AddArc(x,y,r,time*4,time*4+4.5,true);
        gc->SetPen(wxPen(colors.accent,FromDIP(2)));gc->StrokePath(path);
    } else if(feedback_state_==SpeechState::Paused) {
        gc->DrawRoundedRectangle(x-r,y-r,FromDIP(3),r*2,FromDIP(1));
        gc->DrawRoundedRectangle(x+FromDIP(2),y-r,FromDIP(3),r*2,FromDIP(1));
    } else {
        gc->SetBrush(wxBrush(feedback_state_==SpeechState::Playing?colors.accent:colors.muted));
        gc->DrawEllipse(x-r,y-r,r*2,r*2);
    }
}
void MainFrame::play_speech(const std::vector<Reading>& readings) {
    stop_speech();speech_readings_=readings;speech_view_.reset();
    try {
        std::vector<SpeechUtterance> queue;
        for(size_t r=0;r<readings.size();++r) {
            const auto& reading=readings[r];const auto localized=corpus_.localize(reading);const auto passages=localized.segments();
            if(reading.base_language=="sv") {
                auto intro=make_utterance(reading_introduction(reading),"sv",corpus_.pronunciations("sv"));
                intro.cue=SpeechCue{r,0,passages.front().book,"",passages.front().first,passages.front().last,true};queue.push_back(std::move(intro));
            }
            for(size_t s=0;s<passages.size();++s) {
                const auto& passage=passages[s];
                auto language=reading.base_language;
                auto source=reading.source_override.empty()?source_for_language(language,passage.book):reading.source_override;
                if(corpus_.coordinates(source,passage.book).empty()) {
                    for(const auto fallback:{"el","en"}) {
                        auto candidate=source_for_language(fallback,passage.book);
                        if(!corpus_.coordinates(candidate,passage.book).empty()){source=candidate;break;}
                    }
                }
                if(source.starts_with("en-"))language="en";
                else if(source.starts_with("grc-"))language="el";
                else language="sv";
                const auto lexicon=corpus_.pronunciations(language);
                bool found=false;
                for(auto ref:corpus_.coordinates(source,passage.book)) {
                    if(ref>passage.last)break;
                    auto verse=corpus_.verse(source,passage.book,ref);
                    if(verse && ref<=passage.last && passage.first<=verse->last.value_or(ref)) {
                        auto utterance=make_utterance(verse->text,language,lexicon);
                        utterance.cue=SpeechCue{r,s,passage.book,source,verse->ref,verse->last.value_or(ref),false};
                        queue.push_back(std::move(utterance));found=true;
                    }
                }
                if(!found)throw std::runtime_error("Ingen text finns att läsa upp.");
            }
        }
        if(!readings.empty()){open_reading(readings.front());speech_view_=std::pair<size_t,size_t>{0,0};}
        speech_->speak_batch(queue);
        following_audio_=true;scripture_->follow_playback();playback_timer_.Start(30);refresh_speech();
    } catch(const std::exception& error) {
        stop_speech();speech_message_=error.what();display_playback({SpeechState::Error,{},0,0});
        wxMessageBox(u(error.what()),"Uppläsning",wxOK|wxICON_INFORMATION,this);
    }
}
bool MainFrame::smoke_test(const wxString& screenshot_path) {
    const auto original=settings_;const auto date=selected_.date();
    open_psalm();Layout();
    bool ok=corpus_.read_only() && wxFontEnumerator::IsValidFacename("Literata") && wxFontEnumerator::IsValidFacename("IBM Plex Sans") && wxFontEnumerator::IsValidFacename("Noto Serif Hebrew") && wxFontEnumerator::IsValidFacename("Noto Sans Math");
    // Native drawing must use the same shaped widths as paragraph fitting.
    wxClientDC metrics(scripture_);metrics.SetFont(body_font(19));
    for(const auto& [language,text]:std::vector<std::pair<std::string,wxString>>{
        {"sv","I begynnelsen skapade Gud himmel och jord. Och Gud såg att det var gott. Detta är en längre text för att kontrollera styckets jämna radbrytning och mellanrum."},
        {"el",wxString::FromUTF8("Ἐν ἀρχῇ ἦν ὁ Λόγος, καὶ ὁ Λόγος ἦν πρὸς τὸν Θεόν, καὶ Θεὸς ἦν ὁ Λόγος. Οὗτος ἦν ἐν ἀρχῇ πρὸς τὸν Θεόν.")},
        {"en","In the beginning was the Word, and the Word was with God, and the Word was God. The same was in the beginning with God."}}) {
        const auto layout=layout_paragraph(metrics,text,280,language);
        wxString recovered;bool justified=false;
        for(std::size_t i=0;i<layout.lines.size();++i) {
            const auto& line=layout.lines[i];
            if(line.width>281)ok=false;
            if(line.justified){justified=true;if(std::abs(line.width-280)>0.01)ok=false;}
            if(i+1==layout.lines.size()&&line.justified)ok=false;
            for(std::size_t w=0;w<line.runs.size();++w) {
                auto run=line.runs[w].text;
                if(line.hyphenated&&w+1==line.runs.size())run.RemoveLast();
                recovered+=run;
            }
        }
        auto original=text;original.Replace(" ","");
        if(recovered!=original||!justified)ok=false;
    }
    {
        const std::vector<wxString> words={"one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve"};
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsRenderer::GetDefaultRenderer()->CreateMeasuringContext());
        gc->SetFont(metrics.GetFont(),*wxBLACK);
        std::vector<double> widths;double space=0;gc->GetTextExtent(" ",&space,nullptr);
        wxString text;
        for(const auto& word:words){double w=0;gc->GetTextExtent(word,&w,nullptr);widths.push_back(w);if(!text.empty())text+=" ";text+=word;}
        bool beats_greedy=false;int verified=0;
        for(int width=160;width<=400;width+=2) {
            double best=std::numeric_limits<double>::infinity();
            std::function<void(int,int,double)> enumerate=[&](int start,int previous,double cost) {
                if(start==int(words.size())){best=std::min(best,cost);return;}
                double natural=0;
                for(int end=start;end<int(words.size());++end) {
                    natural+=widths[end]+(end>start?space:0);
                    const int gaps=end-start;const double difference=width-natural;
                    double ratio=difference/std::max(1.0,gaps*space*(difference<0?1.0/3.0:0.5));
                    if(end+1==int(words.size())&&difference>=0)ratio=0;
                    const double badness=100*std::pow(std::abs(ratio),3);
                    if(ratio < -1||badness>100)continue;
                    const int fitness=ratio<-0.5?0:ratio<=0.5?1:ratio<=1?2:3;
                    enumerate(end+1,fitness,cost+std::pow(10+badness,2)+(std::abs(previous-fitness)>1?10000:0));
                }
            };
            enumerate(0,1,0);
            if(!std::isfinite(best))continue;
            const auto fit=layout_paragraph(metrics,text,width,"no-patterns");
            if(std::abs(fit.demerits-best)>0.001)ok=false;
            ++verified;
            int greedy=0;double length=0;
            while(greedy<int(words.size())&&length+widths[greedy]+(greedy?space:0)<=width){length+=widths[greedy]+(greedy?space:0);++greedy;}
            beats_greedy|=int(fit.lines.front().runs.size())!=greedy;
        }
        if(!verified||!beats_greedy)ok=false;
        std::cout<<"Paragraph oracle: "<<verified<<" globally optimal fits; differs from greedy="<<beats_greedy<<"\n";
        const std::vector<TextFragment> spans={{"I begynnelsen skapade Gud himmel och jord.","1",0},{"Och Gud såg att det var gott.","2",1}};
        const auto paragraph=layout_paragraph(metrics,spans,280,"sv");
        int markers=0;wxString recovered;
        for(const auto& line:paragraph.lines)for(std::size_t i=0;i<line.runs.size();++i) {
            const auto& run=line.runs[i];
            if(run.marker){++markers;continue;}
            auto token=run.text;if(line.hyphenated&&i+1==line.runs.size())token.RemoveLast();recovered+=token;
        }
        wxString expected;for(const auto& span:spans){auto source=span.text;source.Replace(" ","");expected+=source;}
        if(markers!=2||recovered!=expected)ok=false;
    }
    if(hyphenation_points("begynnelsen","sv").empty()||hyphenation_points("beginning","en").empty()||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"),"el").empty())ok=false;
    if(!hyphenation_points("project","en").empty())ok=false;
    const auto optical=layout_paragraph(metrics,wxString::FromUTF8("“Guds ord.”"),280,"sv");
    if(optical.lines.front().left_protrusion<=0||optical.lines.back().right_protrusion<=0)ok=false;
    settings_.theme=Theme::Light;settings_.parallel="el";apply_settings(false);scripture_->center_passage();
    const double start=scripture_->scroll_position();scripture_->scroll_by(0.375);
    if(std::abs(scripture_->scroll_position()-start-0.375)>0.001)ok=false;
    scripture_->scroll_by(-0.375);
    const auto viewport=scripture_->GetClientSize();
    wxBitmap before(viewport.x,viewport.y),after(viewport.x,viewport.y);
    {wxMemoryDC dc(before);scripture_->render_to(dc,viewport);}
    scripture_->scroll_by(17);
    if(std::abs(scripture_->scroll_position()-start-17)>0.001)ok=false;
    {wxMemoryDC dc(after);scripture_->render_to(dc,viewport);}
    const auto first=before.ConvertToImage(),second=after.ConvertToImage();
    std::size_t equal=0,total=0;
    for(int y=30;y<viewport.y-60;++y)for(int x=0;x<viewport.x;++x) {
        ++total;
        if(first.GetRed(x,y+17)==second.GetRed(x,y)&&first.GetGreen(x,y+17)==second.GetGreen(x,y)&&first.GetBlue(x,y+17)==second.GetBlue(x,y))++equal;
    }
    if(total==0||double(equal)/total<0.995)ok=false;
    for(auto theme:{Theme::Light,Theme::Dark,Theme::System})for(auto mode:{"","el","en","el,en"}) {
        settings_.theme=theme;settings_.parallel=mode;apply_settings(false);scripture_->center_passage();
        const auto size=scripture_->GetClientSize();
        if(size.x<1||size.y<1){ok=false;continue;}
        wxBitmap bitmap(size.x,size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,size);dc.SelectObject(wxNullBitmap);
        auto image=bitmap.ConvertToImage();const auto colors=palette(settings_.theme);
        std::size_t changed=0;
        for(int y=0;y<image.GetHeight();++y)for(int x=0;x<image.GetWidth();++x)
            if(image.GetRed(x,y)!=colors.paper.Red()||image.GetGreen(x,y)!=colors.paper.Green()||image.GetBlue(x,y)!=colors.paper.Blue())++changed;
        if(changed<200||scripture_->cached_rows()>192)ok=false;
        if(!screenshot_path.empty()&&std::string(mode)=="el"&&(theme==Theme::Light||theme==Theme::Dark)) {
            const wxSize page_size(size.x,1100);
            wxBitmap page(page_size.x,page_size.y);wxMemoryDC page_dc(page);scripture_->render_to(page_dc,page_size);page_dc.SelectObject(wxNullBitmap);
            const wxString output=theme==Theme::Light?screenshot_path:screenshot_path.BeforeLast('.')+"-dark.png";
            ok=page.ConvertToImage().SaveFile(output,wxBITMAP_TYPE_PNG)&&ok;
        }
    }
    const auto original_size=GetSize();
    SetSize(FromDIP(wxSize(520,650)));Layout();root_->Layout();
    settings_.parallel="el,en";settings_.theme=Theme::Dark;apply_settings(false);scripture_->center_passage();
    const auto narrow_size=scripture_->GetClientSize();
    if(narrow_size.x<100||narrow_size.y<100)ok=false;
    else {
        wxBitmap bitmap(narrow_size.x,narrow_size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,narrow_size);dc.SelectObject(wxNullBitmap);
        if(!screenshot_path.empty())ok=bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.')+"-narrow.png",wxBITMAP_TYPE_PNG)&&ok;
    }
    SetSize(original_size);Layout();
    if(calendar_->GetString(0)!="Nya kalendern"||calendar_->GetString(1)!="Gamla kalendern")ok=false;
    settings_.parallel="";apply_settings(false);
    for(auto reading:std::vector<Reading>{
        {ReadingKind::OldTestament,{"Gen",{31,50,"a"},{31,50,"a"}},"Första Moseboken 31:50a",{},"el"},
        {ReadingKind::OldTestament,{"4Macc",{8,29},{8,29}},"Fjärde Mackabeerboken 8:29",{},"en"},
        {ReadingKind::OldTestament,{"2Esd",{1,1},{1,1}},"Andra Esdrasboken 1:1"}}) {
        open_reading(reading);scripture_->center_passage();
        if(scripture_->cached_rows()==0)ok=false;
        const auto size=scripture_->GetClientSize();wxBitmap bitmap(size.x,size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,size);dc.SelectObject(wxNullBitmap);
        if(!screenshot_path.empty())ok=bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.')+"-"+u(reading.passage.book)+".png",wxBITMAP_TYPE_PNG)&&ok;
    }
    open_reading({ReadingKind::Epistle,{"1Cor",{4,9},{4,16}},"Första Korintierbrevet 4:9–16"});
    if(!screenshot_path.empty()) {
        const auto size=scripture_->GetClientSize();wxBitmap bitmap(size.x,size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,size);dc.SelectObject(wxNullBitmap);
        ok=bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.')+"-prose.png",wxBITMAP_TYPE_PNG)&&ok;
    }
    // Exercise playback presentation without model loading or audible output.
    open_psalm();settings_.theme=Theme::Light;settings_.parallel="el";apply_settings(false);
    display_playback({SpeechState::Loading,{},0,0});
    if(speech_status_->GetLabel()!="Laddar rösten…"||!pause_->IsEnabled()||!stop_->IsEnabled())ok=false;
    display_playback({SpeechState::Buffering,{},0,0});
    if(speech_status_->GetLabel()!="Förbereder uppläsningen…"||scripture_->marker_position())ok=false;
    SpeechPlayback playing{SpeechState::Playing,SpeechCue{0,0,"Ps","sv1917",{23,3},{23,3},false},0.35,0.4};
    following_audio_=true;scripture_->playback(playing);scripture_->follow_playback();
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position()||!scripture_->follows_playback())ok=false;
    const auto held_marker=scripture_->marker_position();const auto held_scroll=scripture_->scroll_position();
    auto paused=playing;paused.state=SpeechState::Paused;display_playback(paused);
    for(int i=0;i<30;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()!=held_marker||scripture_->scroll_position()!=held_scroll||pause_->GetLabel()!="Fortsätt"||!stop_->IsEnabled())ok=false;
    auto buffering=playing;buffering.state=SpeechState::Buffering;display_playback(buffering);
    for(int i=0;i<30;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()!=held_marker||scripture_->scroll_position()!=held_scroll||speech_status_->GetLabel()!="Väntar på ljud…")ok=false;
    display_playback(playing);display_playback(buffering);
    if(feedback_state_!=SpeechState::Playing)ok=false;
    buffering_since_=std::chrono::steady_clock::now()-std::chrono::seconds(1);display_playback(buffering);
    if(feedback_state_!=SpeechState::Buffering)ok=false;
    scripture_->scroll_by(20);
    if(scripture_->follows_playback()||following_audio_)ok=false;
    following_audio_=true;display_playback(playing);scripture_->follow_playback();
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    const auto save_playback=[&](const wxString& suffix) {
        if(screenshot_path.empty())return;
        const auto size=scripture_->GetClientSize();wxBitmap bitmap(size.x,size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,size);dc.SelectObject(wxNullBitmap);
        ok=bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.')+suffix+".png",wxBITMAP_TYPE_PNG)&&ok;
    };
    save_playback("-playing");
    settings_.theme=Theme::Dark;apply_settings(false);scripture_->playback(playing);
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    save_playback("-playing-dark");
    // Reflow keeps the spoken verse, including the other language column.
    settings_.font_size=24;apply_settings(false);scripture_->playback(playing);
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position())ok=false;
    auto greek=playing;greek.cue->source="grc-lxx";greek.cue->verse=greek.cue->last={22,3};scripture_->playback(greek);
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position())ok=false;
    save_playback("-playing-greek");
    SetSize(FromDIP(wxSize(520,650)));Layout();root_->Layout();display_playback(paused);
    for(auto* control:{pause_,stop_,follow_})if(control->GetRect().GetRight()>speech_panel_->GetClientSize().x)ok=false;
    save_playback("-playing-narrow");
    SetSize(original_size);Layout();root_->Layout();settings_.font_size=original.font_size;apply_settings(false);
    scripture_->playback(playing);scripture_->follow_playback();
    auto distant=playing;distant.cue->verse=distant.cue->last={24,10};scripture_->playback(distant);
    for(int i=0;i<160;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position()||scripture_->scroll_position()<=held_scroll+100)ok=false;
    display_playback({SpeechState::Stopped,{},0,0});
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()||pause_->IsEnabled()||stop_->IsEnabled())ok=false;
    std::cout<<"Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_psalm();
    settings_=original;apply_settings(false);ok=ok && selected_.date()==date;
    return ok;
}
}
