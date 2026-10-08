#include "ui/main_frame.hpp"
#include "core/reading_display.hpp"
#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/date_picker.hpp"
#include "ui/pronunciation_review.hpp"
#include <algorithm>
#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/utils.h>
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
    scripture_ = new ScriptureView(root_, corpus);
    create_toolbar();
    outer->Add(bar_, 0, wxEXPAND);
    outer->Add(reader, 1, wxEXPAND);
    reader->Add(scripture_, 3, wxEXPAND);
    readings_ =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    readings_->SetScrollRate(0, FromDIP(12));
    // The margin is inside the page, so its scrollbar stays at the window edge.
    entries_ = new wxBoxSizer(wxVERTICAL);
    auto* page = new wxBoxSizer(wxVERTICAL);
    page->Add(entries_, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(68));
    readings_->SetSizer(page);
    outer->Add(readings_, 1, wxEXPAND);
    make_menus();
    scripture_->on_release_follow([this] {
        following_audio_ = false;
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
    SetSize(std::min(1120, work.width - 48), std::min(900, work.height - 48));
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
    day_ = lectionary_.readings_for(selected_.date(), settings_.calendar);
    plan_tiles_.clear();
    entries_->Clear(true);
    entries_->AddSpacer(FromDIP(48));
    entries_->Add(ui::label(readings_, ui::utf8(date_swedish(selected_.date())), 13), 0, wxBOTTOM,
                  FromDIP(8));
    // The annotation reads "Pascha … · dag N · title"; the separator is four UTF-8 bytes.
    const std::string separator = " · ";
    const auto info =
        day_.day.annotation.find(separator, day_.day.annotation.find(separator) + separator.size());
    if (info != std::string::npos) {
        auto* feast = new wxStaticText(readings_, wxID_ANY,
                                       ui::utf8(day_.day.annotation.substr(info + separator.size())));
        feast->SetFont(body_font(18));
        feast->Wrap(std::max(200, GetClientSize().x - FromDIP(150)));
        entries_->Add(feast, 0, wxBOTTOM, FromDIP(8));
    }
    entries_->AddSpacer(FromDIP(28));
    if (day_.readings.empty())
        entries_->Add(ui::label(readings_, ui::utf8("Ingen daglig bibelläsning är föreskriven"), 18), 0,
                      wxBOTTOM, FromDIP(20));
    const auto completed = user_.completed();
    for (const auto& reading : day_.readings) {
        auto* section = ui::label(readings_, kind_label(reading.kind), 10);
        entries_->Add(section, 0, wxBOTTOM, FromDIP(6));
        const auto title = ui::utf8(passage_label(corpus_, corpus_.localize(in_primary(reading)).segments()));
        const auto key = day_key(reading);
        const bool done = completed.contains(key);
        auto* open =
            ui::button(readings_, done ? title + ui::utf8("  ✓") : title, [this, reading, key, title] {
                open_tracked(reading, {key, title});
            });
        open->SetFont(body_font(20));
        if (done)
            open->SetToolTip(ui::utf8("Läst"));
        entries_->Add(open, 0, wxBOTTOM, FromDIP(32));
    }
    add_plans();
    readings_->FitInside();
    readings_->Scroll(0, 0);
    apply_settings(false);
    Layout();
}
void MainFrame::show_readings() {
    // Leaving a start-page item whose end has been read offers to mark it.
    if (tracked_ && scripture_->IsShown() && part_ + 1 >= parts_.size() && scripture_->end_seen()) {
        const auto item = *tracked_;
        tracked_.reset();
        offer_completion(item, false);
    }
    tracked_.reset();
    following_audio_ = false;
    scripture_->follow_playback(false);
    scripture_->Hide();
    readings_->Show();
    update_study();
    update_bar();
}
void MainFrame::open_psalm() {
    open_reading({ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}, "Psalm 23"});
}
void MainFrame::open_reading(const Reading& selected) {
    visible_reading_ = selected;
    following_audio_ = false;
    speech_view_.reset();
    scripture_->follow_playback(false);
    // Lectionary references use their reference edition's numbering; open them in the left pane's.
    const auto reading = corpus_.localize(in_primary(selected));
    readings_->Hide();
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
    if (reading.source_override.empty())
        reading.base_language = settings_.primary;
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
void MainFrame::browse_bible() {
    if (const auto reading = pick_bible_reading(this, corpus_, settings_)) {
        tracked_.reset();
        open_reading(*reading);
    }
}
void MainFrame::pick_date() {
    if (const auto date = pick_civil_date(this, selected_.date(), settings_.theme))
        select_day(*date);
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
