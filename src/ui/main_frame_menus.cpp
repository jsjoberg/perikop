#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <wx/menu.h>
namespace ortho {
void MainFrame::make_menus() {
    enum {
        Today = wxID_HIGHEST + 1,
        Previous,
        Next,
        Pick,
        New,
        Old,
        Antiochian,
        Greek,
        Slavic,
        Readings,
        Bible,
        System,
        Light,
        Dark,
        Left,
        Right = Left + 3,
        Larger = Right + 4,
        Smaller,
        Play,
        Stop,
        Rate,
        Review = Rate + 8,
        Alice,
        Bjorn,
        Study,
        Highlight
    };
    const auto& languages = column_languages;
    static const char* names[] = {"Svenska", "Grekiska", "Engelska"};
    static const int rates[] = {25, 50, 75, 100, 125, 150, 175, 200};
    // Podcast style: 0,25×, 0,5× … 2×, with a Swedish decimal comma.
    const auto rate_label = [](int rate) {
        if (rate == 100)
            return wxString("Normal hastighet");
        wxString text = wxString::Format("%d,%02d", rate / 100, rate % 100);
        while (text.EndsWith("0"))
            text.RemoveLast();
        if (text.EndsWith(","))
            text.RemoveLast();
        return text + ui::utf8("×");
    };
    auto* calendar = new wxMenu;
    calendar->Append(Today, "Idag\tCtrl+T");
    calendar->Append(Previous, ui::utf8("Föregående dag\tCtrl+["));
    calendar->Append(Next, ui::utf8("Nästa dag\tCtrl+]"));
    calendar->Append(Pick, ui::utf8("Välj datum…\tCtrl+D"));
    calendar->AppendSeparator();
    calendar->AppendRadioItem(New, "Nya kalendern");
    calendar->AppendRadioItem(Old, "Gamla kalendern");
    calendar->Check(settings_.calendar == CalendarStyle::Old ? Old : New, true);
    calendar->AppendSeparator();
    // In Tradition order.
    calendar->AppendRadioItem(Antiochian, "Antiokiska läsordningen");
    calendar->AppendRadioItem(Greek, "Grekiska läsordningen");
    calendar->AppendRadioItem(Slavic, "Slaviska läsordningen");
    calendar->Check(Antiochian + int(settings_.tradition), true);
    auto* bible = new wxMenu;
    bible->Append(Readings, ui::utf8("Dagens läsningar\tCtrl+L"));
    bible->Append(Bible, ui::utf8("Gå till bibelställe…\tCtrl+G"));
    // Two panes: the left one is the text that is read aloud; the right one is optional.
    auto* view = new wxMenu;
    auto* left = new wxMenu;
    auto* right = new wxMenu;
    right->AppendRadioItem(Right, "Ingen");
    right->AppendRadioItem(Study, "Ordstudium");
    for (int i = 0; i < 3; ++i) {
        left->AppendRadioItem(Left + i, names[i]);
        if (settings_.primary == languages[i])
            left->Check(Left + i, true);
        right->AppendRadioItem(Right + 1 + i, names[i]);
        if (settings_.parallel == languages[i])
            right->Check(Right + 1 + i, true);
        right->Enable(Right + 1 + i, settings_.primary != languages[i]);
    }
    if (settings_.word_study)
        right->Check(Study, true);
    else if (settings_.parallel.empty())
        right->Check(Right, true);
    view->AppendSubMenu(left, ui::utf8("Vänster spalt"));
    view->AppendSubMenu(right, ui::utf8("Höger spalt"));
    view->AppendSeparator();
    view->AppendRadioItem(System, "Systemets tema");
    view->AppendRadioItem(Light, "Ljust tema");
    view->AppendRadioItem(Dark, ui::utf8("Mörkt tema"));
    view->Check(System + int(settings_.theme), true);
    view->AppendSeparator();
    view->Append(Larger, ui::utf8("Större text\tCtrl++"));
    view->Append(Smaller, "Mindre text\tCtrl+-");
    view->AppendSeparator();
    view->AppendCheckItem(Highlight, ui::utf8("Markera texten som läses upp"));
    view->Check(Highlight, settings_.speech_highlight);
    auto* reading = new wxMenu;
    reading->Append(Play, "Lyssna\tCtrl+P");
    reading->Append(Stop, "Stoppa\tCtrl+.");
    reading->AppendSeparator();
    play_item_ = Play;
    stop_item_ = Stop;
    right_item_ = Right;
    study_item_ = Study;
    for (int i = 0; i < 8; ++i) {
        reading->AppendRadioItem(Rate + i, rate_label(rates[i]));
        if (settings_.speech_rate == rates[i])
            reading->Check(Rate + i, true);
    }
    reading->AppendSeparator();
    // Swedish voice. Read-aloud is Swedish only.
    reading->AppendRadioItem(Alice, "Alice");
    reading->AppendRadioItem(Bjorn, ui::utf8("Björn"));
    reading->Check(settings_.speech_voice == "bjorn" ? Bjorn : Alice, true);
    reading->AppendSeparator();
    reading->Append(Review, ui::utf8("Granska svenskt uttal…"));
    auto* bar = new wxMenuBar;
    bar->Append(calendar, "Kalender");
    bar->Append(bible, "Bibel");
    bar->Append(view, "Visa");
    bar->Append(reading, ui::utf8("Uppläsning"));
#ifdef __WXOSX__
    // macOS moves this item to the application menu, where About belongs.
    calendar->Append(wxID_ABOUT, "Om Perikop");
#else
    auto* help = new wxMenu;
    help->Append(wxID_ABOUT, "Om Perikop");
    bar->Append(help, ui::utf8("Hjälp"));
#endif
    SetMenuBar(bar);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            about();
        },
        wxID_ABOUT);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& event) {
            const int id = event.GetId();
            if (page_) {
                if (id == Readings)
                    back();
                return;
            }
            if (id == Today) {
                select_day(local_civil_date());
                return;
            }
            if (id == Previous || id == Next) {
                navigate(id == Next ? 1 : -1);
                return;
            }
            if (id == Pick) {
                pick_date();
                return;
            }
            if (id == Readings) {
                show_readings();
                return;
            }
            if (id == Bible) {
                browse_bible();
                return;
            }
            if (id == Play) {
                play_or_pause();
                return;
            }
            if (id == Stop) {
                stop_speech();
                return;
            }
            if (id == Review) {
                review_pronunciation();
                return;
            }
            if (id == New || id == Old || (id >= Antiochian && id <= Slavic)) {
                if (id == New || id == Old)
                    settings_.calendar = id == Old ? CalendarStyle::Old : CalendarStyle::New;
                else
                    settings_.tradition = static_cast<Tradition>(id - Antiochian);
                if (!scripture_->IsShown())
                    refresh_day();
            } else if (id >= System && id <= Dark)
                settings_.theme = static_cast<Theme>(id - System);
            else if (id >= Left && id < Left + 3) {
                settings_.primary = languages[id - Left];
                if (settings_.parallel == settings_.primary)
                    settings_.parallel.clear();
                // apply_settings updates the right-column items to match.
                apply_settings();
                // The left pane decides the edition and its numbering, so reopen the text.
                if (scripture_->IsShown() && visible_reading_)
                    open_reading(*visible_reading_);
                return;
            } else if (id == Right) {
                settings_.parallel.clear();
                settings_.word_study = false;
            } else if (id == Study) {
                settings_.parallel.clear();
                settings_.word_study = true;
            } else if (id > Right && id <= Right + 3) {
                settings_.parallel = languages[id - Right - 1];
                settings_.word_study = false;
            } else if (id == Larger)
                settings_.font_size = std::min(28, settings_.font_size + 1);
            else if (id == Smaller)
                settings_.font_size = std::max(14, settings_.font_size - 1);
            else if (id == Highlight)
                settings_.speech_highlight = event.IsChecked();
            else if (id >= Rate && id < Rate + 8) {
                settings_.speech_rate = rates[id - Rate];
                speech_->set_speed(settings_.speech_rate / 100.0);
            } else if (id == Alice || id == Bjorn) {
                settings_.speech_voice = id == Bjorn ? "bjorn" : "alice";
                speech_->set_voice(settings_.speech_voice);
            } else {
                event.Skip();
                return;
            }
            apply_settings();
        },
        Today, Highlight);
}
} // namespace ortho
