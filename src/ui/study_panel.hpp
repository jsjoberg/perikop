#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "ui/scripture_view.hpp"
#include <wx/scrolwin.h>
#include <functional>
namespace ortho {
// The right pane in Ordstudium mode: pronunciation, Dalin's definition and
// the Strong's entries of the verse for the word clicked in the left pane.
class StudyPanel final : public wxScrolledWindow {
public:
    using Lexicon=std::function<std::vector<Pronunciation>(const std::string& language)>;
    StudyPanel(wxWindow*,const CorpusDb&,const StudyDb*,SpeechEngine&,Lexicon);
    void show(const ScriptureView::Word&,const std::string& base_source,const std::string& frame);
    void apply(Theme);
    // Lines of the current lookup, for the smoke test.
    std::vector<wxString> text() const;
private:
    void rebuild();
    const CorpusDb& corpus_;
    const StudyDb* study_;
    SpeechEngine& speech_;
    Lexicon lexicon_;
    std::optional<ScriptureView::Word> word_;
    std::string base_source_,frame_;
    // The chosen tagged Greek word of the verse.
    std::optional<std::size_t> selected_;
    Theme theme_=Theme::System;
    int wrapped_=0;
};
}
