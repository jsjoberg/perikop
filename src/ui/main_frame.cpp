#include "ui/main_frame.hpp"
#include "core/reading_display.hpp"
#include "ui/about.hpp"
#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/month_calendar.hpp"
#include "ui/pronunciation_review.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
namespace ortho {
namespace {
wxString kind_label(ReadingKind kind) {
    switch (kind) {
    case ReadingKind::MorningPsalm:
        return "MORGON";
    case ReadingKind::Epistle:
        return "EPISTEL";
    case ReadingKind::Gospel:
        return "EVANGELIUM";
    case ReadingKind::OldTestament:
        return "GAMLA TESTAMENTET";
    case ReadingKind::Vespers:
        return "VESPER";
    case ReadingKind::EveningPsalm:
        return ui::utf8("KVÄLL");
    case ReadingKind::Matins:
        return "MATUTIN";
    case ReadingKind::Hours:
        return ui::utf8("TIMBÖNER");
    case ReadingKind::OtherService:
        return ui::utf8("GUDSTJÄNST");
    }
    return {};
}
} // namespace
MainFrame::MainFrame(const CorpusDb& corpus, UserDb& user, CivilDate date,
                     const std::filesystem::path& resources)
    : wxFrame(nullptr, wxID_ANY, "Perikop", wxDefaultPosition, wxSize(1120, 900)), corpus_(corpus),
      user_(user), resources_(resources), lectionary_(corpus), selected_(date), settings_(user.load()),
      playback_timer_(this) {
    SetMinSize(FromDIP(wxSize(520, 480)));
    root_ = new wxPanel(this);
    root_->SetFont(ui_font());
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* reader = new wxBoxSizer(wxHORIZONTAL);
    reader_sizer_ = reader;
    scripture_ = new ScriptureView(root_, corpus);
    create_toolbar();
    outer->Add(bar_, 0, wxEXPAND);
    outer->Add(reader, 1, wxEXPAND);
    reader->Add(scripture_, 3, wxEXPAND);
    readings_ =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    readings_->SetScrollRate(0, FromDIP(12));
    // The margin is inside the page, so its scrollbar stays at the window edge.
    home_content_ = new wxPanel(readings_);
    entries_ = new wxBoxSizer(wxVERTICAL);
    home_content_->SetSizer(entries_);
    auto* page = new wxBoxSizer(wxVERTICAL);
    page->Add(home_content_, 0, wxALIGN_CENTER_HORIZONTAL);
    readings_->SetSizer(page);
    readings_->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        layout_home();
        event.Skip();
    });
    outer->Add(readings_, 1, wxEXPAND);
    make_menus();
    scripture_->on_release_follow([this] {
        refresh_speech();
    });
    scripture_->on_selection([this] {
        update_bar();
    });
    scripture_->on_return([this] {
        if (active_playback())
            follow_speech();
        else {
            scripture_->center_passage();
            scripture_->SetFocus();
        }
    });
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&) {
            refresh_speech();
        },
        playback_timer_.GetId());
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (page_ && event.GetKeyCode() == WXK_ESCAPE) {
            back();
            return;
        }
        if (active_playback() && event.GetKeyCode() == WXK_ESCAPE) {
            stop_speech();
            return;
        }
        if (event.GetKeyCode() == WXK_SPACE && wxWindow::FindFocus() == scripture_) {
            play_or_pause();
            return;
        }
        event.Skip();
    });
    initialize_speech();
    try {
        study_db_ = std::make_unique<StudyDb>(resources_ / "lexicon/study.db");
    } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
        // Word study is optional; without its database the panel stays empty.
    }
    study_ = new StudyPanel(root_, corpus_, study_db_.get(), *speech_, [this](const std::string& language) {
        return speech_lexicon(language);
    });
    reader->Add(study_, 2, wxEXPAND);
    scripture_->on_word([this](const ScriptureView::Word& word) {
        if (!settings_.word_study)
            return;
        scripture_->highlight_word(word);
        study_->show(word, scripture_->base_source(), scripture_->frame());
    });
    root_->SetSizer(outer);
    auto* frame_sizer = new wxBoxSizer(wxVERTICAL);
    frame_sizer->Add(root_, 1, wxEXPAND);
    SetSizer(frame_sizer);
    refresh_day();
    show_readings();
    apply_settings(false);
    const auto work = wxGetClientDisplayRect();
    const auto desired = FromDIP(wxSize(1120, 900));
    const int inset = FromDIP(48);
    SetSize(std::min(desired.x, work.width - inset), std::min(desired.y, work.height - inset));
    Center();
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& e) {
        if (settings_.theme == Theme::System)
            apply_settings(false);
        e.Skip();
    });
}
void MainFrame::select_day(CivilDate date) {
    selected_.select(date);
    show_readings();
    refresh_day();
}
void MainFrame::navigate(int days) {
    try {
        select_day(shift_date(selected_.date(), days));
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), "Datum", wxOK | wxICON_INFORMATION, this);
    }
}
void MainFrame::refresh_day() {
    day_ = lectionary_.readings_with_variants(selected_.date(), settings_.calendar, settings_.tradition);
    plan_tiles_.clear();
    date_buttons_.clear();
    home_labels_.clear();
    home_width_ = 0;
    entries_->Clear(true);
    entries_->AddSpacer(FromDIP(48));
    const auto wrapped = [this](const wxString& text, const wxFont& font, int beside = 0) {
        auto* label = new wxStaticText(home_content_, wxID_ANY, text);
        label->SetFont(font);
        home_labels_.push_back({label, text, beside});
        return label;
    };
    auto* date_row = new wxBoxSizer(wxHORIZONTAL);
    const auto arrow = [this, date_row](Symbol symbol, const wxString& label, int days) {
        auto* button = new SymbolButton(home_content_, symbol, label, [this, days] {
            // refresh_day rebuilds this row, so defer destruction of the clicked control.
            CallAfter([this, days] {
                navigate(days);
            });
        });
        button->SetMinSize(FromDIP(wxSize(28, 28)));
        button->SetToolTip(label);
        date_buttons_.push_back(button);
        date_row->Add(button, 0, wxALIGN_CENTER_VERTICAL);
    };
    arrow(Symbol::Back, ui::utf8("Föregående dag"), -1);
    auto* date_label = ui::label(
        home_content_, ui::utf8(date_swedish(selected_.date()) + (month_open_ ? "  ▴" : "  ▾")), 13);
    date_label->SetToolTip(ui::utf8("Välj datum"));
    date_label->SetCursor(wxCursor(wxCURSOR_HAND));
    date_label->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
        CallAfter([this] {
            pick_date();
        });
    });
    date_row->Add(date_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(6));
    arrow(Symbol::Next, ui::utf8("Nästa dag"), 1);
    date_row->AddStretchSpacer();
    date_row->Add(ui::button(home_content_, "Idag",
                             [this] {
                                 CallAfter([this] {
                                     select_day(local_civil_date());
                                 });
                             }),
                  0, wxALIGN_CENTER_VERTICAL);
    entries_->Add(date_row, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
    month_ = nullptr;
    if (month_open_) {
        month_ = new MonthCalendar(home_content_, selected_.date(), [this](std::optional<CivilDate> date) {
            // Choosing rebuilds the page and its calendar.
            CallAfter([this, date] {
                month_open_ = false;
                if (date)
                    select_day(*date);
                else
                    refresh_day();
            });
        });
        month_->on_resize([this] {
            readings_->Layout();
            layout_home();
        });
        entries_->Add(month_, 0, wxBOTTOM, FromDIP(20));
    }
    const auto& heading = day_.day.title;
    if (!heading.empty())
        entries_->Add(wrapped(ui::utf8(heading), body_font(18)), 0, wxBOTTOM, FromDIP(8));
    entries_->Add(wrapped(ui::utf8(settings_.tradition == Tradition::Slavic ? "Slavisk läsordning · rysk/OCA"
                                                                            : "Grekisk läsordning"),
                          body_font(13)),
                  0, wxBOTTOM, FromDIP(8));
    entries_->AddSpacer(FromDIP(28));
    entries_->Add(ui::label(home_content_, "FASTA", 10), 0, wxBOTTOM, FromDIP(6));
    const auto fast = day_.day.fasting;
    const auto fast_text = fast.period == FastPeriod::None
                               ? fasting_allowance_label(fast)
                               : fasting_period_label(fast.period) + " · " + fasting_allowance_label(fast);
    entries_->Add(wrapped(ui::utf8(fast_text), body_font(16)), 0, wxBOTTOM, FromDIP(6));
    if (const auto abstentions = fasting_abstentions(fast); !abstentions.empty())
        entries_->Add(wrapped(ui::utf8(abstentions), body_font(13)), 0, wxBOTTOM, FromDIP(6));
    if (!day_.day.commemorations.empty()) {
        entries_->Add(ui::label(home_content_, ui::utf8("ÅMINNELSER"), 10), 0, wxTOP | wxBOTTOM, FromDIP(12));
        for (const auto& name : day_.day.commemorations)
            entries_->Add(wrapped(ui::utf8("• " + name), body_font(13)), 0, wxBOTTOM, FromDIP(6));
    }
    entries_->AddSpacer(FromDIP(24));
    if (day_.readings.empty())
        entries_->Add(wrapped(ui::utf8("Ingen daglig bibelläsning är föreskriven"), body_font(18)), 0,
                      wxBOTTOM, FromDIP(20));
    const auto completed = user_.completed();
    const auto add_reading = [&](const Reading& reading, const std::string& jurisdiction = "") {
        if (!jurisdiction.empty())
            entries_->Add(wrapped(ui::utf8(jurisdiction), body_font(13)), 0, wxBOTTOM, FromDIP(6));
        const auto service =
            reading.service.empty() ? kind_label(reading.kind) : ui::utf8(service_label(reading.service));
        entries_->Add(wrapped(service, ui_font(10)), 0, wxBOTTOM, FromDIP(6));
        if (!reading.occasion.empty())
            entries_->Add(
                wrapped(ui::utf8(swedish_title(reading.occasion).value_or(reading.occasion)), body_font(13)),
                0, wxBOTTOM, FromDIP(6));
        if (!reading.can_open()) {
            entries_->Add(wrapped(ui::utf8(reading.citation), body_font(18)), 0, wxBOTTOM, FromDIP(8));
            entries_->Add(
                wrapped(
                    ui::utf8(
                        "Sammansatt liturgisk läsning. Den sammanställda texten finns inte i bibelkorpusen."),
                    body_font(13)),
                0, wxBOTTOM, FromDIP(28));
            return;
        }
        const auto title = ui::utf8(passage_label(corpus_, corpus_.localize(in_primary(reading)).segments()));
        const auto key = day_key(reading);
        const bool done = completed.contains(key);
        auto* open = ui::button(home_content_, ui::utf8("Läs"), [this, reading, key, title] {
            open_tracked(reading, {key, title});
        });
        const int gap = FromDIP(12);
        auto* passage =
            wrapped(done ? title + ui::utf8("  ✓") : title, body_font(20), open->GetBestSize().x + gap);
        if (done)
            passage->SetToolTip(ui::utf8("Läst"));
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(passage, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
        row->Add(open, 0, wxALIGN_CENTER_VERTICAL);
        entries_->Add(row, 0, wxBOTTOM, FromDIP(28));
    };
    const auto add_variant = [&](const DayReadings::Variant& variant) {
        entries_->Add(wrapped(ui::utf8(variant.explanation), body_font(13)), 0, wxBOTTOM, FromDIP(10));
        if (variant.reading)
            add_reading(*variant.reading, variant.jurisdiction);
        else
            entries_->Add(wrapped(ui::utf8(variant.jurisdiction), body_font(13)), 0, wxBOTTOM, FromDIP(28));
    };
    for (std::size_t i = 0; i < day_.readings.size(); ++i) {
        const bool differs = std::ranges::any_of(day_.variants, [i](const auto& variant) {
            return variant.primary_index == i;
        });
        add_reading(day_.readings[i], differs ? "Grekiska ärkestiftet i Amerika (GOA)" : "");
        for (const auto& variant : day_.variants)
            if (variant.primary_index == i)
                add_variant(variant);
    }
    for (const auto& variant : day_.variants)
        if (!variant.primary_index)
            add_variant(variant);
    add_plans();
    layout_home();
    readings_->Scroll(0, 0);
    apply_settings(false);
    Layout();
}
void MainFrame::layout_home() {
    const int width = std::clamp(readings_->GetClientSize().x - FromDIP(64), FromDIP(200), FromDIP(400));
    home_content_->SetMinSize(wxSize(width, -1));
    home_content_->SetSize(wxSize(width, home_content_->GetSize().y));
    // Replacing a wrapped label at the same width makes wxWidgets skip wrapping it again.
    if (home_width_ != width) {
        home_width_ = width;
        for (const auto& [label, text, beside] : home_labels_) {
            label->SetLabel(text);
            label->Wrap(width - beside);
        }
    }
    home_content_->InvalidateBestSize();
    readings_->FitInside();
}
void MainFrame::show_readings() {
    close_pages();
    // Leaving a start-page item whose end has been read offers to mark it.
    if (tracked_ && scripture_->IsShown() && part_ + 1 >= parts_.size() && scripture_->end_seen()) {
        const auto item = *tracked_;
        tracked_.reset();
        offer_completion(item, false);
    }
    tracked_.reset();
    scripture_->follow_playback(false);
    scripture_->Hide();
    root_->GetSizer()->Show(reader_sizer_, false);
    readings_->Show();
    update_study();
    update_bar();
}
void MainFrame::open_psalm() {
    open_reading({ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}});
}
void MainFrame::open_reading(const Reading& selected) {
    close_pages();
    visible_reading_ = selected;
    speech_view_.reset();
    scripture_->follow_playback(false);
    // Lectionary references use their reference edition's numbering; open them in the left pane's.
    const auto reading = corpus_.localize(in_primary(selected));
    readings_->Hide();
    root_->GetSizer()->Show(reader_sizer_, true);
    scripture_->Show();
    study_->clear();
    update_study();
    reading_title_ = ui::utf8(passage_label(corpus_, reading.segments()));
    parts_.clear();
    const auto segments = reading.segments();
    for (std::size_t i = 0; i < segments.size(); ++i)
        parts_.push_back(wxString::Format(ui::utf8("Del %d · "), int(i + 1)) +
                         ui::utf8(passage_label(corpus_, {segments[i]})));
    part_ = 0;
    update_bar();
    scripture_->open(reading);
    scripture_->SetFocus();
}
Reading MainFrame::in_primary(Reading reading) const {
    if (reading.source_override.empty()) {
        if (reading.reference.empty() && reading.base_language != settings_.primary) {
            std::vector<Passage> parts;
            for (const auto& segment : reading.segments()) {
                const auto mapped =
                    corpus_.map_passage(corpus_.frame_source(reading.base_language, segment.book),
                                        corpus_.frame_source(settings_.primary, segment.book), segment);
                if (mapped.empty())
                    parts.push_back(segment);
                else
                    parts.insert(parts.end(), mapped.begin(), mapped.end());
            }
            reading.passage = parts.front();
            reading.additional.assign(parts.begin() + 1, parts.end());
        }
        reading.base_language = settings_.primary;
    }
    return reading;
}
std::vector<Pronunciation> MainFrame::speech_lexicon(const std::string& language) const {
    auto result = corpus_.pronunciations(language);
    if (language == "sv" && pronunciation_review_) {
        const auto overrides = pronunciation_review_->overrides();
        result.insert(result.begin(), overrides.begin(), overrides.end());
    }
    return result;
}
void MainFrame::review_pronunciation() {
    stop_speech();
    try {
        show_pronunciation_review(this, corpus_, *pronunciation_review_, *speech_, resources_);
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8("Uttalsgranskning"), wxOK | wxICON_ERROR, this);
    }
    speech_->set_speed(settings_.speech_rate / 100.0);
    refresh_speech();
}
void MainFrame::show_page(wxWindow* page, const wxString& title) {
    if (page_) {
        page_->Hide();
        root_->GetSizer()->Detach(page_);
        page_history_.emplace_back(page_, page_title_);
    } else
        return_to_reader_ = scripture_->IsShown();
    page_ = page;
    page_title_ = title;
    scripture_->Hide();
    root_->GetSizer()->Show(reader_sizer_, false);
    readings_->Hide();
    study_->Hide();
    root_->GetSizer()->Add(page_, 1, wxEXPAND);
    update_bar();
    root_->Layout();
    page_->SetFocus();
}
void MainFrame::close_page() {
    if (!page_)
        return;
    root_->GetSizer()->Detach(page_);
    page_->Hide();
    page_->Destroy();
    page_ = nullptr;
    if (!page_history_.empty()) {
        auto previous = page_history_.back();
        page_history_.pop_back();
        page_ = previous.first;
        page_title_ = previous.second;
        root_->GetSizer()->Add(page_, 1, wxEXPAND);
        page_->Show();
        root_->Layout();
        page_->SetFocus();
        return;
    }
    root_->GetSizer()->Show(reader_sizer_, return_to_reader_);
    scripture_->Show(return_to_reader_);
    readings_->Show(!return_to_reader_);
    study_->Show(return_to_reader_ && settings_.word_study);
    root_->Layout();
}
void MainFrame::close_pages() {
    while (page_)
        close_page();
}
void MainFrame::back() {
    if (page_) {
        if (auto* picker = dynamic_cast<BiblePicker*>(page_); picker && picker->back())
            return;
        close_page();
        refresh_speech();
        update_bar();
        if (scripture_->IsShown())
            scripture_->SetFocus();
    } else
        show_readings();
}
void MainFrame::about() {
    if (page_ && page_title_ == "Om Perikop")
        return;
    show_page(make_about_page(root_, settings_.theme, resources_), "Om Perikop");
}
void MainFrame::browse_bible() {
    if (dynamic_cast<BiblePicker*>(page_))
        return;
    auto* page = new BiblePicker(root_, corpus_, settings_, [this](const Reading& reading) {
        close_pages();
        tracked_.reset();
        open_reading(reading);
    });
    show_page(page, "Bibel");
}
void MainFrame::pick_date() {
    if (page_)
        return;
    // From the reader, the calendar opens on the start page.
    month_open_ = !readings_->IsShown() || !month_open_;
    if (!readings_->IsShown())
        show_readings();
    refresh_day();
    if (month_)
        month_->SetFocus();
}
void MainFrame::apply_settings(bool persist) {
#if wxCHECK_VERSION(3, 3, 0)
    static std::optional<Theme> native_theme;
    if (native_theme != settings_.theme) {
        native_theme = settings_.theme;
        wxTheApp->SetAppearance(static_cast<wxApp::Appearance>(settings_.theme));
    }
#endif
    if (persist) {
        try {
            user_.save(settings_);
        } catch (const std::exception& e) {
            wxMessageBox(ui::utf8(e.what()), ui::utf8("Inställningar kunde inte sparas"), wxOK | wxICON_ERROR,
                         this);
        }
    }
    const auto colors = palette(settings_.theme);
    ui::recolor(root_, colors);
    for (auto* button : date_buttons_)
        button->apply(colors);
    if (month_)
        month_->apply(colors);
    for (auto* tile : plan_tiles_)
        tile->apply(colors);
    for (auto* button : {back_, play_, pause_, stop_})
        button->apply(colors);
    for (auto& [pane, button] : panes_)
        button->apply(colors);
    address_->apply(colors);
    scripture_->apply(settings_);
    study_->apply(settings_.theme);
    update_study();
    update_bar();
}
void MainFrame::update_study() {
    if (page_)
        return;
    const bool shown = settings_.word_study && scripture_->IsShown();
    if (!shown)
        scripture_->highlight_word(std::nullopt);
    study_->Show(shown);
    root_->Layout();
}
MainFrame::~MainFrame() {
    playback_timer_.Stop();
    speech_.reset();
}

} // namespace ortho
