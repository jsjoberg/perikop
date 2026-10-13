#include "ui/study_panel.hpp"
#include "ui/controls.hpp"
#include "ui/theme.hpp"
#include <map>
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/weakref.h>
namespace ortho {
namespace {
wxString word_class(const std::string& code) {
    static const std::map<std::string, std::string> names = {
        {"nn", "substantiv"},  {"av", "adjektiv"},     {"vb", "verb"},        {"ab", "adverb"},
        {"pm", "egennamn"},    {"pn", "pronomen"},     {"pp", "preposition"}, {"kn", "konjunktion"},
        {"sn", "subjunktion"}, {"in", "interjektion"}, {"nl", "räkneord"},    {"al", "artikel"}};
    if (const auto found = names.find(code); found != names.end())
        return ui::utf8(found->second);
    return ui::utf8(code);
}
} // namespace
StudyPanel::StudyPanel(wxWindow* parent, const CorpusDb& corpus, SpeechEngine& speech, Lexicon lexicon)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE),
      corpus_(corpus), speech_(speech), lexicon_(std::move(lexicon)) {
    SetMinSize(FromDIP(wxSize(260, -1)));
    SetScrollRate(0, FromDIP(12));
    // Definitions wrap to the panel width; rewrap when it changes noticeably.
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        if (std::abs(GetClientSize().x - wrapped_) > FromDIP(12) && !rebuild_pending_) {
            rebuild_pending_ = true;
            CallAfter([this] {
                rebuild_pending_ = false;
                if (std::abs(GetClientSize().x - wrapped_) > FromDIP(12))
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
        utterance_.reset();
        pronunciation_.clear();
        ++pronunciation_epoch_;
        expanded_articles_.clear();
        rebuild();
    }
}
void StudyPanel::show(const ScriptureView::Word& word, const std::string& base_source) {
    word_ = word;
    utterance_.reset();
    pronunciation_.clear();
    pronunciation_requested_ = false;
    ++pronunciation_epoch_;
    base_source_ = base_source;
    expanded_articles_.clear();
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
    const int pad = FromDIP(20);
    wrapped_ = GetClientSize().x;
    const int width = std::max(FromDIP(160), wrapped_ - 2 * pad - FromDIP(16));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(28));
    const auto text = [&](const wxString& value, const wxFont& font, const wxColour& colour, int below = 6) {
        auto* label = new wxStaticText(this, wxID_ANY, value);
        label->SetFont(font);
        label->SetForegroundColour(colour);
        label->Wrap(width);
        sizer->Add(label, 0, wxLEFT | wxRIGHT, pad);
        sizer->AddSpacer(FromDIP(below));
        return label;
    };
    const auto section = [&](const wxString& title) {
        sizer->AddSpacer(FromDIP(14));
        text(title, ui_font(9), colors.muted, 4);
    };
    const auto link = [&](const wxString& label, const wxString& url) {
        auto* source = new wxHyperlinkCtrl(this, wxID_ANY, label, url);
        source->SetFont(ui_font(9));
        source->SetNormalColour(colors.accent);
        source->SetVisitedColour(colors.accent);
        source->SetHoverColour(colors.ink);
        sizer->Add(source, 0, wxLEFT, pad);
        sizer->AddSpacer(FromDIP(5));
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
            utterance_ = make_utterance(word.text, language, lexicon_(language));
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
            listen_->Enable(listening_allowed_);
            sizer->Add(listen_, 0, wxLEFT | wxBOTTOM, pad);
        }
        if (swedish) {
            const auto form = ui::to_utf8(ui::utf8(word.text).Lower());
            const auto entries = corpus_.swedish(form);
            const auto articles = corpus_.biblical(form);
            const auto saldo = corpus_.saldo(form);
            const auto academic = corpus_.academic(form);
            const auto modern = [&](const std::vector<ModernWordEntry>& words, bool semantic) {
                if (words.empty())
                    return;
                section(ui::utf8(semantic ? "ORD OCH ASSOCIATIONER, SALDO" : "AKADEMISK ORDLISTA"));
                for (const auto& entry : words) {
                    text(ui::utf8(entry.headword) + "   " + word_class(entry.gram), body_font(15), colors.ink,
                         2);
                    if (entry.match != "exact")
                        text(ui::utf8(entry.match == "curated" ? "Äldre form, kopplad till grundformen"
                                                               : "Böjd form, visar grundformen"),
                             ui_font(9), colors.muted, 3);
                    if (!entry.associations.empty())
                        text(ui::utf8("Associationer (betydelsesamband): " + entry.associations), ui_font(11),
                             colors.ink, 6);
                }
                text(ui::utf8(semantic ? "Lars Borin, Lennart Lönngren och Markus Forsberg, Språkbanken"
                                       : "Judy Carola Ribeck, Håkan Jansson och Emma Sköldberg, Språkbanken"),
                     ui_font(8), colors.muted, 4);
                link(ui::utf8("Källa: Språkbanken"), semantic ? "https://www.sprakbanken.se/resurser/saldo"
                                                              : "https://www.sprakbanken.se/resurser/ao");
                link("CC BY 4.0", "https://creativecommons.org/licenses/by/4.0/");
            };
            modern(saldo, true);
            modern(academic, false);
            if (!entries.empty())
                section(ui::utf8("ORDBOK, DALIN 1850–53"));
            for (const auto& entry : entries) {
                text(ui::utf8(entry.headword) +
                         (entry.gram.empty() ? wxString{} : ui::utf8("   ") + ui::utf8(entry.gram)),
                     body_font(15), colors.ink, 2);
                text(ui::utf8(entry.definition), ui_font(11), colors.ink, 10);
            }
            if (!articles.empty()) {
                section(ui::utf8("BIBLISK ORDBOK, NYSTRÖM 1896"));
                text(ui::utf8("Historisk källa: språk, uppgifter och tolkningar från 1896."), ui_font(9),
                     colors.muted, 8);
                for (const auto& entry : articles) {
                    text(ui::utf8(entry.headword), body_font(15), colors.ink, 2);
                    const auto definition = ui::utf8(entry.definition);
                    const bool expanded = expanded_articles_.contains(entry.id);
                    constexpr std::size_t preview_length = 500;
                    wxString preview = definition;
                    if (!expanded && definition.length() > preview_length) {
                        // Trim at a word boundary, never in the middle of a UTF-8 sequence.
                        preview = definition.Left(preview_length);
                        const auto boundary = preview.find_last_of(" \n");
                        if (boundary != wxString::npos)
                            preview = preview.Left(boundary);
                        preview += ui::utf8("…");
                    }
                    text(preview, ui_font(11), colors.ink, 6);
                    if (definition.length() > preview_length) {
                        auto* more =
                            ui::button(this, ui::utf8(expanded ? "Visa mindre" : "Visa hela artikeln"),
                                       [this, id = entry.id] {
                                           CallAfter([this, id] {
                                               if (!expanded_articles_.erase(id))
                                                   expanded_articles_.insert(id);
                                               rebuild();
                                           });
                                       });
                        more->SetFont(ui_font(10));
                        sizer->Add(more, 0, wxLEFT, pad);
                        sizer->AddSpacer(FromDIP(5));
                    }
                    auto* source = new wxHyperlinkCtrl(this, wxID_ANY, ui::utf8("Källa: Projekt Runeberg"),
                                                       ui::utf8(entry.url));
                    source->SetFont(ui_font(9));
                    source->SetNormalColour(colors.accent);
                    source->SetVisitedColour(colors.accent);
                    source->SetHoverColour(colors.ink);
                    sizer->Add(source, 0, wxLEFT, pad);
                    sizer->AddSpacer(FromDIP(12));
                }
            }
            if (entries.empty() && articles.empty() && saldo.empty() && academic.empty()) {
                section(ui::utf8("SVENSKA UPPSLAGSKÄLLOR"));
                text(ui::utf8("Ordet finns inte i de svenska uppslagskällorna."), ui_font(11), colors.muted);
            }
        }
        sizer->AddSpacer(FromDIP(18));
        text(ui::utf8("Dalins ordbok, Språkbanken Text, CC BY 4.0"), ui_font(8), colors.muted);
    }
    SetSizer(sizer);
    SetBackgroundColour(colors.paper);
    for (auto* child : GetChildren())
        child->SetBackgroundColour(colors.paper);
    FitInside();
    Layout();
    Thaw();
    Refresh();
}
} // namespace ortho
