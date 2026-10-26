#include "speech/espeak_voice.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/hyperlink.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
namespace ortho {
namespace {
class SourceStack final : public wxPanel {
public:
    SourceStack(wxWindow* parent, std::vector<SourcePreference>& preferences,
                const std::vector<Source>& catalog, const std::map<std::string, wxString>& details,
                bool primary, std::function<void()> changed)
        : wxPanel(parent), preferences_(preferences), catalog_(catalog), details_(details), primary_(primary),
          changed_(std::move(changed)) {
        rebuild();
    }

private:
    std::vector<SourcePreference>& preferences_;
    std::vector<Source> catalog_;
    std::map<std::string, wxString> details_;
    std::vector<wxCheckBox*> checks_;
    bool primary_;
    std::function<void()> changed_;
    void rebuild() {
        DestroyChildren();
        checks_.clear();
        auto* stack = new wxBoxSizer(wxVERTICAL);
        const auto enabled = std::ranges::count(preferences_, true, &SourcePreference::enabled);
        for (std::size_t i = 0; i < preferences_.size(); ++i) {
            const auto& preference = preferences_[i];
            const auto found = std::ranges::find(catalog_, preference.source, &Source::code);
            const auto name = ui::utf8(found == catalog_.end() ? preference.source : found->name);
            auto* row = new wxBoxSizer(wxHORIZONTAL);
            for (const int direction : {-1, 1}) {
                auto* move = ui::button(this, ui::utf8(direction < 0 ? "↑" : "↓"), [this, i, direction] {
                    CallAfter([this, i, direction] {
                        const auto destination = direction < 0 ? i - 1 : i + 1;
                        std::swap(preferences_[i], preferences_[destination]);
                        changed_();
                        rebuild();
                        checks_[destination]->SetFocus();
                    });
                });
                const auto label = ui::utf8(direction < 0 ? "Flytta upp " : "Flytta ner ") + name;
                move->SetName(label);
                move->SetToolTip(label);
                move->Enable(direction < 0 ? i > 0 : i + 1 < preferences_.size());
                row->Add(move, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(4)));
            }
            auto* check = new wxCheckBox(this, wxID_ANY, name);
            checks_.push_back(check);
            check->SetFont(ui_font());
            check->SetValue(preference.enabled);
            check->SetName(ui::utf8(primary_ ? "Huvudtext, " : "Andra texten, ") + name);
            check->SetToolTip(ui::utf8("Aktivera eller inaktivera denna källa i prioriteringsordningen."));
            check->Enable(found != catalog_.end() && (!primary_ || !preference.enabled || enabled > 1));
            check->Bind(wxEVT_CHECKBOX, [this, i](wxCommandEvent& event) {
                preferences_[i].enabled = event.IsChecked();
                changed_();
                CallAfter([this] {
                    rebuild();
                });
            });
            auto* text = new wxBoxSizer(wxVERTICAL);
            text->Add(check);
            if (const auto detail = details_.find(preference.source); detail != details_.end())
                text->Add(ui::label(this, detail->second, 9), 0, wxTOP, FromDIP(zoomed(2)));
            row->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(8)));
            stack->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(5)));
        }
        SetSizer(stack, true);
        SetMinSize(stack->GetMinSize());
        Layout();
        if (GetParent())
            GetParent()->Layout();
        for (auto* child : GetChildren()) {
            child->SetBackgroundColour(GetBackgroundColour());
            child->SetForegroundColour(GetForegroundColour());
        }
    }
};
// The chosen colour scheme's light and dark variants as small reader pages, so both can be judged at once.
class SchemePreview final : public wxWindow {
public:
    explicit SchemePreview(wxWindow* parent) : wxWindow(parent, wxID_ANY) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(zoomed(wxSize(2 * page_width + gap, page_height + 22))));
        Bind(wxEVT_PAINT, &SchemePreview::paint, this);
    }
    bool AcceptsFocus() const override {
        return false;
    }
    int pair_width() const {
        return FromDIP(zoomed(2 * page_width + gap));
    }
    void fit_width(int width) {
        stacked_ = width < pair_width();
        SetMinSize(FromDIP(zoomed(wxSize(stacked_ ? page_width : 2 * page_width + gap,
                                         stacked_ ? 2 * (page_height + 22) + gap : page_height + 22))));
        Refresh();
    }

