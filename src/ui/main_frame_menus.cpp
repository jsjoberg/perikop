#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <memory>
#include <wx/dcclient.h>
#include <wx/eventfilter.h>
#include <wx/menu.h>
#include <wx/settings.h>
#ifdef __APPLE__
#include "ui/native_menu.hpp"
#endif
namespace ortho {
namespace {
enum {
    Today = wxID_HIGHEST + 1,
    Previous,
    Next,
    Pick,
    Gregorian,
    Julian,
    Greek,
    Slavic,
    Readings,
    Bible,
    System,
    Light,
    Dark,
    Right,
    Larger = Right + 3,
    Smaller,
    Normal,
    Play,
    Stop,
    Rate,
    Review = Rate + 8,
    Alice,
    Bjorn,
    Study,
    Highlight,
    Prepare,
    Plans
};
const int rates[] = {25, 50, 75, 100, 125, 150, 175, 200};
// The left column is Swedish. Beside it: nothing, Ordstudium, or one of these languages.
const char* const parallel[] = {"el", "en"};
const char* const parallel_names[] = {"Svenska och grekiska", "Svenska och engelska"};
#ifndef __WXOSX__
// Alt alone, as in browsers, shows or hides the menu bar. Alt with another key or a click does not.
class AltTap final : public wxEventFilter {
public:
    explicit AltTap(std::function<void()> toggle) : toggle_(std::move(toggle)) {
        wxEvtHandler::AddFilter(this);
    }
    ~AltTap() override {
        wxEvtHandler::RemoveFilter(this);
    }
    int FilterEvent(wxEvent& event) override {
        const auto type = event.GetEventType();
        if (type == wxEVT_KEY_DOWN) {
            // Windows reports AltGr, which Swedish keyboards use for @ and {, as Ctrl+Alt.
            const auto& key = static_cast<wxKeyEvent&>(event);
            tap_ = key.GetKeyCode() == WXK_ALT && !key.ControlDown();
        } else if (type == wxEVT_KEY_UP && static_cast<wxKeyEvent&>(event).GetKeyCode() == WXK_ALT) {
            if (tap_)
                toggle_();
            tap_ = false;
        } else if (type == wxEVT_LEFT_DOWN || type == wxEVT_RIGHT_DOWN)
            tap_ = false;
        return Event_Skip;
    }

private:
    std::function<void()> toggle_;
    bool tap_ = false;
};
#endif
} // namespace
std::vector<std::pair<wxMenu*, wxString>> MainFrame::menus() const {
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
    calendar->AppendRadioItem(Gregorian, "Gregoriansk / reviderad juliansk");
    calendar->AppendRadioItem(Julian, "Juliansk");
    calendar->Check(settings_.calendar == CalendarStyle::Old ? Julian : Gregorian, true);
    calendar->AppendSeparator();
    calendar->AppendRadioItem(Greek, "Grekiska läsordningen");
    calendar->AppendRadioItem(Slavic, "Slaviska läsordningen");
    calendar->Check(settings_.tradition == Tradition::Slavic ? Slavic : Greek, true);
    auto* bible = new wxMenu;
    bible->Append(Readings, ui::utf8("Dagens läsningar\tCtrl+L"));
    bible->Append(Bible, ui::utf8("Gå till bibelställe…\tCtrl+G"));
    bible->Append(Plans, ui::utf8("Läsplaner"));
    auto* view = new wxMenu;
    view->AppendRadioItem(Right, "Bara svenska");
    view->AppendRadioItem(Study, "Svenska och ordstudium");
    for (int i = 0; i < 2; ++i)
        view->AppendRadioItem(Right + 1 + i, parallel_names[i]);
    view->AppendSeparator();
    view->AppendRadioItem(System, "Systemets tema");
    view->AppendRadioItem(Light, "Ljust tema");
    view->AppendRadioItem(Dark, ui::utf8("Mörkt tema"));
    view->Check(System + int(settings_.theme), true);
    view->AppendSeparator();
    view->Append(Larger, ui::utf8("Större text\tCtrl++"));
    view->Append(Smaller, "Mindre text\tCtrl+-");
    view->Append(Normal, "Normal textstorlek\tCtrl+0");
    view->AppendSeparator();
    view->AppendCheckItem(Highlight, ui::utf8("Markera texten som läses upp"));
    view->Check(Highlight, settings_.speech_highlight);
    auto* reading = new wxMenu;
    reading->Append(Play, "Lyssna\tCtrl+P");
    reading->Append(Stop, "Stoppa\tCtrl+.");
    reading->Append(Prepare, ui::utf8("Förbered dagens ljud"));
    reading->AppendSeparator();
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
    std::vector<std::pair<wxMenu*, wxString>> result = {
        {calendar, "Kalender"}, {bible, "Bibel"}, {view, "Visa"}, {reading, ui::utf8("Uppläsning")}};
    for (auto& [menu, title] : result)
        sync_menu(*menu);
    return result;
}
void MainFrame::sync_menu(wxMenu& menu) const {
    const bool auxiliary = page_ != nullptr;
    const bool reader = scripture_->IsShown(), active = active_playback();
    const bool marked = reader && !scripture_->selections().empty();
    const bool paused = read_aloud_.playback().state == SpeechState::Paused;
    const wxString label = !active  ? (marked ? ui::utf8("Läs markering") : "Lyssna")
                           : paused ? ui::utf8("Fortsätt")
                                    : "Pausa";
    if (menu.FindItem(Play)) {
        if (menu.GetLabel(Play) != label + "\tCtrl+P")
            menu.SetLabel(Play, label + "\tCtrl+P");
        menu.Enable(Play, !auxiliary && reader);
        const bool preparing = preparing_day();
        const auto stop_label =
            (preparing ? ui::utf8("Avbryt förberedelsen") : wxString("Stoppa")) + "\tCtrl+.";
        if (menu.GetLabel(Stop) != stop_label)
            menu.SetLabel(Stop, stop_label);
        menu.Enable(Stop, !auxiliary && ((reader && active) || preparing));
        const bool available = std::ranges::any_of(day_.readings,
                                                   [](const Reading& reading) {
                                                       return reading.can_open();
                                                   }) ||
                               std::ranges::any_of(day_.variants, [](const DayReadings::Variant& variant) {
                                   return variant.reading && variant.reading->can_open();
                               });
        menu.Enable(Prepare, !auxiliary && !active && !preparing && available);
    }
    if (menu.FindItem(Right)) {
        int pane = settings_.word_study ? Study : Right;
        for (int i = 0; i < 2; ++i)
            if (!settings_.word_study && settings_.parallel == parallel[i])
                pane = Right + 1 + i;
        if (!menu.IsChecked(pane))
            menu.Check(pane, true);
    }
}
void MainFrame::make_menus() {
    auto* bar = new wxMenuBar;
    for (const auto& [menu, title] : menus())
        bar->Append(menu, title);
#ifdef __WXOSX__
    // macOS moves this item to the application menu, where About belongs.
    bar->GetMenu(0)->Append(wxID_ABOUT, "Om Perikop");
#else
    auto* help = new wxMenu;
    help->Append(wxID_ABOUT, "Om Perikop");
    bar->Append(help, ui::utf8("Hjälp"));
#endif
    menu_bar_ = bar;
    SetMenuBar(bar);
#ifndef __WXOSX__
    // The ⋮ menu holds the same commands, so Windows and Linux hide the bar, as browsers do.
    // Shortcuts then come from an accelerator table instead of the bar.
    std::vector<wxAcceleratorEntry> entries;
    const std::function<void(wxMenu*)> collect = [&](wxMenu* menu) {
        for (auto* item : menu->GetMenuItems()) {
            if (item->IsSubMenu())
                collect(item->GetSubMenu());
            else if (const std::unique_ptr<wxAcceleratorEntry> accel{item->GetAccel()})
                entries.emplace_back(accel->GetFlags(), accel->GetKeyCode(), item->GetId());
        }
    };
    for (size_t i = 0; i < bar->GetMenuCount(); ++i)
        collect(bar->GetMenu(i));
    accelerators_ = wxAcceleratorTable(int(entries.size()), entries.data());
    show_menu_bar(false);
    alt_tap_ = std::make_unique<AltTap>([this] {
        show_menu_bar(GetMenuBar() == nullptr);
    });
#endif
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
            if (id == Plans) {
                show_plans();
                return;
            }
            if (id == Play) {
                play_or_pause();
                return;
            }
            if (id == Stop) {
                if (scripture_->IsShown() || preparing_day())
                    stop_speech();
                return;
            }
            if (id == Prepare) {
                prepare_day();
                return;
            }
            if (id == Review) {
                review_pronunciation();
                return;
            }
            if (id == Gregorian || id == Julian || id == Greek || id == Slavic) {
                clear_preparation();
                if (id == Gregorian || id == Julian)
                    settings_.calendar = id == Julian ? CalendarStyle::Old : CalendarStyle::New;
                else
                    settings_.tradition = id == Slavic ? Tradition::Slavic : Tradition::Greek;
                if (!scripture_->IsShown())
                    refresh_day();
            } else if (id >= System && id <= Dark)
                settings_.theme = static_cast<Theme>(id - System);
            else if (id == Right) {
                settings_.parallel.clear();
                settings_.word_study = false;
            } else if (id == Study) {
                settings_.parallel.clear();
                settings_.word_study = true;
            } else if (id > Right && id <= Right + 2) {
                settings_.parallel = parallel[id - Right - 1];
                settings_.word_study = false;
            } else if (id == Larger)
                settings_.font_size = std::min(28, settings_.font_size + 1);
            else if (id == Smaller)
                settings_.font_size = std::max(14, settings_.font_size - 1);
            else if (id == Normal)
                settings_.font_size = Settings{}.font_size;
            else if (id == Highlight)
                settings_.speech_highlight = event.IsChecked();
            else if (id >= Rate && id < Rate + 8) {
                settings_.speech_rate = rates[id - Rate];
                speech_->set_speed(settings_.speech_rate / 100.0);
            } else if (id == Alice || id == Bjorn) {
                clear_preparation();
                settings_.speech_voice = id == Bjorn ? "bjorn" : "alice";
                speech_->set_voice(settings_.speech_voice);
            } else {
                event.Skip();
                return;
            }
            apply_settings();
        },
        Today, Plans);
}
void MainFrame::show_menu_bar(bool shown) {
#ifdef __WXOSX__
    (void)shown;
#else
    if ((GetMenuBar() != nullptr) == shown)
        return;
    // Detaching keeps the bar; the frame owns it again when it is shown.
    SetMenuBar(shown ? menu_bar_ : nullptr);
    SetAcceleratorTable(shown ? wxNullAcceleratorTable : accelerators_);
    if (shown)
        update_bar();
#endif
}
void MainFrame::show_menu() {
    // Chrome's ⋮ menu: every command of the menu bar, below the button at the right.
    wxMenu menu;
    for (const auto& [submenu, title] : menus()) {
        auto* item = menu.AppendSubMenu(submenu, title);
        menu.Enable(item->GetId(), page_ == nullptr);
    }
    menu.AppendSeparator();
    menu.Append(wxID_ABOUT, "Om Perikop");
    // Like Chrome's, the menu opens inside the window with its right edge under the button's.
    // wxWidgets places a popup by its top-left corner, so it needs the menu's width.
#ifdef __APPLE__
    const int width = native_menu_width(menu);
#else
    // An estimate from the menu font that errs wide: labels, shortcuts, check and submenu marks.
    wxClientDC dc(menu_);
    dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
    int labels = 0, shortcuts = 0;
    for (const auto* item : menu.GetMenuItems()) {
        labels = std::max(labels, dc.GetTextExtent(item->GetItemLabelText()).x);
        shortcuts = std::max(shortcuts, dc.GetTextExtent(item->GetItemLabel().AfterFirst('\t')).x);
    }
    const int width = labels + shortcuts + FromDIP(96);
#endif
    menu_->PopupMenu(&menu, menu_->GetSize().x - width, menu_->GetSize().y);
}
} // namespace ortho
