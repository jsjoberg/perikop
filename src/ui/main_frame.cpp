#include "ui/main_frame.hpp"
#include "ui/pronunciation_review.hpp"
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
#include <wx/menu.h>
#include <wx/stdpaths.h>
#include <limits>
#include <map>
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
// Short Swedish book names for the Bible picker grid.
std::string book_abbreviation(const std::string& book) {
    static const std::map<std::string,std::string> names={
        {"Gen","1 Mos"},{"Exod","2 Mos"},{"Lev","3 Mos"},{"Num","4 Mos"},{"Deut","5 Mos"},{"Josh","Jos"},{"Judg","Dom"},{"Ruth","Rut"},
        {"1Sam","1 Sam"},{"2Sam","2 Sam"},{"1Kgs","1 Kung"},{"2Kgs","2 Kung"},{"1Chr","1 Krön"},{"2Chr","2 Krön"},{"Ezra","Esra"},{"Neh","Neh"},
        {"Esth","Est"},{"Job","Job"},{"Ps","Ps"},{"Prov","Ords"},{"Eccl","Pred"},{"Song","Höga v"},{"Isa","Jes"},{"Jer","Jer"},{"Lam","Klag"},
        {"Ezek","Hes"},{"Dan","Dan"},{"Hos","Hos"},{"Joel","Joel"},{"Amos","Am"},{"Obad","Ob"},{"Jonah","Jona"},{"Micah","Mika"},{"Nah","Nah"},
        {"Hab","Hab"},{"Zeph","Sef"},{"Hag","Hagg"},{"Zech","Sak"},{"Mal","Mal"},
        {"Tob","Tob"},{"Jdt","Judit"},{"EsthGr","T Est"},{"Wis","Vish"},{"Sir","Syr"},{"Baruch","Bar"},{"EpJer","Jer br"},{"PrAzar","Asarj"},
        {"Sus","Sus"},{"Bel","Bel"},{"DanGr","Dan gr"},{"1Macc","1 Mack"},{"2Macc","2 Mack"},{"3Macc","3 Mack"},{"4Macc","4 Mack"},
        {"1Esd","1 Esd"},{"2Esd","2 Esd"},{"PrMan","Man"},{"Ps151","Ps 151"},
        {"Matt","Matt"},{"Mark","Mark"},{"Luke","Luk"},{"John","Joh"},{"Acts","Apg"},{"Rom","Rom"},{"1Cor","1 Kor"},{"2Cor","2 Kor"},
        {"Gal","Gal"},{"Eph","Ef"},{"Phil","Fil"},{"Col","Kol"},{"1Thess","1 Tess"},{"2Thess","2 Tess"},{"1Tim","1 Tim"},{"2Tim","2 Tim"},
        {"Titus","Tit"},{"Philemon","Filem"},{"Heb","Hebr"},{"James","Jak"},{"1Peter","1 Petr"},{"2Peter","2 Petr"},{"1John","1 Joh"},
        {"2John","2 Joh"},{"3John","3 Joh"},{"Jude","Jud"},{"Rev","Upp"}};
    const auto it=names.find(book);
    return it==names.end()?book:it->second;
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
    resources_=resources;
    auto* outer=new wxBoxSizer(wxVERTICAL);
    auto* reader=new wxBoxSizer(wxHORIZONTAL);outer->Add(reader,1,wxEXPAND);
    scripture_=new ScriptureView(root_,corpus);reader->Add(scripture_,3,wxEXPAND);
    readings_=new wxScrolledWindow(root_,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL|wxBORDER_NONE);
    readings_->SetScrollRate(0,FromDIP(12));entries_=new wxBoxSizer(wxVERTICAL);readings_->SetSizer(entries_);
    outer->Add(readings_,1,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(68));
    // Every control lives in one bottom bar, shown in the reader and during
    // playback. Day navigation is in the Kalender menu.
    bar_=new wxPanel(root_);bar_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    bar_->Bind(wxEVT_PAINT,&MainFrame::paint_playback,this);
    auto* bar=new wxBoxSizer(wxHORIZONTAL);
    const auto add=[&](wxWindow* control,int proportion=0){bar->Add(control,proportion,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(8));};
    back_=button(bar_,"‹ Läsningar",[this]{show_readings();});add(back_);
    part_=new wxChoice(bar_,wxID_ANY);part_->SetName("Läsningens del");
    part_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){following_audio_=false;scripture_->follow_playback(false);scripture_->open_section(part_->GetSelection());refresh_speech();});
    add(part_);
    speech_status_=new wxStaticText(bar_,wxID_ANY,"",wxDefaultPosition,wxDefaultSize,wxST_ELLIPSIZE_END|wxST_NO_AUTORESIZE);
    speech_status_->SetFont(ui_font(11));speech_status_->SetMinSize(FromDIP(wxSize(40,-1)));
    bar->Add(speech_status_,1,wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT,FromDIP(8));
    follow_=button(bar_,"Följ uppläsningen",[this]{
        if(active_playback())follow_speech();
        else{scripture_->center_passage();scripture_->SetFocus();}
    });add(follow_);
    play_=button(bar_,"Lyssna",[this]{play_or_pause();});play_->SetToolTip("Lyssna, pausa eller fortsätt · mellanslag");add(play_);
    stop_=button(bar_,"Stoppa",[this]{stop_speech();});stop_->SetToolTip("Avsluta uppläsningen · Escape");
    bar->Add(stop_,0,wxALIGN_CENTER_VERTICAL);
    auto* bar_inset=new wxBoxSizer(wxVERTICAL);bar_inset->Add(bar,0,wxEXPAND|wxALL,FromDIP(10));
    bar_->SetSizer(bar_inset);outer->Add(bar_,0,wxEXPAND);
    make_menus();
    scripture_->on_release_follow([this]{following_audio_=false;refresh_speech();});
    scripture_->on_selection([this]{update_bar();});
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){refresh_speech();},playback_timer_.GetId());
    Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& event){
        if(active_playback()&&event.GetKeyCode()==WXK_ESCAPE){stop_speech();return;}
        if(event.GetKeyCode()==WXK_SPACE&&wxWindow::FindFocus()==scripture_){play_or_pause();return;}
        event.Skip();
    });
    // The worker copies the shared owner, not wxWeakRef's main-thread tracking data.
    auto weak=std::make_shared<wxWeakRef<MainFrame>>(this);
    auto latest_speech=std::make_shared<std::atomic<uint64_t>>(0);
    // ToUTF8 returns a scoped view. Keep its wxString owner alive until the path is copied.
    const auto data_dir=wxStandardPaths::Get().GetUserLocalDataDir();
    const auto data_utf8=data_dir.ToUTF8();
    const auto data_path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(data_utf8.data()),data_utf8.length()));
    pronunciation_review_=std::make_unique<PronunciationReviewDb>(data_path/"pronunciation-review.db");
    speech_=create_portable_speech(data_path,[weak,latest_speech](const SpeechUpdate& update){
        auto known=latest_speech->load();
        while(known<update.sequence&&!latest_speech->compare_exchange_weak(known,update.sequence)){}
        if(update.sequence<known)return;
        auto deliver=[weak,latest_speech,update]{if(*weak&&latest_speech->load()==update.sequence)(*weak)->speech_status(update.text);};
        // Delivery must run after the engine releases its mutex, including
        // Pause and Stop callbacks made on the main thread.
        wxTheApp->CallAfter(deliver);
    });
    speech_->set_speed(settings_.speech_rate/100.0);speech_->set_voice(settings_.speech_voice);
    try{study_db_=std::make_unique<StudyDb>(resources_/"lexicon/study.db");}catch(const std::exception&){}
    study_=new StudyPanel(root_,corpus_,study_db_.get(),*speech_,[this](const std::string& language){return speech_lexicon(language);});
    reader->Add(study_,2,wxEXPAND);
    scripture_->on_word([this](const ScriptureView::Word& word){
        if(!settings_.word_study)return;
        scripture_->highlight_word(word);study_->show(word,scripture_->base_source(),scripture_->frame());
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
    entries_->Clear(true);entries_->AddSpacer(FromDIP(48));
    entries_->Add(label(readings_,u(date_swedish(selected_.date())),13),0,wxBOTTOM,FromDIP(8));
    // The annotation reads "Pascha … · dag N · title"; the separator is four UTF-8 bytes.
    const std::string separator=" · ";
    const auto info=day_.day.annotation.find(separator,day_.day.annotation.find(separator)+separator.size());
    if(info!=std::string::npos) {
        auto* feast=new wxStaticText(readings_,wxID_ANY,u(day_.day.annotation.substr(info+separator.size())));feast->SetFont(body_font(18));
        feast->Wrap(std::max(200,GetClientSize().x-FromDIP(150)));entries_->Add(feast,0,wxBOTTOM,FromDIP(8));
    }
    entries_->AddSpacer(FromDIP(28));
    if(day_.readings.empty())entries_->Add(label(readings_,"Ingen daglig bibelläsning är föreskriven",18),0,wxBOTTOM,FromDIP(20));
    for(const auto& reading:day_.readings) {
        auto* section=label(readings_,kind_label(reading.kind),10);entries_->Add(section,0,wxBOTTOM,FromDIP(6));
        auto* open=button(readings_,u(label_of(corpus_.localize(in_primary(reading)).segments())),[this,reading]{open_reading(reading);});open->SetFont(body_font(20));
        entries_->Add(open,0,wxBOTTOM,FromDIP(32));
    }
    readings_->FitInside();readings_->Scroll(0,0);apply_settings(false);Layout();
}
void MainFrame::show_readings() {
    following_audio_=false;if(scripture_)scripture_->follow_playback(false);
    scripture_->Hide();readings_->Show();update_study();update_bar();
}
void MainFrame::open_psalm() { open_reading({ReadingKind::MorningPsalm,{"Ps",{23,1},{23,6}},"Psalm 23"}); }
void MainFrame::open_reading(const Reading& selected) {
    visible_reading_=selected;following_audio_=false;speech_view_.reset();scripture_->follow_playback(false);
    // Lectionary references use their reference edition's numbering; open them in the left pane's.
    const auto reading=corpus_.localize(in_primary(selected));
    readings_->Hide();scripture_->Show();update_study();
    reading_title_=u(label_of(reading.segments()));
    part_->Clear();const auto segments=reading.segments();
    for(std::size_t i=0;i<segments.size();++i)part_->Append(wxString::Format("Del %d · ",int(i+1))+u(label_of({segments[i]})));
    part_->SetSelection(0);
    update_bar();scripture_->open(reading);scripture_->SetFocus();
}
Passage MainFrame::shown(Passage passage) const {
    // A framing passage in the book and chapter numbers the reader shows.
    if(const auto* canon=canon_book(passage.book,passage.first.chapter)) {
        passage.book=canon->code;passage.first.chapter-=canon->offset();passage.last.chapter-=canon->offset();
    }
    return passage;
}
std::string MainFrame::label_of(const std::vector<Passage>& passages) const {
    std::string label,book;
    for(const auto& framing:passages) {
        const auto p=shown(framing);
        if(!label.empty())label+="; ";
        if(p.book!=book){label+=corpus_.book_name(p.book)+" ";book=p.book;}
        label+=std::to_string(p.first.chapter)+":"+std::to_string(p.first.verse)+p.first.suffix;
        if(p.last!=p.first)label+="–"+(p.last.chapter!=p.first.chapter?std::to_string(p.last.chapter)+":":"")+std::to_string(p.last.verse)+p.last.suffix;
    }
    return label;
}
Reading MainFrame::in_primary(Reading reading) const {
    if(reading.source_override.empty())reading.base_language=settings_.primary;
    return reading;
}
bool MainFrame::active_playback() const {
    const auto state=playback_ui_.state;
    return state==SpeechState::Playing||state==SpeechState::Paused||state==SpeechState::Buffering||state==SpeechState::Loading;
}
void MainFrame::play_or_pause() {
    if(active_playback())toggle_pause();
    else if(const auto marked=scripture_->IsShown()?scripture_->selection():std::nullopt) {
        Reading reading{new_testament_book(marked->book)?ReadingKind::Gospel:ReadingKind::OldTestament,*marked,label_of({*marked}),{},settings_.primary};
        reading.reference=scripture_->frame();
        play_speech({reading});
    }
    else if(scripture_->IsShown())play_speech({visible_reading_.value_or(scripture_->reading())});
}
void MainFrame::update_bar() {
    const bool reader=scripture_->IsShown(),active=active_playback();
    const wxString follow_label=active?"Följ uppläsningen":"Till läsningen";
    const bool marked=reader&&scripture_->selection();
    const wxString play_label=!active?(marked?"Läs markering":"Lyssna"):paused_?"Fortsätt":"Pausa";
    if(auto* bar=GetMenuBar()) {
        bar->SetLabel(play_item_,play_label+"\tCtrl+P");bar->Enable(play_item_,reader||active);
        bar->Enable(stop_item_,active);
    }
    bool changed=follow_->GetLabel()!=follow_label||play_->GetLabel()!=play_label;
    follow_->SetLabel(follow_label);play_->SetLabel(play_label);
    const std::pair<wxWindow*,bool> visibility[]={
        {bar_,reader||active},{back_,reader},
        {part_,reader&&part_->GetCount()>1},{follow_,active?!following_audio_:reader},{play_,reader||active},{stop_,active}};
    for(const auto& [control,shown]:visibility)if(control->IsShown()!=shown){control->Show(shown);changed=true;}
    if(changed){bar_->Layout();root_->Layout();}
    wxString location;
    if(playback_ui_.cue) {
        const auto& cue=*playback_ui_.cue;
        if(cue.introduction)location="Introduktion";
        else location=u(label_of({{cue.book,cue.verse,cue.verse}}));
    }
    // One short line: where the reading is, or what it is waiting for.
    const wxString ready=playback_ui_.ready>0?wxString::Format(" %d %%",int(playback_ui_.ready*100)):wxString{};
    const wxString idle=scripture_->IsShown()?reading_title_:wxString{};
    wxString title,tooltip;
    switch(feedback_state_) {
    case SpeechState::Loading:title="Laddar rösten…";break;
    case SpeechState::Buffering:title=(playback_ui_.cue?"Förbereder fortsättningen…":"Förbereder uppläsningen…")+ready;
        tooltip="Uppläsningen startar när tillräckligt mycket ljud är klart för att den inte ska stanna.";break;
    case SpeechState::Playing:title=location;break;
    case SpeechState::Paused:title=location.empty()?"Pausad":"Pausad · "+location;break;
    case SpeechState::Error:title="Uppläsningen kunde inte fortsätta";tooltip=u(speech_message_);break;
    case SpeechState::Stopped:case SpeechState::Completed:case SpeechState::Idle:title=idle;break;
    }
    if(speech_status_->GetLabel()!=title)speech_status_->SetLabel(title);
    speech_status_->SetToolTip(tooltip.empty()?title:tooltip);
}
void MainFrame::make_menus() {
    enum {Today=wxID_HIGHEST+1,Previous,Next,Pick,New,Old,Readings,Bible,System,Light,Dark,Left,Right=Left+3,Larger=Right+4,Smaller,Play,Stop,Rate,Review=Rate+8,Alice,Bjorn,Study};
    static const char* languages[]={"sv","el","en"};
    static const char* names[]={"Svenska","Grekiska","Engelska"};
    static const int rates[]={25,50,75,100,125,150,175,200};
    // Podcast style: 0,25×, 0,5× … 2×, with a Swedish decimal comma.
    const auto rate_label=[](int rate) {
        if(rate==100)return wxString("Normal hastighet");
        wxString text=wxString::Format("%d,%02d",rate/100,rate%100);
        while(text.EndsWith("0"))text.RemoveLast();
        if(text.EndsWith(","))text.RemoveLast();
        return text+u("×");
    };
    auto* calendar=new wxMenu;
    calendar->Append(Today,"Idag\tCtrl+T");
    calendar->Append(Previous,u("Föregående dag\tCtrl+["));calendar->Append(Next,u("Nästa dag\tCtrl+]"));
    calendar->Append(Pick,u("Välj datum…\tCtrl+D"));calendar->AppendSeparator();
    calendar->AppendRadioItem(New,"Nya kalendern");calendar->AppendRadioItem(Old,"Gamla kalendern");
    calendar->Check(settings_.calendar==CalendarStyle::Old?Old:New,true);
    auto* bible=new wxMenu;
    bible->Append(Readings,"Dagens läsningar\tCtrl+L");bible->Append(Bible,u("Gå till bibelställe…\tCtrl+G"));
    // Two panes: the left one is the text that is read aloud; the right one is optional.
    auto* view=new wxMenu;auto* left=new wxMenu;auto* right=new wxMenu;
    right->AppendRadioItem(Right,"Ingen");right->AppendRadioItem(Study,"Ordstudium");
    for(int i=0;i<3;++i) {
        left->AppendRadioItem(Left+i,names[i]);if(settings_.primary==languages[i])left->Check(Left+i,true);
        right->AppendRadioItem(Right+1+i,names[i]);if(settings_.parallel==languages[i])right->Check(Right+1+i,true);
        right->Enable(Right+1+i,settings_.primary!=languages[i]);
    }
    if(settings_.word_study)right->Check(Study,true);
    else if(settings_.parallel.empty())right->Check(Right,true);
    view->AppendSubMenu(left,u("Vänster spalt"));view->AppendSubMenu(right,u("Höger spalt"));view->AppendSeparator();
    view->AppendRadioItem(System,"Systemets tema");view->AppendRadioItem(Light,"Ljust tema");view->AppendRadioItem(Dark,u("Mörkt tema"));
    view->Check(System+int(settings_.theme),true);view->AppendSeparator();
    view->Append(Larger,u("Större text\tCtrl++"));view->Append(Smaller,"Mindre text\tCtrl+-");
    auto* reading=new wxMenu;
    reading->Append(Play,"Lyssna\tCtrl+P");reading->Append(Stop,"Stoppa\tCtrl+.");reading->AppendSeparator();
    play_item_=Play;stop_item_=Stop;
    for(int i=0;i<8;++i) {
        reading->AppendRadioItem(Rate+i,rate_label(rates[i]));
        if(settings_.speech_rate==rates[i])reading->Check(Rate+i,true);
    }
    reading->AppendSeparator();
    // Swedish voice. Greek and English are read by Chatterbox.
    reading->AppendRadioItem(Alice,"Alice");reading->AppendRadioItem(Bjorn,u("Björn"));
    reading->Check(settings_.speech_voice=="bjorn"?Bjorn:Alice,true);
    reading->AppendSeparator();reading->Append(Review,u("Granska svenskt uttal…"));
    auto* bar=new wxMenuBar;bar->Append(calendar,"Kalender");bar->Append(bible,"Bibel");bar->Append(view,"Visa");bar->Append(reading,u("Uppläsning"));SetMenuBar(bar);
    Bind(wxEVT_MENU,[this,right](wxCommandEvent& event) {
        const int id=event.GetId();
        if(id==Today){selected_.select(local_civil_date());show_readings();refresh_day();return;}
        if(id==Previous||id==Next){navigate(id==Next?1:-1);return;}
        if(id==Pick){pick_date();return;}
        if(id==Readings){show_readings();return;}
        if(id==Bible){browse_bible();return;}
        if(id==Play){play_or_pause();return;}
        if(id==Stop){stop_speech();return;}
        if(id==Review){review_pronunciation();return;}
        if(id==New||id==Old) {
            settings_.calendar=id==Old?CalendarStyle::Old:CalendarStyle::New;
            if(!scripture_->IsShown())refresh_day();
        }
        else if(id>=System&&id<=Dark)settings_.theme=static_cast<Theme>(id-System);
        else if(id>=Left&&id<Left+3) {
            settings_.primary=languages[id-Left];
            if(settings_.parallel==settings_.primary){settings_.parallel.clear();right->Check(Right,true);}
            for(int i=0;i<3;++i)right->Enable(Right+1+i,settings_.primary!=languages[i]);
            apply_settings();
            // The left pane decides the edition and its numbering, so reopen the text.
            if(scripture_->IsShown()&&visible_reading_)open_reading(*visible_reading_);
            return;
        }
        else if(id==Right){settings_.parallel.clear();settings_.word_study=false;}
        else if(id==Study){settings_.parallel.clear();settings_.word_study=true;}
        else if(id>Right&&id<=Right+3){settings_.parallel=languages[id-Right-1];settings_.word_study=false;}
        else if(id==Larger)settings_.font_size=std::min(28,settings_.font_size+1);
        else if(id==Smaller)settings_.font_size=std::max(14,settings_.font_size-1);
        else if(id>=Rate&&id<Rate+8){settings_.speech_rate=rates[id-Rate];speech_->set_speed(settings_.speech_rate/100.0);}
        else if(id==Alice||id==Bjorn){settings_.speech_voice=id==Bjorn?"bjorn":"alice";speech_->set_voice(settings_.speech_voice);}
        else{event.Skip();return;}
        apply_settings();
    },Today,Study);
}
std::vector<Pronunciation> MainFrame::speech_lexicon(const std::string& language) const {
    auto result=corpus_.pronunciations(language);
    if(language=="sv"&&pronunciation_review_) {
        const auto overrides=pronunciation_review_->overrides();result.insert(result.begin(),overrides.begin(),overrides.end());
    }
    return result;
}
void MainFrame::review_pronunciation() {
    stop_speech();
    try {show_pronunciation_review(this,corpus_,*pronunciation_review_,*speech_,resources_);}
    catch(const std::exception& e){wxMessageBox(u(e.what()),u("Uttalsgranskning"),wxOK|wxICON_ERROR,this);}
    speech_->set_speed(settings_.speech_rate/100.0);refresh_speech();
}
void MainFrame::browse_bible() {
    // Three grids, as in most Bible apps: book, then chapter, then verse.
    // Books follow the OSB order; the Old Testament uses Septuagint numbers.
    wxDialog dialog(this,wxID_ANY,u("Gå till bibelställe"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    auto* sizer=new wxBoxSizer(wxVERTICAL);
    auto* header=new wxBoxSizer(wxHORIZONTAL);
    std::function<void()> go_back;
    // Each step replaces go_back, so call a copy.
    auto* back=button(&dialog,u("‹ Tillbaka"),[&]{if(auto action=go_back)action();});
    auto* title=label(&dialog,"",13);
    header->Add(back,0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(12));header->Add(title,1,wxALIGN_CENTER_VERTICAL);
    sizer->Add(header,0,wxEXPAND|wxALL,FromDIP(16));
    auto* grid=new wxPanel(&dialog);sizer->Add(grid,1,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,FromDIP(16));
    dialog.SetSizer(sizer);
    std::optional<Reading> chosen;
    std::function<void()> show_books;std::function<void(CanonBook)> show_chapters;
    const auto frame=[this](const CanonBook& book){return frame_source(settings_.primary,book.frame_book);};
    // Framing verses of a book, within its chapters of the framing book.
    const auto verses_of=[&](const CanonBook& book) {
        std::vector<VerseRef> result;
        for(const auto& ref:corpus_.coordinates(frame(book),book.frame_book))
            if(book.first_chapter<=ref.chapter&&ref.chapter<=book.last_chapter)result.push_back(ref);
        return result;
    };
    // A book is available when the left pane's edition has text for its opening verses;
    // the first verses alone can be missing, as Sirach's prologue is in Swedish.
    const auto available=[&](const CanonBook& book) {
        const auto verses=verses_of(book);
        const auto source=source_for_language(settings_.primary,book.code);
        return std::any_of(verses.begin(),verses.begin()+std::min<size_t>(verses.size(),50),[&](VerseRef ref){
            return bool(corpus_.parallel_verse(frame(book),source,book.frame_book,ref));});
    };
    // Opens the chosen verses; Lyssna then reads all of them.
    const auto choose=[&](const CanonBook& book,VerseRef first,VerseRef last) {
        Reading reading{book.new_testament?ReadingKind::Gospel:ReadingKind::OldTestament,{book.frame_book,first,last},
            label_of({{book.frame_book,first,last}}),{},settings_.primary};
        reading.reference=frame(book);
        chosen=reading;dialog.EndModal(wxID_OK);
    };
    // `whole` is an optional button before the numbers, for the whole book or chapter.
    const auto cells=[&](const std::vector<std::pair<wxString,std::function<void()>>>& items,int columns,const std::vector<std::pair<size_t,wxString>>& groups,
                         const std::pair<wxString,std::function<void()>>& whole={}) {
        grid->DestroyChildren();auto* rows=new wxBoxSizer(wxVERTICAL);
        if(whole.second)rows->Add(button(grid,whole.first,[&dialog,action=whole.second]{dialog.CallAfter(action);}),0,wxBOTTOM,FromDIP(10));
        size_t group=0;wxGridSizer* table=nullptr;
        for(size_t i=0;i<items.size();++i) {
            if(!table||(group<groups.size()&&groups[group].first==i)) {
                if(group<groups.size()&&groups[group].first==i){rows->Add(label(grid,groups[group].second,9),0,wxTOP|wxBOTTOM,FromDIP(8));++group;}
                table=new wxGridSizer(columns,FromDIP(4),FromDIP(4));rows->Add(table,0,wxEXPAND);
            }
            // The grid is rebuilt by a click, so the clicked button must outlive its handler.
            auto* cell=button(grid,items[i].first,[&dialog,action=items[i].second]{dialog.CallAfter(action);});
            cell->SetMinSize(FromDIP(wxSize(58,30)));
            table->Add(cell,0,wxEXPAND);
        }
        grid->SetSizer(rows);recolor(&dialog,palette(settings_.theme));
        dialog.Layout();dialog.Fit();
        // Keep the width of the book grid, so the dialog does not jump between steps.
        if(dialog.GetMinSize().x<0)dialog.SetMinSize(wxSize(dialog.GetSize().x,-1));
    };
    show_books=[&] {
        back->Hide();go_back={};title->SetLabel("Välj bok");
        std::vector<std::pair<wxString,std::function<void()>>> items;std::vector<std::pair<size_t,wxString>> groups;
        std::vector<std::pair<wxString,const CanonBook*>> cells_books;
        for(int part=0;part<2;++part) {
            groups.emplace_back(items.size(),part==0?"GAMLA TESTAMENTET":"NYA TESTAMENTET");
            for(const auto& book:osb_canon())if(book.new_testament==bool(part)) {
                items.emplace_back(u(book_abbreviation(book.code)),[&,book]{show_chapters(book);});
                cells_books.emplace_back(items.back().first,&book);
            }
        }
        cells(items,8,groups);
        // Not yet available in the left pane's language: shown, but greyed out.
        for(auto* child:grid->GetChildren())if(auto* cell=wxDynamicCast(child,wxButton))
            for(const auto& [name,book]:cells_books)if(cell->GetLabel()==name) {
                cell->SetToolTip(u(corpus_.book_name(book->code)));
                if(!available(*book)){cell->Disable();cell->SetToolTip(u(corpus_.book_name(book->code)+" · saknas ännu på detta språk"));}
            }
    };
    show_chapters=[&](CanonBook book) {
        back->Show();go_back=show_books;title->SetLabel(u(corpus_.book_name(book.code)));
        const auto verses=verses_of(book);
        std::vector<int> chapters;for(const auto& ref:verses)if(chapters.empty()||chapters.back()!=ref.chapter)chapters.push_back(ref.chapter);
        std::vector<std::pair<wxString,std::function<void()>>> items;
        for(int chapter:chapters)items.emplace_back(wxString::Format("%d",chapter-book.offset()),[&,book,chapter,verses] {
            title->SetLabel(u(corpus_.book_name(book.code))+wxString::Format(" %d",chapter-book.offset()));
            go_back=[&,book]{show_chapters(book);};
            std::vector<std::pair<wxString,std::function<void()>>> numbers;
            std::vector<VerseRef> in_chapter;
            for(const auto& ref:verses)if(ref.chapter==chapter) {
                in_chapter.push_back(ref);
                numbers.emplace_back(wxString::Format("%d",ref.verse)+u(ref.suffix),[&,book,ref]{choose(book,ref,ref);});
            }
            cells(numbers,10,{},{u("Hela kapitlet"),[&,book,in_chapter]{choose(book,in_chapter.front(),in_chapter.back());}});
        });
        cells(items,10,{},{u("Hela boken"),[&,book,verses]{choose(book,verses.front(),verses.back());}});
    };
    show_books();
    dialog.Centre();
    if(dialog.ShowModal()==wxID_OK&&chosen)open_reading(*chosen);
}
void MainFrame::pick_date() {
    const auto current=selected_.date();
    wxDialog dialog(this,wxID_ANY,"Välj civilt datum",wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    auto* sizer=new wxBoxSizer(wxVERTICAL);
    auto* calendar=new wxCalendarCtrl(&dialog,wxID_ANY,
        wxDateTime(unsigned(current.day()),static_cast<wxDateTime::Month>(unsigned(current.month())-1),int(current.year())),
        wxDefaultPosition,wxDefaultSize,wxCAL_SUNDAY_FIRST);
    // Weeks start on Sunday, as in the parish calendar; Sundays are red.
    const auto mark_sundays=[calendar] {
        const auto shown=calendar->GetDate();
        for(unsigned day=1;day<=wxDateTime::GetNumberOfDays(shown.GetMonth(),shown.GetYear());++day) {
            if(wxDateTime(day,shown.GetMonth(),shown.GetYear()).GetWeekDay()==wxDateTime::Sun)calendar->SetAttr(day,new wxCalendarDateAttr(*wxRED));
            else calendar->ResetAttr(day);
        }
        calendar->Refresh();
    };
    calendar->Bind(wxEVT_CALENDAR_PAGE_CHANGED,[mark_sundays](wxCalendarEvent& event){mark_sundays();event.Skip();});
    mark_sundays();
    sizer->Add(calendar,0,wxALL,FromDIP(20));

    sizer->Add(button(&dialog,"Aktuellt datum",[calendar]{calendar->SetDate(wxDateTime::Today());}),0,wxALIGN_CENTER|wxBOTTOM,FromDIP(12));
    auto* buttons=dialog.CreateButtonSizer(wxOK|wxCANCEL);
    if(auto* cancel=wxDynamicCast(dialog.FindWindow(wxID_CANCEL),wxButton))cancel->SetLabel("Avbryt");
    sizer->Add(buttons,0,wxALIGN_RIGHT|wxALL,FromDIP(12));
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
    recolor(root_,palette(settings_.theme));
    scripture_->apply(settings_);study_->apply(settings_.theme);update_study();
}
void MainFrame::update_study() {
    const bool shown=settings_.word_study&&scripture_->IsShown();
    if(!shown)scripture_->highlight_word(std::nullopt);
    study_->Show(shown);root_->Layout();
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
    // Freeze tracking immediately, but do not flash buffering text for a
    // brief gap between audio callbacks. Startup feedback remains immediate.
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
    paused_=state==SpeechState::Paused;
    update_bar();bar_->Refresh(false);
}
void MainFrame::paint_playback(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(bar_);const auto colors=palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));dc.Clear();
    const auto size=bar_->GetClientSize();
    dc.SetPen(wxPen(colors.rule,1));dc.DrawLine(0,0,size.x,0);
    // The rule doubles as the progress line while a reading plays.
    if(active_playback()&&playback_ui_.progress>0) {
        dc.SetPen(*wxTRANSPARENT_PEN);dc.SetBrush(wxBrush(colors.accent));
        dc.DrawRectangle(0,0,int(size.x*playback_ui_.progress),FromDIP(2));
    }
}
void MainFrame::play_speech(const std::vector<Reading>& readings) {
    stop_speech();speech_readings_=readings;speech_view_.reset();
    try {
        std::vector<SpeechUtterance> queue;
        for(size_t r=0;r<readings.size();++r) {
            const auto reading=in_primary(readings[r]);const auto localized=corpus_.localize(reading);const auto passages=localized.segments();
            if(reading.base_language=="sv") {
                // Announced in the numbers shown on screen.
                const Reading announced{reading.kind,shown(passages.front()),label_of({passages.front()})};
                auto intro=make_utterance(reading_introduction(announced),"sv",speech_lexicon("sv"));
                intro.cue=SpeechCue{r,0,passages.front().book,"",passages.front().first,passages.front().last,true};queue.push_back(std::move(intro));
            }
            for(size_t s=0;s<passages.size();++s) {
                const auto& passage=passages[s];
                // Walk the framing verses and read the left pane's text for each,
                // so Hebrew-only verses are not read and merged verses are read once.
                auto frame=reading.source_override.empty()?frame_source(reading.base_language,passage.book):reading.source_override;
                const auto* canon=canon_book(passage.book,passage.first.chapter);
                auto source=reading.source_override.empty()?source_for_language(reading.base_language,canon?canon->code:passage.book):reading.source_override;
                if(corpus_.coordinates(frame,passage.book).empty())frame=source;
                const auto language=source.starts_with("en-")?"en":source.starts_with("grc-")?"el":"sv";
                const auto lexicon=speech_lexicon(language);
                bool found=false;std::optional<VerseRef> previous;
                for(auto ref:corpus_.coordinates(frame,passage.book)) {
                    if(ref>passage.last)break;
                    if(ref<passage.first)continue;
                    auto verse=corpus_.parallel_verse(frame,source,passage.book,ref);
                    const bool continues=(verse&&previous&&verse->ref==*previous)||(!verse&&verse.error()=="Ingår i föregående vers");
                    if(continues&&found){queue.back().cue->last=ref;continue;}
                    if(!verse)continue;
                    previous=verse->ref;
                    auto utterance=make_utterance(verse->text,language,lexicon);
                    utterance.cue=SpeechCue{r,s,passage.book,frame,ref,ref,false};
                    queue.push_back(std::move(utterance));found=true;
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
    for(auto theme:{Theme::Light,Theme::Dark,Theme::System})for(auto mode:{"","el","en"}) {
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
    settings_.parallel="en";settings_.theme=Theme::Dark;apply_settings(false);scripture_->center_passage();
    const auto narrow_size=scripture_->GetClientSize();
    if(narrow_size.x<100||narrow_size.y<100)ok=false;
    else {
        wxBitmap bitmap(narrow_size.x,narrow_size.y);wxMemoryDC dc(bitmap);scripture_->render_to(dc,narrow_size);dc.SelectObject(wxNullBitmap);
        if(!screenshot_path.empty())ok=bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.')+"-narrow.png",wxBITMAP_TYPE_PNG)&&ok;
    }
    SetSize(original_size);Layout();
    // The day page is navigation only; it shows no bottom bar.
    show_readings();
    for(auto* child:readings_->GetChildren())if(auto* control=dynamic_cast<wxButton*>(child);control&&control->GetLabel().Contains("Lyssna"))ok=false;
    if(bar_->IsShown()||play_->IsShown()||back_->IsShown())ok=false;
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
    // Marked verses replace the reading as what Lyssna reads.
    open_psalm();scripture_->select_verses({23,4},{23,2});
    const auto marked=scripture_->selection();
    if(!marked||marked->first!=VerseRef{23,2}||marked->last!=VerseRef{23,4}||play_->GetLabel()!="Läs markering")ok=false;
    scripture_->clear_selection();if(play_->GetLabel()!="Lyssna")ok=false;
    // The left pane's language chooses the edition and its numbering: LXX Psalm 22 is Psalm 23.
    settings_.primary="el";apply_settings(false);open_psalm();
    if(scripture_->base_source()!="grc-lxx"||scripture_->reading().passage.first.chapter!=22)ok=false;
    settings_.primary="sv";apply_settings(false);
    // Ordstudium replaces the right pane; a word shows Dalin and the verse's Strong's entries.
    settings_.word_study=true;apply_settings(false);
    open_reading({ReadingKind::Gospel,{"John",{1,1},{1,5}},"Johannesevangeliet 1:1–5"});
    if(!study_->IsShown()||scripture_->base_source()!="sv1917")ok=false;
    study_->show({"John","begynnelsen",{1,1},0},scripture_->base_source(),scripture_->frame());
    {
        const auto lines=study_->text();
        const auto has=[&](const wxString& part){return std::any_of(lines.begin(),lines.end(),[&](const wxString& line){return line.Contains(part);});};
        if(!has("DALIN")||!has("begynnelse")||!has("G746"))ok=false;
    }
    settings_.word_study=false;apply_settings(false);
    if(study_->IsShown())ok=false;
    // Exercise playback presentation without model loading or audible output.
    open_psalm();settings_.theme=Theme::Light;settings_.parallel="el";apply_settings(false);
    display_playback({SpeechState::Loading,{},0,0});
    if(speech_status_->GetLabel()!="Laddar rösten…"||play_->GetLabel()!="Pausa"||!stop_->IsShown())ok=false;
    display_playback({SpeechState::Buffering,{},0,0,0.4});
    if(speech_status_->GetLabel()!="Förbereder uppläsningen… 40 %"||scripture_->marker_position())ok=false;
    SpeechPlayback playing{SpeechState::Playing,SpeechCue{0,0,"Ps","sv1917",{23,3},{23,3},false},0.35,0.4};
    following_audio_=true;scripture_->playback(playing);scripture_->follow_playback();
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position()||!scripture_->follows_playback())ok=false;
    const auto held_marker=scripture_->marker_position();const auto held_scroll=scripture_->scroll_position();
    auto paused=playing;paused.state=SpeechState::Paused;display_playback(paused);
    for(int i=0;i<30;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()!=held_marker||scripture_->scroll_position()!=held_scroll||play_->GetLabel()!="Fortsätt"||!stop_->IsShown())ok=false;
    auto buffering=playing;buffering.state=SpeechState::Buffering;display_playback(buffering);
    for(int i=0;i<30;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()!=held_marker||scripture_->scroll_position()!=held_scroll||speech_status_->GetLabel()!="Förbereder fortsättningen…")ok=false;
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
    for(wxWindow* control:{back_,follow_,play_,stop_})if(control->IsShown()&&control->GetRect().GetRight()>bar_->GetClientSize().x)ok=false;
    save_playback("-playing-narrow");
    SetSize(original_size);Layout();root_->Layout();settings_.font_size=original.font_size;apply_settings(false);
    scripture_->playback(playing);scripture_->follow_playback();
    auto distant=playing;distant.cue->verse=distant.cue->last={24,10};scripture_->playback(distant);
    for(int i=0;i<160;++i)scripture_->advance_playback(0.016);
    if(!scripture_->marker_position()||scripture_->scroll_position()<=held_scroll+100)ok=false;
    display_playback({SpeechState::Stopped,{},0,0});
    for(int i=0;i<90;++i)scripture_->advance_playback(0.016);
    if(scripture_->marker_position()||play_->GetLabel()!="Lyssna"||stop_->IsShown())ok=false;
    std::cout<<"Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_psalm();
    settings_=original;apply_settings(false);ok=ok && selected_.date()==date;
    return ok;
}
}
