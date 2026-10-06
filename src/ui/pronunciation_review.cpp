#include "ui/pronunciation_review.hpp"
#include "speech/pronunciation_review.hpp"
#include "ui/theme.hpp"
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <map>
#include <set>
#include <functional>
namespace ortho {
namespace {
wxString u(const std::string& text){return wxString::FromUTF8(text);}
std::string utf8(const wxString& text){return text.ToStdString(wxConvUTF8);}
std::filesystem::path path(const wxString& text){const auto bytes=text.ToUTF8();return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(bytes.data()),bytes.length()));}
wxString status_label(const std::string& status) {
    return status=="approved"?u("Godkänt"):status=="corrected"?u("Korrigerat"):status=="deferred"?u("Senare"):u("Ej granskat");
}
}
void show_pronunciation_review(wxWindow* parent,const CorpusDb& corpus,PronunciationReviewDb& db,SpeechEngine& speech,const std::filesystem::path& resources) {
    const auto words=load_pronunciation_words(resources/"lexicon/sv1917-words.tsv");
    speech.stop();speech.set_speed(1);
    wxDialog dialog(parent,wxID_ANY,u("Uttalsgranskning · Svenska 1917"),wxDefaultPosition,wxSize(1080,760),wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    dialog.SetMinSize(wxSize(880,650));dialog.SetFont(ui_font(11));
    auto* outer=new wxBoxSizer(wxVERTICAL);
    auto* hint=new wxStaticText(&dialog,wxID_ANY,u("Lyssna på ordet och bibelversen. Skriv en uttalsstavning vid behov och provlyssna innan du sparar."));
    outer->Add(hint,0,wxEXPAND|wxALL,16);
    auto* filters=new wxBoxSizer(wxHORIZONTAL);
    auto* search=new wxTextCtrl(&dialog,wxID_ANY);search->SetHint(u("Sök ord…"));search->SetName(u("Sök ord"));filters->Add(search,1,wxRIGHT,10);
    auto* category=new wxChoice(&dialog,wxID_ANY,wxDefaultPosition,wxDefaultSize,{u("Namn utan NST"),u("Alla utan NST"),u("Alla ord")});category->SetSelection(0);category->SetName(u("Ordgrupp"));filters->Add(category,0,wxRIGHT,10);
    auto* pending=new wxCheckBox(&dialog,wxID_ANY,u("Endast ej granskade"));pending->SetValue(true);filters->Add(pending,0,wxALIGN_CENTER_VERTICAL);
    outer->Add(filters,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,16);
    auto* progress=new wxStaticText(&dialog,wxID_ANY,"");outer->Add(progress,0,wxLEFT|wxRIGHT|wxBOTTOM,16);
    auto* split=new wxBoxSizer(wxHORIZONTAL);
    auto* list=new wxListCtrl(&dialog,wxID_ANY,wxDefaultPosition,wxSize(350,-1),wxLC_REPORT|wxLC_SINGLE_SEL);list->SetName(u("Ord att granska"));
    list->InsertColumn(0,"Ord",wxLIST_FORMAT_LEFT,155);list->InsertColumn(1,"Antal",wxLIST_FORMAT_RIGHT,55);list->InsertColumn(2,"Status",wxLIST_FORMAT_LEFT,110);
    split->Add(list,0,wxEXPAND|wxRIGHT,20);
    auto* detail=new wxBoxSizer(wxVERTICAL);
    auto* title=new wxStaticText(&dialog,wxID_ANY,"");title->SetFont(body_font(24));detail->Add(title,0,wxBOTTOM,10);
    auto* nst=new wxTextCtrl(&dialog,wxID_ANY,"",wxDefaultPosition,wxDefaultSize,wxTE_READONLY);nst->SetName("NST SAMPA");detail->Add(nst,0,wxEXPAND|wxBOTTOM,8);
    auto* classification=new wxStaticText(&dialog,wxID_ANY,u("Namn är möjliga namn, identifierade genom versaler. NST SAMPA är ett referensuttal."));classification->Wrap(550);detail->Add(classification,0,wxEXPAND|wxBOTTOM,10);
    auto* contexts=new wxChoice(&dialog,wxID_ANY);contexts->SetName(u("Bibelvers"));detail->Add(contexts,0,wxEXPAND|wxBOTTOM,8);
    auto* verse=new wxTextCtrl(&dialog,wxID_ANY,"",wxDefaultPosition,wxSize(-1,145),wxTE_MULTILINE|wxTE_READONLY);verse->SetName(u("Ordet i sitt sammanhang"));verse->SetFont(body_font(16));detail->Add(verse,1,wxEXPAND|wxBOTTOM,10);
    detail->Add(new wxStaticText(&dialog,wxID_ANY,u("Uttalsstavning för rösten")),0,wxBOTTOM,4);
    auto* spoken=new wxTextCtrl(&dialog,wxID_ANY);spoken->SetName(u("Uttalsstavning"));detail->Add(spoken,0,wxEXPAND|wxBOTTOM,8);
    auto* current=new wxStaticText(&dialog,wxID_ANY,"");detail->Add(current,0,wxEXPAND|wxBOTTOM,8);
    auto* note=new wxTextCtrl(&dialog,wxID_ANY);note->SetHint(u("Anteckning (valfri)"));note->SetName(u("Anteckning"));detail->Add(note,0,wxEXPAND|wxBOTTOM,10);
    auto* audio=new wxBoxSizer(wxHORIZONTAL);
    auto add_button=[&](wxBoxSizer* row,const wxString& label,const std::function<void()>& action){auto* control=new wxButton(&dialog,wxID_ANY,label);control->Bind(wxEVT_BUTTON,[action](wxCommandEvent&){action();});row->Add(control,0,wxRIGHT,6);return control;};
    std::map<std::string,PronunciationDecision> decisions;
    auto reload=[&]{decisions.clear();for(const auto& value:db.decisions())decisions[pronunciation_key(value.form)]=value;};reload();
    std::vector<size_t> visible;std::vector<WordExample> examples;std::optional<size_t> selected;
    bool rebuilding=false;
    std::function<void(size_t)> select;
    std::function<void(const std::string&)> rebuild;
    auto lexicon=[&]{auto result=corpus.pronunciations("sv");auto overrides=db.overrides();result.insert(result.begin(),overrides.begin(),overrides.end());return result;};
    auto play=[&](bool proposed,bool in_context){
        if(!selected)return;
        auto text=in_context?utf8(verse->GetValue()):words[*selected].form;
        if(text.empty())return;
        auto entries=lexicon();
        if(proposed){auto value=spoken->GetValue();value.Trim(true).Trim(false);if(value.empty()){wxMessageBox(u("Skriv en uttalsstavning först."),u("Uttalsgranskning"),wxOK,&dialog);return;}entries.insert(entries.begin(),{"sv",words[*selected].form,utf8(value),"",2000});}
        speech.stop();speech.speak(make_utterance(text,"sv",entries));
    };
    auto* original_word=add_button(audio,u("Ord · nu"),[&]{play(false,false);});
    auto* original_context=add_button(audio,u("Vers · nu"),[&]{play(false,true);});
    auto* proposed_word=add_button(audio,u("Ord · förslag"),[&]{play(true,false);});
    auto* proposed_context=add_button(audio,u("Vers · förslag"),[&]{play(true,true);});
    add_button(audio,"Stoppa",[&]{speech.stop();});detail->Add(audio,0,wxBOTTOM,10);
    auto* feedback=new wxStaticText(&dialog,wxID_ANY,"");detail->Add(feedback,0,wxEXPAND|wxBOTTOM,10);
    auto* actions=new wxBoxSizer(wxHORIZONTAL);
    auto save=[&](const std::string& state){
        if(!selected)return;
        auto value=spoken->GetValue();value.Trim(true).Trim(false);
        if(state=="corrected"&&value.empty()){wxMessageBox(u("Skriv en uttalsstavning först."),u("Uttalsgranskning"),wxOK,&dialog);return;}
        const long row=list->GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
        try {
            // Approve the actual active spelling, including a bundled override.
            const auto active=make_utterance(words[*selected].form,"sv",lexicon()).speech_text;
            const auto status=state=="approved"&&pronunciation_key(active)!=pronunciation_key(words[*selected].form)?"corrected":state;
            db.save({words[*selected].form,status,state=="corrected"||state=="deferred"?utf8(value):state=="approved"&&status=="corrected"?active:"",utf8(note->GetValue())});
            speech.stop();reload();rebuild("");
            if(!visible.empty())select(std::min<size_t>(std::max(0L,row),visible.size()-1));
        }catch(const std::exception& e){wxMessageBox(u(e.what()),u("Kunde inte spara"),wxOK|wxICON_ERROR,&dialog);}
    };
    auto* approve=add_button(actions,u("Godkänn nuvarande"),[&]{save("approved");});
    auto* correct=add_button(actions,u("Spara korrigering"),[&]{save("corrected");});
    auto* defer=add_button(actions,u("Granska senare"),[&]{save("deferred");});detail->Add(actions,0,wxBOTTOM,10);
    split->Add(detail,1,wxEXPAND);outer->Add(split,1,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,16);
    auto* bottom=new wxBoxSizer(wxHORIZONTAL);
    add_button(bottom,"Exportera TSV…",[&]{
        wxFileDialog file(&dialog,u("Exportera granskning"),"","sv1917-uttalsgranskning.tsv","TSV (*.tsv)|*.tsv",wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
        if(file.ShowModal()!=wxID_OK)return;
        try{db.export_tsv(path(file.GetPath()));wxMessageBox(u("Granskningen exporterades."),u("Uttalsgranskning"),wxOK,&dialog);}catch(const std::exception& e){wxMessageBox(u(e.what()),u("Export misslyckades"),wxOK|wxICON_ERROR,&dialog);}
    });bottom->AddStretchSpacer();auto* close=new wxButton(&dialog,wxID_CANCEL,u("Stäng"));bottom->Add(close);outer->Add(bottom,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,16);
    select=[&](size_t row){
        if(row>=visible.size())return;
        speech.stop();selected=visible[row];const auto& word=words[*selected];
        list->SetItemState(long(row),wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED);list->EnsureVisible(long(row));
        title->SetLabel(u(word.form)+wxString::Format(" · %d förekomster",word.occurrences));
        nst->ChangeValue(word.sampa.empty()?u("NST: uttal saknas"):u("NST SAMPA: "+word.sampa));
        examples=corpus.word_examples(word.form);contexts->Clear();
        for(const auto& example:examples)contexts->Append(u(corpus.book_name(example.book))+wxString::Format(" %d:%d",example.verse.ref.chapter,example.verse.ref.verse));
        if(!examples.empty()){contexts->SetSelection(0);verse->ChangeValue(u(examples[0].verse.text));}else verse->ChangeValue("");
        const auto active=make_utterance(word.form,"sv",lexicon()).speech_text;
        spoken->ChangeValue(u(active));note->ChangeValue("");
        if(auto found=decisions.find(pronunciation_key(word.form));found!=decisions.end()) {
            note->ChangeValue(u(found->second.note));
            if(found->second.status=="deferred"&&!found->second.spoken.empty())spoken->ChangeValue(u(found->second.spoken));
        }
        current->SetLabel(u("Nuvarande uttalsstavning: "+active));
        for(auto* control:{original_word,proposed_word,approve,correct,defer})control->Enable();
        original_context->Enable(!examples.empty());proposed_context->Enable(!examples.empty());spoken->Enable();note->Enable();dialog.Layout();
    };
    rebuild=[&](const std::string& preserve){
        rebuilding=true;list->Freeze();list->DeleteAllItems();visible.clear();selected.reset();
        const auto query=search->GetValue().Lower();std::set<std::string> completed;
        for(const auto& [key,value]:decisions)if(value.status!="deferred")completed.insert(key);
        for(size_t i=0;i<words.size();++i) {
            const auto& word=words[i];const auto key=pronunciation_key(word.form);
            if(category->GetSelection()==0&&(word.kind!="name"||!word.sampa.empty()))continue;
            if(category->GetSelection()==1&&!word.sampa.empty())continue;
            if(pending->GetValue()&&decisions.contains(key))continue;
            if(!query.empty()&&!u(word.form).Lower().Contains(query))continue;
            const auto row=list->InsertItem(long(visible.size()),u(word.form));list->SetItem(row,1,wxString::Format("%d",word.occurrences));
            list->SetItem(row,2,decisions.contains(key)?status_label(decisions.at(key).status):u("Ej granskat"));visible.push_back(i);
        }
        list->Thaw();rebuilding=false;
        progress->SetLabel(wxString::Format("%zu ordformer i urvalet · %zu granskade ord · %zu sparade beslut",visible.size(),completed.size(),decisions.size()));
        if(!visible.empty()){
            size_t row=0;for(size_t i=0;i<visible.size();++i)if(words[visible[i]].form==preserve){row=i;break;}select(row);
        } else {
            title->SetLabel(u("Inga ord i urvalet"));nst->ChangeValue("");contexts->Clear();verse->ChangeValue("");spoken->ChangeValue("");note->ChangeValue("");current->SetLabel("");
            for(auto* control:{original_word,original_context,proposed_word,proposed_context,approve,correct,defer})control->Disable();spoken->Disable();note->Disable();
        }
    };
    list->Bind(wxEVT_LIST_ITEM_SELECTED,[&](wxListEvent& e){const auto row=e.GetIndex();if(!rebuilding&&row>=0&&size_t(row)<visible.size()&&(!selected||*selected!=visible[size_t(row)]))select(size_t(row));});
    auto filter=[&]{const auto form=selected?words[*selected].form:"";speech.stop();rebuild(form);};
    search->Bind(wxEVT_TEXT,[&](wxCommandEvent&){filter();});category->Bind(wxEVT_CHOICE,[&](wxCommandEvent&){filter();});pending->Bind(wxEVT_CHECKBOX,[&](wxCommandEvent&){filter();});
    contexts->Bind(wxEVT_CHOICE,[&](wxCommandEvent&){speech.stop();const auto index=contexts->GetSelection();if(index>=0&&size_t(index)<examples.size())verse->ChangeValue(u(examples[size_t(index)].verse.text));});
    wxTimer timer(&dialog);dialog.Bind(wxEVT_TIMER,[&](wxTimerEvent&){
        const auto playback=speech.playback();
        const auto text=playback.state==SpeechState::Loading?u("Förbereder röstmodellen…"):playback.state==SpeechState::Buffering?u("Förbereder ljud…"):playback.state==SpeechState::Playing?u("Spelar upp"):playback.state==SpeechState::Error?u("Ljudet kunde inte skapas. Kontrollera röstpaketet."):playback.state==SpeechState::Completed?u("Uppläsningen är klar"):u("Normal hastighet · Chatterbox");
        if(feedback->GetLabel()!=text)feedback->SetLabel(text);
    },timer.GetId());
    dialog.SetSizer(outer);rebuild("");timer.Start(100);dialog.CentreOnParent();dialog.ShowModal();timer.Stop();speech.stop();
}
}
