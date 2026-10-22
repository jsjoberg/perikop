#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "ui/scripture_view.hpp"
#include <functional>
#include <set>
#include <wx/scrolwin.h>
namespace ortho {
// The right pane in Ordstudium mode: pronunciation and Swedish dictionary entries
// for the word clicked in the left pane.
class StudyPanel final : public wxScrolledWindow {
public:
    using Lexicon = std::function<std::vector<Pronunciation>(const std::string& language)>;
    StudyPanel(wxWindow*, const CorpusDb&, SpeechEngine&, Lexicon);
    void show(const ScriptureView::Word&, const std::string& base_source);
    // Back to the hint, when the looked-up word is no longer on screen.
    void clear();
    void apply(Theme);
    // Lyssna would stop a reading or preparation in progress, so the window disables it meanwhile.
    void allow_listening(bool);
    // Lines of the current lookup, for the smoke test.
    std::vector<wxString> text() const;
    // The narrowest the pane gets, at the current text size.
    wxSize GetMinSize() const override {
        return FromDIP(zoomed(wxSize(260, -1)));
    }

private:
    void rebuild();
    const CorpusDb& corpus_;
    SpeechEngine& speech_;
    Lexicon lexicon_;
    std::optional<ScriptureView::Word> word_;
    std::optional<SpeechUtterance> utterance_;
    std::string pronunciation_;
    std::size_t pronunciation_epoch_ = 0;
    bool pronunciation_requested_ = false;
    wxWindow* listen_ = nullptr;
    bool listening_allowed_ = true;
    std::string base_source_;
    std::set<std::string> expanded_articles_;
    Theme theme_ = Theme::System;
    int wrapped_ = 0;
    bool rebuild_pending_ = false;
};
} // namespace ortho
