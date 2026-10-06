#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "ui/scripture_view.hpp"
#include <functional>
#include <set>
#include <wx/scrolwin.h>
namespace ortho {
// The right pane in Ordstudium mode: pronunciation, Swedish dictionary entries and
// the Strong's entries of the verse for the word clicked in the left pane.
class StudyPanel final : public wxScrolledWindow {
public:
    using Lexicon = std::function<std::vector<Pronunciation>(const std::string& language)>;
    StudyPanel(wxWindow*, const CorpusDb&, const StudyDb*, SpeechEngine&, Lexicon);
    void show(const ScriptureView::Word&, const std::string& base_source, const std::string& frame);
    // Back to the hint, when the looked-up word is no longer on screen.
    void clear();
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
    std::string base_source_, frame_;
    std::set<std::string> expanded_articles_;
    // The chosen tagged Greek word of the verse, and whether the lookup has
    // already applied the aligner's choice (later the user's choice stands).
    std::optional<std::size_t> selected_;
    bool chosen_ = false;
    Theme theme_ = Theme::System;
    int wrapped_ = 0;
};
} // namespace ortho
