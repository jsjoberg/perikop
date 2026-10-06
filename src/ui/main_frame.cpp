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
#include <algorithm>
#include <functional>
#include <cmath>
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
    case ReadingKind::Epistle:return "APOSTEL";
    case ReadingKind::Gospel:return "EVANGELIUM";
    case ReadingKind::OldTestament:return "GAMLA TESTAMENTET";
    case ReadingKind::Vespers:return "VESPER";
    case ReadingKind::EveningPsalm:return "KVÄLL";
    }
    return {};
}
}
MainFrame::MainFrame(const CorpusDb& corpus,UserDb& user,CivilDate date)
    :wxFrame(nullptr,wxID_ANY,"Ortodox läsare",wxDefaultPosition,wxSize(1120,900)),
    corpus_(corpus),user_(user),lectionary_(corpus),selected_(date),settings_(user.load()) {
    SetMinSize(FromDIP(wxSize(520,480)));root_=new wxPanel(this);root_->SetFont(ui_font());
    auto* outer=new wxBoxSizer(wxVERTICAL);
    auto* top=new wxBoxSizer(wxHORIZONTAL);
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
    reader_tools->Add(button(reader_header_,"Till läsningen",[this]{scripture_->center_passage();scripture_->SetFocus();}),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,FromDIP(10));
    reader_tools->Add(button(reader_header_,"Uttalsprov",[this]{preview_speech({scripture_->reading()});}),0,wxALIGN_CENTER_VERTICAL);
    reader_header_->SetSizer(reader_tools);outer->Add(reader_header_,0,wxEXPAND|wxALL,FromDIP(20));
    part_=new wxChoice(reader_header_,wxID_ANY);part_->SetName("Läsningens del");
    part_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){scripture_->open_section(part_->GetSelection());});
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
    const auto separator=day_.day.annotation.find(" · ",day_.day.annotation.find(" · ")+3);
    annotation_->SetLabel(u(separator==std::string::npos?day_.day.annotation:day_.day.annotation.substr(0,separator)));
    annotation_->SetToolTip(u(day_.day.annotation+"\nFast kalender: "+day_.day.fixed_cycle.value_or("")+"\n"+day_.day.paschal_cycle.value_or("")));
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
            line->Add(open,0,wxALIGN_CENTER_VERTICAL);line->Add(button(readings_,"Uttalsprov",[this,reading]{preview_speech({reading});}),0,wxALIGN_CENTER_VERTICAL|wxLEFT,FromDIP(16));
            entries_->Add(line,0,wxEXPAND|wxBOTTOM,FromDIP(36));
        }
        entries_->AddSpacer(FromDIP(10));
        listen_all_=button(readings_,"Lyssna på läsningarna · förhandsprov",[this]{preview_speech(day_.readings);});
        entries_->Add(listen_all_,0,wxALIGN_CENTER|wxBOTTOM,FromDIP(16));
        speech_status_=label(readings_,"Talgränssnittet är ett prov. Ingen ljudsyntes är inkopplad.",10);
        entries_->Add(speech_status_,0,wxALIGN_CENTER);
    }
    readings_->FitInside();readings_->Scroll(0,0);apply_settings(false);Layout();
}
void MainFrame::show_readings() {
    scripture_->Hide();reader_header_->Hide();readings_->Show();root_->Layout();
}
void MainFrame::open_psalm() { open_reading({ReadingKind::MorningPsalm,{"Ps",{23,1},{23,6}},"Psalm 23"}); }
void MainFrame::open_reading(const Reading& selected) {
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
    scripture_->apply(settings_);root_->Layout();
}
void MainFrame::preview_speech(const std::vector<Reading>& readings) {
    StubSpeechEngine engine;const auto lexicon=corpus_.pronunciations("sv");wxString preview;
    for(const auto& reading:readings) {
        std::string text=reading_introduction(reading)+"\n";
        for(const auto& passage:corpus_.localize(reading).segments()) {
            const auto refs=corpus_.coordinates("sv1917",passage.book);
            for(auto ref:refs)if(passage.contains(ref)) {
                auto verse=corpus_.verse("sv1917",passage.book,ref);
                if(verse)text+=verse->text+"\n";
            }
        }
        engine.speak(make_utterance(text,"sv",lexicon));
        preview+=u(reading.label)+"\n\n"+u(engine.accepted.back().speech_text)+"\n\n";
    }
    auto proof=make_utterance("Melkisedek","sv",lexicon);
    preview+="Uttalsexempel: "+u(proof.display_text)+" → "+u(proof.speech_text)+"\nBibeltexten i databasen ändras inte.";
    wxDialog dialog(this,wxID_ANY,"Talprov · ingen ljudsyntes",wxDefaultPosition,FromDIP(wxSize(700,560)),wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    auto* sizer=new wxBoxSizer(wxVERTICAL);
    sizer->Add(label(&dialog,"Detta är talrepresentationen som en framtida röst får.",11),0,wxALL,FromDIP(18));
    auto* text=new wxTextCtrl(&dialog,wxID_ANY,preview,wxDefaultPosition,wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY);
    text->SetFont(ui_font(12));sizer->Add(text,1,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(18));
    sizer->Add(dialog.CreateButtonSizer(wxOK),0,wxALIGN_RIGHT|wxALL,FromDIP(18));dialog.SetSizer(sizer);
    recolor(&dialog,palette(settings_.theme));dialog.ShowModal();
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
    open_psalm();
    settings_=original;apply_settings(false);ok=ok && selected_.date()==date;
    return ok;
}
}
