#include "ui/study_panel.hpp"
#include "ui/controls.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/uri.h>
#include <wx/weakref.h>
namespace ortho {
StudyPanel::StudyPanel(wxWindow* parent, const CorpusDb& corpus, SpeechEngine& speech, Lexicon lexicon)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE),
      corpus_(corpus), speech_(speech), lexicon_(std::move(lexicon)) {
    SetScrollRate(0, FromDIP(zoomed(12)));
    // Glosses wrap to the panel width; rewrap when it changes noticeably.
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        if (std::abs(GetClientSize().x - wrapped_) > FromDIP(zoomed(12)) && !rebuild_pending_) {
            rebuild_pending_ = true;
            CallAfter([this] {
                rebuild_pending_ = false;
                if (std::abs(GetClientSize().x - wrapped_) > FromDIP(zoomed(12)))
                    rebuild();
            });
        }
        e.Skip();
    });
    rebuild();
}
void StudyPanel::apply(Theme theme) {
    theme_ = theme;
    rebuild();
}
void StudyPanel::allow_listening(bool allowed) {
    listening_allowed_ = allowed;
    if (listen_)
        listen_->Enable(allowed);
}
void StudyPanel::clear() {
    if (word_) {
        word_.reset();
        placed_.clear();
        utterance_.reset();
        pronunciation_.clear();
        ++pronunciation_epoch_;
        rebuild();
    }
}
void StudyPanel::show(const ScriptureView::Word& word, const std::string& base_source,
                      const std::string& frame) {
    word_ = word;
    placed_.clear();
    const auto language = source_language(base_source);
    if (!frame.empty())
        if (const auto verse = corpus_.parallel_verse(frame, base_source, word.book, word.verse))
            placed_ = PronunciationMatcher(language, lexicon_(language))
                          .phonemes_at(verse->text, word.text, word.occurrence);
    utterance_.reset();
    pronunciation_.clear();
    pronunciation_requested_ = false;
    ++pronunciation_epoch_;
    base_source_ = base_source;
    rebuild();
    Scroll(0, 0);
}
std::vector<wxString> StudyPanel::text() const {
    std::vector<wxString> result;
    for (auto* child : GetChildren())
        if (auto* label = wxDynamicCast(child, wxStaticText))
            result.push_back(label->GetLabel());
    return result;
}
void StudyPanel::rebuild() {
    Freeze();
    DestroyChildren();
    listen_ = nullptr;
    const auto colors = palette(theme_);
    const int pad = FromDIP(zoomed(20));
    wrapped_ = GetClientSize().x;
    const int width = std::max(FromDIP(zoomed(160)), wrapped_ - 2 * pad - FromDIP(zoomed(16)));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(zoomed(28)));
    const auto text = [&](const wxString& value, const wxFont& font, const wxColour& colour, int below = 6) {
        auto* label = new wxStaticText(this, wxID_ANY, value);
        label->SetFont(font);
        label->SetForegroundColour(colour);
        label->Wrap(width);
        sizer->Add(label, 0, wxLEFT | wxRIGHT, pad);
        sizer->AddSpacer(FromDIP(zoomed(below)));
        return label;
    };
    const auto link = [&](const wxString& label, const wxString& url) {
        auto* source = new wxHyperlinkCtrl(this, wxID_ANY, label, url);
        source->SetFont(ui_font(9));
        source->SetNormalColour(colors.accent);
        source->SetVisitedColour(colors.accent);
        source->SetHoverColour(colors.ink);
        source->SetToolTip(url);
        sizer->Add(source, 0, wxLEFT, pad);
        sizer->AddSpacer(FromDIP(zoomed(5)));
    };
    if (!word_)
        text(ui::utf8("Klicka på ett ord i texten för att slå upp det."), ui_font(12), colors.muted);
    else {
        const auto& word = *word_;
        const auto language = source_language(base_source_);
        const bool swedish = language == "sv";
        text(ui::utf8(word.text), body_font(26), colors.ink, 4);
        // What the voice says: bundled phonemes and review corrections apply.
        if (!utterance_)
            utterance_ = placed_.empty() ? make_utterance(word.text, language, lexicon_(language))
                                         : SpeechUtterance{word.text, "⟦" + placed_ + "⟧", language};
        const auto utterance = *utterance_;
        if (swedish && !pronunciation_requested_) {
            pronunciation_requested_ = true;
            auto weak = std::make_shared<wxWeakRef<StudyPanel>>(this);
            const auto epoch = pronunciation_epoch_;
            speech_.pronunciation_async(utterance.speech_text, [weak, epoch](std::string ipa) {
                // wxWeakRef is inspected only on the UI thread, after worker delivery.
                wxTheApp->CallAfter([weak, epoch, ipa = std::move(ipa)] {
                    if (*weak && (*weak)->pronunciation_epoch_ == epoch && !ipa.empty()) {
                        (*weak)->pronunciation_ = ipa;
                        (*weak)->rebuild();
                    }
                });
            });
        }
        if (swedish && !pronunciation_.empty())
            text(ui::utf8("/" + pronunciation_ + "/"), ui_font(13), colors.muted, 4);
        // Read-aloud is Swedish only.
        if (swedish) {
            listen_ = ui::button(this, "Lyssna", [this, utterance] {
                speech_.stop();
                speech_.speak(utterance);
            });
            listen_->SetToolTip(ui::utf8("Lyssna på ordet"));
            listen_->Enable(listening_allowed_);
            sizer->Add(listen_, 0, wxLEFT | wxBOTTOM, pad);
        }
        if (swedish) {
            // Each sense: base form and kind, a one-line gloss, and where to read more.
            auto senses = corpus_.senses(ui::to_utf8(ui::utf8(word.text).Lower()));
            // The sense said here: the one with the verse's phonemes, else the word's usual one.
            auto here = std::ranges::find_if(senses, [&](const WordSense& sense) {
                return !placed_.empty() && sense.phonemes == placed_;
            });
            if (here == senses.end())
                here = std::ranges::find(senses, true, &WordSense::usual);
            const bool placed = here != senses.end();
            if (placed)
                std::rotate(senses.begin(), here, here + 1);
            else if (wxIslower(ui::utf8(word.text)[0]))
                // A word written without a capital is not a name here.
                std::ranges::stable_partition(senses, [](const WordSense& sense) {
                    return sense.kind != "egennamn";
                });
            const auto dictionaries = [&](const std::string& headword) {
                link(ui::utf8("Slå upp " + headword + " i Svenska Akademiens ordböcker"),
                     wxURI(ui::utf8("https://svenska.se/?q=" + headword)).BuildURI());
            };
            std::string looked_up;
            for (const auto& sense : senses) {
                sizer->AddSpacer(FromDIP(zoomed(10)));
                if (placed && &sense == &senses.front())
                    text(ui::utf8("SÅ LÄSES ORDET HÄR"), ui_font(9), colors.muted, 4);
                else if (placed && senses.size() > 1 && &sense == &senses[1])
                    text(ui::utf8("ANDRA BETYDELSER"), ui_font(9), colors.muted, 4);
                text(ui::utf8(sense.headword + "   " + sense.kind), body_font(15), colors.ink, 2);
                if (!sense.gloss.empty())
                    text(ui::utf8(sense.gloss), ui_font(11), colors.ink, 4);
                if (!sense.url.empty())
                    link(ui::utf8("Läs artikeln i Biblisk ordbok, Nyström 1896"), ui::utf8(sense.url));
                else if (sense.headword != looked_up)
                    dictionaries(looked_up = sense.headword);
            }
            if (senses.empty()) {
                sizer->AddSpacer(FromDIP(zoomed(10)));
                text(ui::utf8("Ordet finns inte i ordlistan."), ui_font(11), colors.muted);
                dictionaries(word.text);
            }
            sizer->AddSpacer(FromDIP(zoomed(18)));
            text(ui::utf8("Ordklass och samband: SALDO. Förklaringar: Dalins ordbok 1850–53 och Nyströms "
                          "Biblisk ordbok 1896, med äldre språk och tolkningar."),
                 ui_font(8), colors.muted, 4);
            link(ui::utf8("Källa: Språkbanken Text"), "https://www.sprakbanken.se/resurser/saldo");
            link("CC BY 4.0", "https://creativecommons.org/licenses/by/4.0/");
        }
    }
    SetSizer(sizer);
    SetBackgroundColour(colors.paper);
    for (auto* child : GetChildren())
        child->SetBackgroundColour(colors.paper);
    FitInside();
    Layout();
    Thaw();
    Refresh();
    // wxGTK left this pane out of the Tab order while it had no buttons. Moving it to the place it
    // already has makes the window list it again, now with its Lyssna button and links.
    if (auto* previous = GetPrevSibling())
        MoveAfterInTabOrder(previous);
}
} // namespace ortho
