#include "ui/study_panel.hpp"
#include "ui/controls.hpp"
#include "ui/theme.hpp"
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/weakref.h>
namespace ortho {
namespace {
// Strong's numbers read "G746", without the lexicon's padding zeros.
wxString strong_label(const std::string& strong) {
    std::size_t digits = 1;
    while (digits + 1 < strong.size() && strong[digits] == '0')
        ++digits;
    return ui::utf8(strong.substr(0, 1) + strong.substr(digits));
}
const char* greek_source = "grc-patriarchal";
} // namespace
StudyPanel::StudyPanel(wxWindow* parent, const CorpusDb& corpus, const StudyDb* study, SpeechEngine& speech,
                       Lexicon lexicon)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE),
      corpus_(corpus), study_(study), speech_(speech), lexicon_(std::move(lexicon)) {
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
void StudyPanel::clear() {
    if (word_) {
        word_.reset();
        utterance_.reset();
        pronunciation_.clear();
        ++pronunciation_epoch_;
        selected_.reset();
        expanded_articles_.clear();
        rebuild();
    }
}
void StudyPanel::show(const ScriptureView::Word& word, const std::string& base_source,
                      const std::string& frame) {
    word_ = word;
    utterance_.reset();
    pronunciation_.clear();
    pronunciation_requested_ = false;
    ++pronunciation_epoch_;
    base_source_ = base_source;
    frame_ = frame;
    selected_.reset();
    chosen_ = false;
    expanded_articles_.clear();
    // A Greek word is its own tagged word: the same occurrence of the same form.
    if (base_source == greek_source && study_ && corpus_.new_testament_book(word.book)) {
        const auto target = ui::utf8(word.text).Lower();
        int seen = 0;
        const auto words = study_->greek_words(word.book, word.verse);
        for (std::size_t i = 0; i < words.size(); ++i)
            if (ui::utf8(words[i].surface).Lower() == target && seen++ == word.occurrence) {
                selected_ = i;
                break;
            }
    }
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
    if (!study_)
        text(ui::utf8(
                 "Ordstudiedata saknas. Bygg resources/lexicon/study.db med tools/lexicon/build_study.py."),
             ui_font(12), colors.muted);
    else if (!word_)
        text(ui::utf8("Klicka på ett ord i texten för att slå upp det."), ui_font(12), colors.muted);
    else {
        const auto& word = *word_;
        const auto language = source_language(base_source_);
        const bool swedish = language == "sv", greek = base_source_ == greek_source;
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
            auto* listen = ui::button(this, "Lyssna", [this, utterance] {
                speech_.stop();
                speech_.speak(utterance);
            });
            sizer->Add(listen, 0, wxLEFT | wxBOTTOM, pad);
        }
        if (swedish) {
            const auto form = ui::to_utf8(ui::utf8(word.text).Lower());
            const auto entries = study_->swedish(form);
            const auto articles = study_->biblical(form);
            if (!entries.empty())
                section(ui::utf8("ORDBOK · DALIN 1850–53"));
            for (const auto& entry : entries) {
                text(ui::utf8(entry.headword) +
                         (entry.gram.empty() ? wxString{} : ui::utf8("   ") + ui::utf8(entry.gram)),
                     body_font(15), colors.ink, 2);
                text(ui::utf8(entry.definition), ui_font(11), colors.ink, 10);
            }
            if (!articles.empty()) {
                section(ui::utf8("BIBLISK ORDBOK · NYSTRÖM 1896"));
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
                    auto* source = new wxHyperlinkCtrl(this, wxID_ANY, ui::utf8("Källa · Projekt Runeberg"),
                                                       ui::utf8(entry.url));
                    source->SetFont(ui_font(9));
                    source->SetNormalColour(colors.accent);
                    source->SetVisitedColour(colors.accent);
                    source->SetHoverColour(colors.ink);
                    sizer->Add(source, 0, wxLEFT, pad);
                    sizer->AddSpacer(FromDIP(12));
                }
            }
            if (entries.empty() && articles.empty()) {
                section(ui::utf8("SVENSKA UPPSLAGSKÄLLOR"));
                text(ui::utf8("Ordet finns inte i de svenska uppslagskällorna."), ui_font(11), colors.muted);
            }
        }
        section(ui::utf8("GREKISKA · STRONG'S"));
        if (!corpus_.new_testament_book(word.book))
            text(ui::utf8("Strong's-uppgifter finns ännu bara för Nya testamentet."), ui_font(11),
                 colors.muted);
        else {
            // The verse's tagged Greek words, in each Greek verse it translates.
            std::vector<std::pair<std::string, VerseRef>> verses;
            if (frame_ == greek_source)
                verses.emplace_back(word.book, word.verse);
            else
                verses = corpus_.counterparts(frame_, greek_source, word.book, word.verse);
            std::vector<GreekWord> words;
            // A Swedish word preselects the Greek word the aligner linked it to, once per lookup.
            const auto link =
                swedish ? study_->greek_link(word.book, word.verse, ui::to_utf8(ui::utf8(word.text).Lower()),
                                             word.occurrence)
                        : std::nullopt;
            for (const auto& [book, ref] : verses) {
                if (link && !chosen_ && ref == link->first)
                    selected_ = words.size() + std::size_t(link->second);
                auto part = study_->greek_words(book, ref);
                words.insert(words.end(), part.begin(), part.end());
            }
            chosen_ = true;
            if (words.empty())
                text(ui::utf8("Versen saknar Strong's-taggning i den grekiska källan."), ui_font(11),
                     colors.muted);
            else {
                if (!greek)
                    text(link ? ui::utf8(
                                    "Förvalt av en automatisk ordlänkning. Välj ett annat ord om det inte "
                                    "stämmer.")
                              : ui::utf8("Välj det grekiska ord som motsvarar ordet."),
                         ui_font(10), colors.muted, 4);
                for (std::size_t i = 0; i < words.size(); ++i) {
                    const auto entry = study_->strongs(words[i].strong);
                    const bool chosen = selected_ == i;
                    auto* row = text(ui::utf8(words[i].surface) + ui::utf8("   ") +
                                         (entry ? ui::utf8(entry->gloss) : wxString{}) + ui::utf8("   ") +
                                         strong_label(words[i].strong),
                                     chosen ? body_font(14).Bold() : body_font(14),
                                     chosen ? colors.accent : colors.ink, 1);
                    row->SetCursor(wxCursor(wxCURSOR_HAND));
                    // Rebuilding destroys the clicked label, so do it after the click.
                    row->Bind(wxEVT_LEFT_UP, [this, i](wxMouseEvent&) {
                        CallAfter([this, i] {
                            selected_ = selected_ == i ? std::nullopt : std::optional<std::size_t>{i};
                            chosen_ = true;
                            rebuild();
                        });
                    });
                    // The chosen word's lexicon entry opens below its row.
                    if (!chosen)
                        continue;
                    if (entry) {
                        sizer->AddSpacer(FromDIP(6));
                        text(ui::utf8(entry->lemma) + ui::utf8("   ") + ui::utf8(entry->transliteration),
                             body_font(18), colors.ink, 2);
                        text(ui::utf8(entry->definition), ui_font(10), colors.muted, 12);
                    } else
                        text(ui::utf8("Strong's-numret saknas i lexikonet."), ui_font(11), colors.muted, 12);
                }
            }
        }
        sizer->AddSpacer(FromDIP(18));
        text(ui::utf8("Dalins ordbok, Språkbanken Text, CC BY 4.0 · Strong's och lexikon: STEP Bible, "
                      "www.STEPBible.org, CC BY 4.0"),
             ui_font(8), colors.muted);
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
