#include "ui/main_frame.hpp"
#include "core/reading_display.hpp"
#include "speech/reading_speech.hpp"
#include "ui/about.hpp"
#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/month_calendar.hpp"
#include "ui/pronunciation_review.hpp"
#include "ui/reading_tooltips.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <numeric>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/eventfilter.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/utils.h>
namespace ortho {
namespace {
wxString kind_label(ReadingKind kind) {
    switch (kind) {
    case ReadingKind::MorningPsalm:
        return "Morgon";
    case ReadingKind::Epistle:
        return "Epistel";
    case ReadingKind::Gospel:
        return "Evangelium";
    case ReadingKind::OldTestament:
        return "Gamla testamentet";
    case ReadingKind::Vespers:
        return "Vesper";
    case ReadingKind::EveningPsalm:
        return ui::utf8("Kväll");
    case ReadingKind::Matins:
        return "Ortros";
    case ReadingKind::Hours:
        return ui::utf8("Timböner");
    case ReadingKind::OtherService:
        return ui::utf8("Gudstjänst");
    }
    return {};
}
} // namespace
MainFrame::MainFrame(const CorpusDb& corpus, UserDb& user, CivilDate date,
                     const std::filesystem::path& resources, const std::filesystem::path& data,
                     bool diagnostic)
    : wxFrame(nullptr, wxID_ANY, "Perikop", wxDefaultPosition, wxSize(1120, 900)), corpus_(corpus),
      user_(user), resources_(resources), data_(data), diagnostic_(diagnostic), lectionary_(corpus),
      selected_(date), settings_(user.load()), playback_timer_(this) {
    text_zoom = settings_.text_zoom;
    color_scheme = settings_.color_scheme;
    SetMinSize(FromDIP(wxSize(520, 480)));
    root_ = new wxPanel(this);
    root_->SetFont(ui_font());
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* reader = new wxBoxSizer(wxHORIZONTAL);
    reader_sizer_ = reader;
    // Tab follows the order of creation: the toolbar, then the page, then the study pane.
    create_toolbar();
    scripture_ = new ScriptureView(root_, corpus);
    outer->Add(bar_, 0, wxEXPAND);
    outer->Add(reader, 1, wxEXPAND);
    reader->Add(scripture_, 3, wxEXPAND);
    readings_ =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    readings_->SetScrollRate(0, FromDIP(zoomed(12)));
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
        if (navigation_.auxiliary() && event.GetKeyCode() == WXK_ESCAPE) {
            back();
            return;
        }
        if (controls().stop && active_playback() && event.GetKeyCode() == WXK_ESCAPE) {
            stop_speech();
            return;
        }
        if (event.GetKeyCode() == WXK_SPACE && wxWindow::FindFocus() == scripture_) {
            play_or_pause();
            return;
        }
        // The drawn controls take every key to see Space and Return, so Tab is moved on here.
        auto* focus = wxWindow::FindFocus();
        if (event.GetKeyCode() == WXK_TAB && !event.ControlDown() && focus && focus->HasFlag(wxWANTS_CHARS)) {
            focus->Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward
                                              : wxNavigationKeyEvent::IsForward);
            return;
        }
        event.Skip();
    });
    initialize_speech();
    study_ = new StudyPanel(root_, corpus_, *speech_, [this](const std::string& language) {
        return speech_lexicon(language);
    });
    reader->Add(study_, 2, wxEXPAND);
    scripture_->on_word([this](const std::optional<ScriptureView::Word>& word) {
        if (!settings_.word_study)
            return;
        if (word)
            study_->show(*word, scripture_->base_source());
        else
            study_->clear();
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
    if (!selectable_date(date))
        return;
    if (date != selected_.date())
        clear_preparation();
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
    date_buttons_.clear();
    home_labels_.clear();
    home_rows_.clear();
    home_width_ = 0;
    entries_->Clear(true);
    entries_->AddSpacer(FromDIP(zoomed(48)));
    const auto wrapped = [this](const wxString& text, const wxFont& font, int beside = 0, long style = 0) {
        auto* label =
            new wxStaticText(home_content_, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, style);
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
        button->SetMinSize(FromDIP(zoomed(wxSize(28, 28))));
        button->SetToolTip(label);
        button->Enable(selectable_date(shift_date(selected_.date(), days)));
        date_buttons_.push_back(button);
        date_row->Add(button, 0, wxALIGN_CENTER_VERTICAL);
    };
    arrow(Symbol::Back, ui::utf8("Föregående dag"), -1);
    // A button, so the keyboard reaches the date between the arrows.
    auto* date_label = ui::button(
        home_content_, ui::utf8(date_swedish(selected_.date()) + (month_open_ ? "  ▴" : "  ▾")), [this] {
            // pick_date rebuilds this row, so defer destruction of the clicked control.
            CallAfter([this] {
                pick_date();
            });
        });
    date_label->SetFont(ui_font(13));
    date_label->SetName(ui::utf8("Välj datum, ") + ui::utf8(date_swedish(selected_.date())));
    date_label->SetToolTip(ui::utf8("Välj datum, Ctrl+D"));
    date_row->Add(date_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(zoomed(6)));
    arrow(Symbol::Next, ui::utf8("Nästa dag"), 1);
    entries_->Add(date_row, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(zoomed(12)));
    month_ = nullptr;
    if (month_open_) {
        month_ = new MonthCalendar(
            home_content_, selected_.date(),
            [this](CivilDate date) {
                return lectionary_.readings_with_variants(date, settings_.calendar, settings_.tradition);
            },
            [this](std::optional<CivilDate> date) {
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
        entries_->Add(month_, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(zoomed(20)));
    }
    if (!day_.day.cycle_title.empty())
        entries_->Add(wrapped(ui::utf8(day_.day.cycle_title), body_font(24), 0, wxALIGN_CENTER_HORIZONTAL), 0,
                      wxEXPAND | wxBOTTOM, FromDIP(zoomed(28)));
    entries_->Add(ui::label(home_content_, ui::utf8("LÄSNINGAR"), 10), 0, wxBOTTOM, FromDIP(zoomed(6)));
    if (day_.readings.empty())
        entries_->Add(wrapped(ui::utf8("Ingen daglig bibelläsning är föreskriven"), body_font(18)), 0,
                      wxBOTTOM, FromDIP(zoomed(20)));
    const auto completed = user_.completed();
    const auto add_row = [this](ReadingRow* row, const std::string& tooltip = "") {
        if (!tooltip.empty())
            row->SetToolTip(ui::utf8(tooltip));
        home_rows_.push_back(row);
        entries_->Add(row, 0, wxEXPAND);
    };
    const auto add_reading = [&](const Reading& reading, const std::string& church,
                                 const std::string& tooltip) {
        auto label =
            reading.service.empty() ? kind_label(reading.kind) : ui::utf8(service_label(reading.service));
        // "Composite" names no occasion; the reading's own references show that it is composed.
        if (!reading.occasion.empty() && reading.occasion != "Composite")
            label += ui::utf8(", " + swedish_title(reading.occasion).value_or(reading.occasion));
        if (!church.empty())
            label = ui::utf8(church + ", ") + label;
        if (!reading.can_open()) {
            add_row(new ReadingRow(home_content_, label, ui::utf8(reading.citation), {}),
                    "Sammansatt liturgisk läsning. Den sammanställda texten finns inte i bibelkorpusen.");
            return;
        }
        const auto localized = corpus_.localize(in_swedish(reading));
        const auto segments = localized.segments();
        auto preview = ui::reading_tooltip(corpus_, localized);
        if (!tooltip.empty())
            preview += (preview.empty() ? wxString{} : "\n\n") + ui::utf8(tooltip);
        const auto title = ui::utf8(passage_label(corpus_, segments));
        const auto key = day_key(reading);
        add_row(new ReadingRow(
                    home_content_, label, ui::utf8(passage_label(corpus_, segments, true)),
                    [this, reading, key, title] {
                        open_tracked(reading, {key, title});
                    },
                    completed.contains(key), title),
                preview.ToStdString(wxConvUTF8));
    };
    // Epistles, then Gospels, then the other services, each in their own order.
    std::vector<std::size_t> order(day_.readings.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, {}, [this](std::size_t i) {
        const auto& service = day_.readings[i].service;
        return service == "Epistle" ? 0 : service == "Gospel" ? 1 : 2;
    });
    for (const auto i : order) {
        // Where the Antiochian order differs, both churches' readings are named.
        std::string greek_note;
        bool differs = false;
        for (const auto& variant : day_.variants)
            if (variant.primary_index == i) {
                differs = true;
                if (!variant.reading)
                    greek_note = variant.explanation;
            }
        add_reading(day_.readings[i], differs ? "Grekisk" : "", greek_note);
        for (const auto& variant : day_.variants)
            if (variant.primary_index == i && variant.reading)
                add_reading(*variant.reading, "Antiokisk", variant.explanation);
    }
    for (const auto& variant : day_.variants)
        if (!variant.primary_index && variant.reading)
            add_reading(*variant.reading, "Antiokisk", variant.explanation);
    add_next_plan();
    layout_home();
    readings_->Scroll(0, 0);
    apply_settings(false);
    Layout();
}
void MainFrame::layout_home() {
    const int width = std::clamp(readings_->GetClientSize().x - FromDIP(zoomed(64)), FromDIP(zoomed(200)),
                                 FromDIP(zoomed(560)));
    home_content_->SetMinSize(wxSize(width, -1));
    home_content_->SetSize(wxSize(width, home_content_->GetSize().y));
    // Replacing a wrapped label at the same width makes wxWidgets skip wrapping it again.
    if (home_width_ != width) {
        home_width_ = width;
        for (const auto& [label, text, beside] : home_labels_) {
            label->SetLabel(text);
            label->Wrap(width - beside);
        }
        for (auto* row : home_rows_)
            row->wrap(width);
    }
    home_content_->InvalidateBestSize();
    readings_->FitInside();
}
void MainFrame::open_psalm() {
    // Psalm 23 in the 1917 Bible's Hebrew numbering, the Septuagint's Psalm 22.
    Reading psalm{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}};
    psalm.reference = "sv1917";
    open_reading(psalm);
}
void MainFrame::open_reading(const Reading& selected) {
    if (playback_.select_reading(*speech_)) {
        scripture_->follow_playback(false);
        refresh_speech();
    }
    show_reading(selected);
}
void MainFrame::show_reading(const Reading& selected) {
    close_pages();
    visible_reading_ = selected;
    // Lyssna is offered only where playback would find Swedish verses to read.
    try {
        reading_speech(corpus_, {in_swedish(selected)}, [](const std::string&) {
            return std::vector<Pronunciation>{};
        });
        speakable_ = true;
    } catch (const std::exception&) {
        speakable_ = false;
    }
    playback_.forget_location();
    scripture_->follow_playback(false);
    // Lectionary references use their reference edition's numbering; open them in the left pane's.
    const auto reading = corpus_.localize(in_swedish(selected));
    navigation_.show_root(Screen::Reader);
    present_navigation();
    study_->clear();
    update_study();
    reading_title_ = ui::utf8(passage_label(corpus_, reading.segments()));
    parts_.clear();
    const auto segments = reading.segments();
    for (std::size_t i = 0; i < segments.size(); ++i)
        parts_.push_back(wxString::Format(ui::utf8("Del %d: "), int(i + 1)) +
                         ui::utf8(passage_label(corpus_, {segments[i]})));
    part_ = 0;
    update_bar();
    scripture_->open(reading);
    scripture_->SetFocus();
}
Reading MainFrame::in_swedish(Reading reading) const {
    if (reading.source_override.empty()) {
        if (reading.reference.empty() && reading.base_language != "sv") {
            std::vector<Passage> parts;
            for (const auto& segment : reading.segments()) {
                const auto mapped =
                    corpus_.map_passage(corpus_.frame_source(reading.base_language, segment.book),
                                        corpus_.frame_source("sv", segment.book), segment);
                if (mapped.empty())
                    parts.push_back(segment);
                else
                    parts.insert(parts.end(), mapped.begin(), mapped.end());
            }
            reading.passage = parts.front();
            reading.additional.assign(parts.begin() + 1, parts.end());
        }
        reading.base_language = "sv";
    }
    return reading;
}
std::vector<Pronunciation> MainFrame::speech_lexicon(const std::string& language) const {
    auto result = corpus_.pronunciations(language);
    if (language == "sv" && pronunciation_review_) {
        try {
            const auto overrides = pronunciation_review_->overrides();
            result.insert(result.begin(), overrides.begin(), overrides.end());
        } catch (const std::exception& e) {
            wxLogWarning("Uttalskorrigeringar kunde inte läsas: %s", ui::utf8(e.what()));
        }
    }
    return result;
}
void MainFrame::review_pronunciation() {
    stop_speech();
    try {
        if (!pronunciation_review_)
            throw std::runtime_error(pronunciation_error_);
        show_pronunciation_review(this, corpus_, *pronunciation_review_, *speech_, resources_);
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8("Uttalsgranskning"), wxOK | wxICON_ERROR, this);
    }
    speech_->set_speed(settings_.speech_rate / 100.0);
    refresh_speech();
}
void MainFrame::apply_settings(bool persist) {
    color_scheme = settings_.color_scheme;
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
    if (text_zoom != settings_.text_zoom) {
        // Every page draws its fonts and spacing again at the new size.
        text_zoom = settings_.text_zoom;
        root_->SetFont(ui_font());
        for (auto* control : bar_->GetChildren())
            control->InvalidateBestSize();
        refresh_day();
        for (auto& [id, page] : pages_)
            switch (page.screen) {
            case Screen::Bible:
                static_cast<BiblePicker*>(page.window)->rebuild();
                break;
            case Screen::Plans:
                fill_plans(static_cast<wxScrolledWindow*>(page.window));
                break;
            case Screen::Settings:
                fill_settings(static_cast<wxScrolledWindow*>(page.window));
                break;
            case Screen::About: {
                auto* about = make_about_page(root_, settings_.theme, resources_);
                about->Show(page.window->IsShown());
                root_->GetSizer()->Replace(page.window, about);
                page.window->Destroy();
                page.window = about;
                break;
            }
            case Screen::Day:
            case Screen::Reader:
                break;
            }
        root_->Layout();
    }
    const auto colors = palette(settings_.theme);
    ui::recolor(root_, colors);
    for (auto* button : date_buttons_)
        button->apply(colors);
    for (auto* row : home_rows_)
        row->apply(colors);
    if (month_)
        month_->apply(colors);
    for (auto* button : {back_, play_, pause_, stop_, menu_})
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
    if (navigation_.auxiliary())
        return;
    const bool shown = settings_.word_study && navigation_.reader();
    if (!shown)
        scripture_->highlight_word(std::nullopt);
    study_->Show(shown);
    root_->Layout();
}
MainFrame::~MainFrame() {
    alt_tap_.reset();
    // A hidden menu bar is not the frame's to delete.
    if (GetMenuBar() != menu_bar_)
        delete menu_bar_;
    playback_timer_.Stop();
    speech_.reset();
}

} // namespace ortho
