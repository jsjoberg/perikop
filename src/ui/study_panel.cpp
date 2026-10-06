#include "ui/study_panel.hpp"
#include "ui/theme.hpp"
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
namespace ortho {
namespace {
wxString u(const std::string& s){return wxString::FromUTF8(s);}
std::string utf8(const wxString& s){const auto bytes=s.ToUTF8();return {bytes.data(),bytes.length()};}
// Strong's numbers read "G746", without the lexicon's padding zeros.
wxString strong_label(const std::string& strong) {
    std::size_t digits=1;while(digits+1<strong.size()&&strong[digits]=='0')++digits;
    return u(strong.substr(0,1)+strong.substr(digits));
}
const char* greek_source="grc-patriarchal";
}
StudyPanel::StudyPanel(wxWindow* parent,const CorpusDb& corpus,const StudyDb* study,SpeechEngine& speech,Lexicon lexicon)
    :wxScrolledWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL|wxBORDER_NONE),
    corpus_(corpus),study_(study),speech_(speech),lexicon_(std::move(lexicon)) {
    SetMinSize(FromDIP(wxSize(260,-1)));SetScrollRate(0,FromDIP(12));
    // Definitions wrap to the panel width; rewrap when it changes noticeably.
    Bind(wxEVT_SIZE,[this](wxSizeEvent& e){
        if(std::abs(GetClientSize().x-wrapped_)>FromDIP(12))CallAfter([this]{rebuild();});
        e.Skip();
    });
    rebuild();
}
void StudyPanel::apply(Theme theme){theme_=theme;rebuild();}
void StudyPanel::show(const ScriptureView::Word& word,const std::string& base_source,const std::string& frame) {
    word_=word;base_source_=base_source;frame_=frame;selected_.reset();
    // A Greek word is its own tagged word: the same occurrence of the same form.
    if(base_source==greek_source&&study_&&new_testament_book(word.book)) {
        const auto target=u(word.text).Lower();int seen=0;
        const auto words=study_->greek_words(word.book,word.verse);
        for(std::size_t i=0;i<words.size();++i)
            if(u(words[i].surface).Lower()==target&&seen++==word.occurrence){selected_=i;break;}
    }
    rebuild();Scroll(0,0);
}
std::vector<wxString> StudyPanel::text() const {
    std::vector<wxString> result;
    for(auto* child:GetChildren())if(auto* label=wxDynamicCast(child,wxStaticText))result.push_back(label->GetLabel());
    return result;
}
void StudyPanel::rebuild() {
    Freeze();DestroyChildren();
    const auto colors=palette(theme_);
    const int pad=FromDIP(20);wrapped_=GetClientSize().x;
    const int width=std::max(FromDIP(160),wrapped_-2*pad-FromDIP(16));
    auto* sizer=new wxBoxSizer(wxVERTICAL);sizer->AddSpacer(FromDIP(28));
    const auto text=[&](const wxString& value,const wxFont& font,const wxColour& colour,int below=6) {
        auto* label=new wxStaticText(this,wxID_ANY,value);label->SetFont(font);label->SetForegroundColour(colour);label->Wrap(width);
        sizer->Add(label,0,wxLEFT|wxRIGHT,pad);sizer->AddSpacer(FromDIP(below));
        return label;
    };
    const auto section=[&](const wxString& title){sizer->AddSpacer(FromDIP(14));text(title,ui_font(9),colors.muted,4);};
    if(!study_)text(u("Ordstudiedata saknas. Bygg resources/lexicon/study.db med tools/lexicon/build_study.py."),ui_font(12),colors.muted);
    else if(!word_)text(u("Klicka på ett ord i texten för att slå upp det."),ui_font(12),colors.muted);
    else {
        const auto& word=*word_;
        const bool swedish=base_source_.starts_with("sv"),greek=base_source_==greek_source;
        const std::string language=swedish?"sv":base_source_.starts_with("grc")?"el":"en";
        text(u(word.text),body_font(26),colors.ink,4);
        // What the voice says: bundled phonemes and review corrections apply.
        const auto utterance=make_utterance(word.text,language,lexicon_(language));
        if(swedish)if(const auto ipa=speech_.pronunciation(utterance.speech_text);!ipa.empty())text(u("/"+ipa+"/"),ui_font(13),colors.muted,4);
        auto* listen=new wxButton(this,wxID_ANY,u("Lyssna"),wxDefaultPosition,wxDefaultSize,wxBU_EXACTFIT);listen->SetFont(ui_font());
        listen->Bind(wxEVT_BUTTON,[this,utterance](wxCommandEvent&){speech_.stop();speech_.speak(utterance);});
        sizer->Add(listen,0,wxLEFT|wxBOTTOM,pad);
        if(swedish) {
            section(u("ORDBOK · DALIN 1850–53"));
            const auto entries=study_->swedish(utf8(u(word.text).Lower()));
            if(entries.empty())text(u(word.text).Left(1).IsSameAs(u(word.text).Left(1).Lower())
                ?u("Ordet finns inte i Dalins ordbok."):u("Troligen ett egennamn. Det finns inte i Dalins ordbok."),ui_font(11),colors.muted);
            for(const auto& entry:entries) {
                text(u(entry.headword)+(entry.gram.empty()?wxString{}:u("   ")+u(entry.gram)),body_font(15),colors.ink,2);
                text(u(entry.definition),ui_font(11),colors.ink,10);
            }
        }
        section(u("GREKISKA · STRONG'S"));
        if(!new_testament_book(word.book))text(u("Strong's-uppgifter finns ännu bara för Nya testamentet."),ui_font(11),colors.muted);
        else {
            // The verse's tagged Greek words, in each Greek verse it translates.
            std::vector<std::pair<std::string,VerseRef>> verses;
            if(frame_==greek_source)verses.emplace_back(word.book,word.verse);
            else verses=corpus_.counterparts(frame_,greek_source,word.book,word.verse);
            std::vector<GreekWord> words;
            for(const auto& [book,ref]:verses){auto part=study_->greek_words(book,ref);words.insert(words.end(),part.begin(),part.end());}
            if(words.empty())text(u("Versen saknar Strong's-taggning i den grekiska källan."),ui_font(11),colors.muted);
            else {
                if(!greek)text(u("Välj det grekiska ord som motsvarar ordet."),ui_font(10),colors.muted,4);
                for(std::size_t i=0;i<words.size();++i) {
                    const auto entry=study_->strongs(words[i].strong);
                    const bool chosen=selected_==i;
                    auto* row=text(u(words[i].surface)+u("   ")+(entry?u(entry->gloss):wxString{})+u("   ")+strong_label(words[i].strong),
                        chosen?body_font(14).Bold():body_font(14),chosen?colors.accent:colors.ink,1);
                    row->SetCursor(wxCursor(wxCURSOR_HAND));
                    // Rebuilding destroys the clicked label, so do it after the click.
                    row->Bind(wxEVT_LEFT_UP,[this,i](wxMouseEvent&){CallAfter([this,i]{selected_=selected_==i?std::nullopt:std::optional<std::size_t>{i};rebuild();});});
                    // The chosen word's lexicon entry opens below its row.
                    if(!chosen)continue;
                    if(entry) {
                        sizer->AddSpacer(FromDIP(6));
                        text(u(entry->lemma)+u("   ")+u(entry->transliteration),body_font(18),colors.ink,2);
                        text(u(entry->definition),ui_font(10),colors.muted,12);
                    } else text(u("Strong's-numret saknas i lexikonet."),ui_font(11),colors.muted,12);
                }
            }
        }
        sizer->AddSpacer(FromDIP(18));
        text(u("Dalins ordbok, Språkbanken Text, CC BY 4.0 · Strong's och lexikon: STEP Bible, www.STEPBible.org, CC BY 4.0"),ui_font(8),colors.muted);
    }
    SetSizer(sizer);SetBackgroundColour(colors.paper);
    for(auto* child:GetChildren())child->SetBackgroundColour(colors.paper);
    FitInside();Layout();Thaw();Refresh();
}
}