private:
    static constexpr int page_width = 210, page_height = 172, gap = 14;
    bool stacked_ = false;
    void paint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc)
            return;
        const double unit = FromDIP(zoomed(100)) / 100.0;
        const auto& names = scheme_names[std::size_t(color_scheme)];
        for (const bool dark : {false, true}) {
            gc->PushState();
            gc->Translate(dark && !stacked_ ? (page_width + gap) * unit : 0,
                          dark && stacked_ ? (page_height + 22 + gap) * unit : 0);
            gc->SetFont(ui_font(9), GetForegroundColour());
            gc->DrawText(ui::utf8(dark ? names.dark : names.light), 0, 0);
            gc->Translate(0, 20 * unit);
            page(*gc, palette(color_scheme, dark), unit);
            gc->PopState();
        }
    }
    static void page(wxGraphicsContext& gc, const Palette& colors, double unit) {
        double width = 0, height = 0;
        const auto text = [&](const wxString& line, const wxFont& font, const wxColour& colour, double x,
                              double y) {
            gc.SetFont(font, colour);
            gc.DrawText(line, x * unit, y * unit);
            gc.GetTextExtent(line, &width, &height);
            return x + width / unit;
        };
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colors.rule).Width(unit)));
        gc.SetBrush(wxBrush(colors.paper));
        gc.DrawRoundedRectangle(0, 0, page_width * unit, page_height * unit, 6 * unit);
        gc.SetPen(*wxTRANSPARENT_PEN);
        // The address field, resting and hovered.
        gc.SetBrush(wxBrush(colors.surface));
        gc.DrawRoundedRectangle(46 * unit, 9 * unit, 118 * unit, 18 * unit, 5 * unit);
        gc.SetBrush(wxBrush(colors.overlay));
        gc.DrawRoundedRectangle(14 * unit, 9 * unit, 22 * unit, 18 * unit, 5 * unit);
        text(ui::utf8("Joh 1:1–5"), ui_font(7.5), colors.ink, 82, 12);
        gc.SetBrush(wxBrush(colors.rule));
        gc.DrawRectangle(0, 35 * unit, page_width * unit, unit);
        text("JOHANNES", ui_font(6.5), colors.muted, 82, 44);
        text("1", body_font(13), colors.ink, 100, 54);
        // The prescribed-reading bar, a selected phrase, and verse numbers in the rubric colour.
        gc.SetBrush(wxBrush(colors.accent));
        gc.DrawRoundedRectangle(14 * unit, 80 * unit, 2.5 * unit, 47 * unit, unit);
        const auto body = body_font(8.5), number = body_font(6);
        double x = text("1", number, colors.verse, 24, 80);
        text(ui::utf8("I begynnelsen var Ordet,"), body, colors.ink, x + 2, 82);
        x = text(ui::utf8("och Ordet var "), body, colors.ink, 24, 97);
        gc.GetTextExtent(ui::utf8("hos Gud,"), &width, &height);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(colors.selection));
        gc.DrawRoundedRectangle((x - 3) * unit, 96 * unit, width + 6 * unit, height + 2 * unit, 3 * unit);
        text(ui::utf8("hos Gud,"), body, colors.ink, x, 97);
        x = text("2", number, colors.verse, 24, 110);
        x = text(ui::utf8("och Ordet var "), body, colors.ink, x + 2, 112);
        text("Gud.", body, colors.accent, x, 112);
        text(ui::utf8("Ordet finns inte i texten."), body, colors.muted, 24, 127);
        // The calendar's today ring and service markers.
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colors.today).Width(1.4 * unit)));
        gc.DrawRoundedRectangle(24 * unit, 148 * unit, 18 * unit, 14 * unit, 3 * unit);
        gc.SetPen(*wxTRANSPARENT_PEN);
        for (std::size_t i = 0; i < colors.services.size(); ++i) {
            gc.SetBrush(wxBrush(colors.services[i]));
            gc.DrawEllipse((54 + 12 * double(i)) * unit, 152 * unit, 6 * unit, 6 * unit);
        }
    }
};
} // namespace
void MainFrame::show_settings() {
    if (navigation_.current().screen == Screen::Settings)
        return;
    auto* page = new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    fill_settings(page);
    show_page(page, Screen::Settings, ui::utf8("Inställningar"));
}
void MainFrame::preview_voice(const std::string& language, wxWindow* button) {
    if (voice_preview_) {
        const bool same_language = voice_preview_language_ == language;
        stop_speech(false);
        if (same_language)
            return;
    }
    const auto& voice = settings_.speech_voices.at(language);
    if (espeak_voice_name(voice).empty())
        return;
    try {
        const auto verse = corpus_.verse(corpus_.source_for_language(language, "John"), "John", {3, 16});
        if (!verse)
            throw std::runtime_error("Provtexten Johannes 3:16 kunde inte läsas.");
        save_reading_position();
        stop_speech(false);
        // A preview has no reading cues or completion tracking and stays on the settings page.
        playback_.start({});
        speech_tracked_.reset();
        speech_->set_voice(voice, language);
        voice_preview_language_ = language;
        if (button)
            voice_preview_button_ = button;
        voice_preview_ = true;
        if (voice_preview_button_) {
            voice_preview_button_->SetLabel(ui::utf8("Stoppa provet"));
            voice_preview_button_->SetName(voice_preview_button_->GetLabel() + ", " +
                                           ui::speech_language_name(voice_preview_language_));
            voice_preview_button_->Refresh();
        }
        speech_->speak_batch({make_utterance(verse->text, language, speech_lexicon(language))});
        playback_timer_.Start(30);
        refresh_speech();
    } catch (const std::exception& error) {
        stop_speech(false);
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Prova rösten"), wxOK | wxICON_INFORMATION, this);
    }
}
void MainFrame::fill_settings(wxScrolledWindow* page) {
    page->DestroyChildren();
    auto* categories = new wxListBox(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                     wxLB_SINGLE | wxBORDER_NONE);
    categories->SetFont(ui_font());
    categories->SetName(ui::utf8("Inställningskategorier"));
    auto* sections = new wxSimplebook(page);
    sections->SetName(ui::utf8("Inställningar för vald kategori"));
    sections->SetMinSize({1, 1});
    wxPanel* content = nullptr;
    wxBoxSizer* entries = nullptr;
    std::vector<SettingsCategory> category_keys;
    const auto category = [&](SettingsCategory key) {
        const auto found = std::ranges::find_if(settings_categories_, [key](const auto& entry) {
            return entry.first == key;
        });
        const auto title = ui::utf8(found->second);
        category_keys.push_back(key);
        categories->Append(title);
        auto* section = new wxScrolledWindow(sections, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                             wxHSCROLL | wxVSCROLL | wxBORDER_NONE);
        section->SetName(title);
        section->SetScrollRate(FromDIP(zoomed(12)), FromDIP(zoomed(12)));
        content = new wxPanel(section);
        entries = new wxBoxSizer(wxVERTICAL);
        content->SetSizer(entries);
        entries->Add(ui::label(content, title, 18), 0, wxBOTTOM, FromDIP(zoomed(18)));
        auto* outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(content, 1, wxEXPAND | wxALL, FromDIP(zoomed(24)));
        section->SetSizer(outer);
        sections->AddPage(section, title);
    };
    category(SettingsCategory::Sources);
    const auto sources = corpus_.sources();
    std::map<std::string, wxString> source_details;
    for (const auto& source : sources) {
        bool old = false, nt = false, apocrypha = false;
        for (const auto& book : corpus_.source_books(source.code)) {
            if (corpus_.new_testament_book(book))
                nt = true;
            else if (corpus_.deuterocanonical_book(book))
                apocrypha = true;
            else
                old = true;
        }
        auto detail = ui::speech_language_name(source.language);
        detail[0] = wxToupper(detail[0]);
        for (const auto& [present, name] : {std::pair{old, "GT"}, {nt, "NT"}, {apocrypha, "apokryfer"}})
            if (present)
                detail += ui::utf8(" · ") + name;
        source_details.emplace(source.code, detail);
    }
    const auto source_stack = [&](const char* heading, std::vector<SourcePreference>& preferences,
                                  bool primary) {
        auto* column = new wxPanel(content);
        auto* stack = new wxBoxSizer(wxVERTICAL);
        column->SetSizer(stack);
        stack->Add(ui::label(column, ui::utf8(heading), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        stack->Add(ui::label(column, ui::utf8("Källor i prioritetsordning"), 10), 0, wxBOTTOM,
                   FromDIP(zoomed(12)));
        stack->Add(new SourceStack(column, preferences, sources, source_details, primary,
                                   [this, primary] {
                                       if (primary)
                                           stop_speech(false);
                                       apply_settings();
                                   }),
                   0, wxEXPAND);
        return column;
    };
    auto* columns = new wxBoxSizer(wxHORIZONTAL);
    columns->Add(source_stack("VÄNSTER TEXT – HUVUDTEXT", settings_.primary_sources, true), 1, wxEXPAND);
    columns->AddSpacer(FromDIP(zoomed(28)));
    columns->Add(source_stack("HÖGER TEXT – DELAD LÄSNING", settings_.secondary_sources, false), 1, wxEXPAND);
    entries->Add(columns, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(14)));
    entries->Add(
        ui::label(content, ui::utf8("Den första aktiverade källan med text i kapitlet används."), 10));
    entries->Add(ui::label(
        content, ui::utf8("Andra texten väljer en annan källa. Uppläsningen följer huvudtextens språk."),
        10));
    auto* authority =
        ui::label(content,
                  ui::utf8("Bokordning och referenser följer Orthodox Study Bible (OSB). "
                           "Översättningens egna versnummer visas inom parentes där de skiljer sig. "
                           "Text utan känd OSB-referens märks särskilt och ingår inte i uppläsningen."),
                  10);
    authority->Wrap(FromDIP(zoomed(440)));
    entries->Add(authority, 0, wxTOP, FromDIP(zoomed(8)));
    category(SettingsCategory::Import);
    add_content_loader(content, entries);
    const auto choices = [&](const char* heading, std::initializer_list<const char*> names, int chosen,
                             const std::function<void(int)>& choose) {
        entries->Add(ui::label(content, ui::utf8(heading), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        int i = 0;
        for (const auto* name : names) {
            auto* radio = new wxRadioButton(content, wxID_ANY, ui::utf8(name), wxDefaultPosition,
                                            wxDefaultSize, i ? 0 : wxRB_GROUP);
            radio->SetFont(ui_font());
            radio->SetValue(i == chosen);
            radio->Bind(wxEVT_RADIOBUTTON, [choose, i](wxCommandEvent&) {
                choose(i);
            });
            entries->Add(radio, 0, wxBOTTOM, FromDIP(zoomed(6)));
            ++i;
        }
        entries->AddSpacer(FromDIP(zoomed(18)));
    };
    const auto percent_choice = [&](const char* heading, std::span<const int> steps, int Settings::* setting,
                                    const std::function<wxString(int)>& format,
                                    const std::function<void(int)>& change) {
        entries->Add(ui::label(content, ui::utf8(heading), 10), 0, wxBOTTOM, FromDIP(zoomed(8)));
        auto* choice = new wxChoice(content, wxID_ANY);
        choice->SetName(ui::utf8(heading));
        choice->SetFont(ui_font());
        for (const auto value : steps)
            choice->Append(format(value));
        const auto sync = [this, choice, steps, setting] {
            const auto found = std::ranges::find(steps, settings_.*setting);
            choice->SetSelection(found == steps.end() ? wxNOT_FOUND : int(found - steps.begin()));
        };
        sync();
        choice->Bind(wxEVT_UPDATE_UI, [sync](wxUpdateUIEvent&) {
            sync();
        });
        choice->Bind(wxEVT_CHOICE, [steps, change](wxCommandEvent& event) {
            change(steps[std::size_t(event.GetSelection())]);
        });
        entries->Add(choice, 0, wxBOTTOM, FromDIP(zoomed(18)));
    };
    category(SettingsCategory::Appearance);
    percent_choice(
        "TEXTSTORLEK", Settings::text_zoom_steps, &Settings::text_zoom,
        [](int value) {
            return wxString::Format("%d %%", value);
        },
        [this](int value) {
            // Zoom rebuilds this page; finish the choice event before destroying its control.
            CallAfter([this, value] {
                settings_.text_zoom = value;
                apply_settings();
                if (auto* choice = wxWindow::FindWindowByName("TEXTSTORLEK", current_page()))
                    choice->SetFocus();
            });
        });
    // The scheme list beside a preview of its light and dark variants.
    entries->Add(ui::label(content, ui::utf8("FÄRGSCHEMA"), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
    auto* schemes = new wxBoxSizer(wxVERTICAL);
    auto* preview = new SchemePreview(content);
    for (std::size_t i = 0; i < scheme_names.size(); ++i) {
        auto* radio = new wxRadioButton(content, wxID_ANY, ui::utf8(scheme_names[i].name), wxDefaultPosition,
                                        wxDefaultSize, i ? 0 : wxRB_GROUP);
        radio->SetFont(ui_font());
        radio->SetValue(ColorScheme(i) == settings_.color_scheme);
        radio->Bind(wxEVT_RADIOBUTTON, [this, i](wxCommandEvent&) {
            settings_.color_scheme = ColorScheme(i);
            apply_settings();
        });
        schemes->Add(radio, 0, wxBOTTOM, FromDIP(zoomed(6)));
    }
    auto* scheme_row = new wxBoxSizer(wxHORIZONTAL);
    scheme_row->Add(schemes, 0, wxRIGHT, FromDIP(zoomed(28)));
    scheme_row->Add(preview);
    entries->Add(scheme_row);
    auto* appearance = static_cast<wxScrolledWindow*>(content->GetParent());
    appearance->Bind(wxEVT_SIZE, [this, appearance, schemes, scheme_row, preview](wxSizeEvent& event) {
        const int width = std::max(1, event.GetSize().x - FromDIP(zoomed(64)));
        const bool beside = width >= schemes->GetMinSize().x + FromDIP(zoomed(28)) + preview->pair_width();
        scheme_row->SetOrientation(beside ? wxHORIZONTAL : wxVERTICAL);
        scheme_row->GetItem(std::size_t{0})->SetFlag(beside ? wxRIGHT : wxBOTTOM);
        preview->fit_width(width);
        appearance->FitInside();
        event.Skip();
    });
    entries->AddSpacer(FromDIP(zoomed(18)));
    choices("TEMA", {"Systemets tema", "Ljust tema", "Mörkt tema"}, int(settings_.theme), [this](int i) {
        settings_.theme = static_cast<Theme>(i);
        apply_settings();
    });
    category(SettingsCategory::Calendar);
    const auto day_changed = [this] {
        refresh_day();
        apply_settings();
    };
    entries->Add(ui::label(content, ui::utf8("Kalendern styr fasta högtider. Påskdatumet är detsamma."), 10),
                 0, wxBOTTOM, FromDIP(zoomed(8)));
    choices("KALENDER", {"Gregoriansk / reviderad juliansk", "Juliansk"},
            settings_.calendar == CalendarStyle::Old, [this, day_changed](int i) {
                settings_.calendar = i ? CalendarStyle::Old : CalendarStyle::New;
                day_changed();
            });
    entries->Add(ui::label(content, ui::utf8("Läsordningen styr vilka bibeltexter som läses varje dag."), 10),
                 0, wxBOTTOM, FromDIP(zoomed(8)));
    choices("LÄSORDNING", {"Grekiska läsordningen", "Slaviska läsordningen"},
            settings_.tradition == Tradition::Slavic, [this, day_changed](int i) {
                settings_.tradition = i ? Tradition::Slavic : Tradition::Greek;
                day_changed();
            });
    category(SettingsCategory::Speech);
    percent_choice("UPPLÄSNINGSHASTIGHET", Settings::speech_rate_steps, &Settings::speech_rate,
                   ui::rate_label, [this](int value) {
                       set_speech_rate(value);
                   });
    std::string voice_error;
    for (const auto& setting : speech_languages) {
        const std::string language = setting.code;
        const auto name = ui::utf8(setting.name).Upper();
        std::vector<EspeakVoice> voices;
        try {
            auto& catalog = voice_catalog_[language];
            if (catalog.empty())
                catalog = Espeak::voices(language);
            voices = catalog;
            if (voices.empty())
                voice_error += "Inga eSpeak-röster hittades för " + std::string(setting.name) + ". ";
        } catch (const std::exception& error) {
            if (voice_error.find(error.what()) == std::string::npos)
                voice_error += std::string(error.what()) + " ";
        }
        auto chosen = std::ranges::find(voices, settings_.speech_voices.at(language), &EspeakVoice::id);
        const auto available_count = voices.size();
        if (chosen == voices.end()) {
            // Preserve the preference if its voice was removed or eSpeak is temporarily unavailable.
            voices.push_back({settings_.speech_voices.at(language),
                              settings_.speech_voices.at(language) + " (inte tillgänglig)"});
            chosen = std::prev(voices.end());
        }
        entries->Add(ui::label(content, name, 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        auto* voice_choice = new wxChoice(content, wxID_ANY);
        voice_choice->SetFont(ui_font());
        voice_choice->SetName(ui::utf8("Röst för uppläsning, ") + ui::speech_language_name(language));
        for (const auto& voice : voices)
            voice_choice->Append(ui::utf8(voice.name));
        voice_choice->SetSelection(int(chosen - voices.begin()));
        auto* voice_try =
            new wxButton(content, wxID_ANY,
                         ui::utf8(voice_preview_ && voice_preview_language_ == language ? "Stoppa provet"
                                                                                        : "Prova rösten"));
        voice_try->SetFont(ui_font());
        voice_try->SetName(voice_try->GetLabel() + ", " + ui::speech_language_name(language));
        voice_try->Bind(wxEVT_BUTTON, [this, language, voice_try](wxCommandEvent&) {
            preview_voice(language, voice_try);
        });
        const auto button_size = voice_try->GetBestSize();
        voice_try->SetMinSize({std::max(button_size.x, voice_try->GetTextExtent(ui::utf8("Stoppa provet")).x +
                                                           FromDIP(zoomed(28))),
                               button_size.y});
        voice_try->SetToolTip(ui::utf8("Läs Johannes 3:16 med den valda eSpeak-rösten."));
        voice_try->Enable(std::size_t(voice_choice->GetSelection()) < available_count &&
                          !espeak_voice_name(settings_.speech_voices.at(language)).empty());
        if (voice_preview_language_ == language)
            voice_preview_button_ = voice_try;
        voice_choice->Bind(
            wxEVT_CHOICE, [this, language, voices, available_count, voice_try](wxCommandEvent& event) {
                if (voice_preview_)
                    stop_speech(false);
                settings_.speech_voices.at(language) = voices.at(std::size_t(event.GetSelection())).id;
                speech_->set_voice(settings_.speech_voices.at(language), language);
                voice_try->Enable(std::size_t(event.GetSelection()) < available_count &&
                                  !espeak_voice_name(settings_.speech_voices.at(language)).empty());
                apply_settings();
            });
        auto* voice_row = new wxBoxSizer(wxHORIZONTAL);
        voice_row->Add(voice_choice, 1, wxALIGN_CENTER_VERTICAL);
        voice_row->Add(voice_try, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(12)));
        entries->Add(voice_row, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(18)));
    }
    // eSpeak NG is the user's own installation; Perikop never ships it.
    if (!voice_error.empty()) {
        entries->Add(ui::label(content, ui::utf8(voice_error), 10));
#if defined(__APPLE__)
        const wxString guide = "https://formulae.brew.sh/formula/espeak-ng";
#else
        const wxString guide =
            "https://github.com/espeak-ng/espeak-ng/blob/master/docs/guide.md#installation";
#endif
        auto* espeak_link =
            new wxHyperlinkCtrl(content, wxID_ANY, ui::utf8("Så installerar du eSpeak NG"), guide);
        espeak_link->SetFont(ui_font(10));
        espeak_link->SetToolTip(guide);
        entries->Add(espeak_link, 0, wxBOTTOM, FromDIP(zoomed(18)));
    }
    auto* highlight = new wxCheckBox(content, wxID_ANY, ui::utf8("Markera texten som läses upp"));
    highlight->SetFont(ui_font());
    highlight->SetValue(settings_.speech_highlight);
    highlight->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        settings_.speech_highlight = event.IsChecked();
        apply_settings();
    });
    entries->Add(highlight);
    entries->AddSpacer(FromDIP(zoomed(18)));
    category(SettingsCategory::Updates);
    add_update_controls(content, entries);
    choices("UPPDATERINGSKANAL", {"Utgåvor (rekommenderas)", "Nattbyggen (experimentella)"},
            settings_.update_channel == UpdateChannel::Nightly, [this](int i) {
                settings_.update_channel = i ? UpdateChannel::Nightly : UpdateChannel::Releases;
                apply_settings();
                // An offer from the other channel no longer applies.
                updater_->reset();
            });
    categories->SetMinSize({categories->GetBestSize().x + FromDIP(zoomed(20)), 1});
    const int selected = int(std::ranges::find(category_keys, settings_category_) - category_keys.begin());
    categories->SetSelection(selected);
    sections->SetSelection(selected);
    categories->Bind(wxEVT_LISTBOX, [this, sections, category_keys](wxCommandEvent& event) {
        if (voice_preview_)
            stop_speech(false);
        settings_category_ = category_keys.at(std::size_t(event.GetSelection()));
        sections->SetSelection(event.GetSelection());
    });
    auto* outer = new wxBoxSizer(wxHORIZONTAL);
    outer->Add(categories, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT, FromDIP(zoomed(16)));
    outer->Add(sections, 1, wxEXPAND);
    page->SetSizer(outer);
    ui::recolor(page, palette(settings_.theme));
    for (std::size_t i = 0; i < sections->GetPageCount(); ++i) {
        auto* section = static_cast<wxScrolledWindow*>(sections->GetPage(i));
        section->Bind(wxEVT_SIZE, [this, section](wxSizeEvent& event) {
            const int width = std::max(1, event.GetSize().x - FromDIP(zoomed(64)));
            const std::function<void(wxWindow*)> wrap = [&](wxWindow* parent) {
                for (auto* child : parent->GetChildren()) {
                    if (auto* label = wxDynamicCast(child, wxStaticText);
                        label && label != update_text_.get()) {
                        auto text = label->GetLabel();
                        text.Replace("\n", " ");
                        label->SetLabel(text);
                        const int available = parent->GetClientSize().x - label->GetPosition().x;
                        label->Wrap(std::max(1, std::min({width, available, FromDIP(zoomed(600))})));
                    } else
                        wrap(child);
                }
            };
            wrap(section->GetChildren().front());
            if (update_text_ && update_text_->GetParent()->GetParent() == section)
                show_update_status();
            section->FitInside();
            event.Skip();
        });
        section->FitInside();
        for (auto* child : section->GetChildren().front()->GetChildren())
            if (auto* link = wxDynamicCast(child, wxHyperlinkCtrl)) {
                link->SetNormalColour(palette(settings_.theme).accent);
                link->SetVisitedColour(palette(settings_.theme).accent);
                link->SetHoverColour(palette(settings_.theme).ink);
            }
    }
    page->Layout();
}
} // namespace ortho
