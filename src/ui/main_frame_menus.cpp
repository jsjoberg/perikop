#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <memory>
#include <span>
#include <wx/eventfilter.h>
#include <wx/menu.h>
#include <wx/popupwin.h>
#include <wx/sizer.h>
#ifdef __APPLE__
#include "ui/native_menu.hpp"
#endif
#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif
namespace ortho {
namespace {
enum {
    Today = wxID_HIGHEST + 1,
    Previous,
    Next,
    Pick,
    Readings,
    Bible,
    Plans,
    Right,
    Study,
    Parallel,
    Larger = Parallel + 2,
    Smaller,
    Normal,
    Play,
    Stop,
    Prepare,
    Faster,
    Slower,
    NormalRate,
    Review
};
// Text sizes and speeds in percent, as Chrome steps its zoom.
const int zooms[] = {75, 80, 90, 100, 110, 125, 150, 175, 200};
const int rates[] = {25, 50, 75, 100, 125, 150, 175, 200};
// The next step up or down from a value, or the value itself at either end.
int step(std::span<const int> steps, int value, int direction) {
    if (direction > 0) {
        const auto above = std::ranges::upper_bound(steps, value);
        return above == steps.end() ? value : *above;
    }
    const auto below = std::ranges::lower_bound(steps, value);
    return below == steps.begin() ? value : *std::prev(below);
}
// Podcast style: 0,25×, 0,5× … 2×, with a Swedish decimal comma.
wxString rate_label(int rate) {
    wxString text = wxString::Format("%d,%02d", rate / 100, rate % 100);
    while (text.EndsWith("0"))
        text.RemoveLast();
    if (text.EndsWith(","))
        text.RemoveLast();
    return text + ui::utf8("×");
}
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
// Chrome's ⋮ menu: rows of commands, and rows with − and + that keep the menu open.
// Up and Down move between rows, Left and Right within one, Tab through every control.
class MenuPopup final : public wxPopupTransientWindow {
public:
    MenuPopup(wxWindow* owner, wxWindow* button, const Palette& colors)
        : wxPopupTransientWindow(owner, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS), owner_(owner),
          button_(button), colors_(colors) {
        SetBackgroundColour(colors.rule);
        panel_ = new wxPanel(this);
        panel_->SetBackgroundColour(colors.paper);
        panel_->SetForegroundColour(colors.ink);
        panel_->SetFont(ui_font());
        rows_ = new wxBoxSizer(wxVERTICAL);
        rows_->AddSpacer(FromDIP(zoomed(6)));
        panel_->SetSizer(rows_);
        auto* frame = new wxBoxSizer(wxVERTICAL);
        frame->Add(panel_, 1, wxEXPAND | wxALL, FromDIP(1));
        SetSizer(frame);
        Bind(wxEVT_CHAR_HOOK, &MenuPopup::key, this);
    }
    void item(const wxString& label, const wxString& shortcut, bool enabled, std::function<void()> action,
              const wxString& help = {}) {
        auto* row = new MenuRow(panel_, label, shortcut, [this, action = std::move(action)] {
            chosen_ = action;
            DismissAndNotify();
        });
        row->apply(colors_);
        row->Enable(enabled);
        if (!help.empty())
            row->SetToolTip(help);
        rows_->Add(row, 0, wxEXPAND);
        lines_.push_back({row});
    }
    void separator() {
        auto* rule = new wxPanel(panel_, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 1)));
        rule->SetBackgroundColour(colors_.rule);
        rows_->Add(rule, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(zoomed(6)));
    }
    // The value between − and +; can(±1) says whether a step is possible, step(±1) takes it.
    void stepper(const wxString& label, const wxString& less, const wxString& more,
                 const std::function<wxString()>& value, const std::function<bool(int)>& can,
                 const std::function<void(int)>& step) {
        auto* line = new wxBoxSizer(wxHORIZONTAL);
        line->Add(new wxStaticText(panel_, wxID_ANY, label), 1, wxALIGN_CENTER_VERTICAL | wxLEFT,
                  FromDIP(zoomed(16)));
        auto* shown = new wxStaticText(panel_, wxID_ANY, value(), wxDefaultPosition, wxDefaultSize,
                                       wxALIGN_CENTER_HORIZONTAL | wxST_NO_AUTORESIZE);
        shown->SetMinSize({shown->GetTextExtent(ui::utf8("200 %")).x + FromDIP(zoomed(8)), -1});
        auto buttons = std::make_shared<std::array<SymbolButton*, 2>>();
        const auto press = [=](int direction) {
            step(direction);
            shown->SetLabel(value());
            for (int i = 0; i < 2; ++i)
                (*buttons)[i]->Enable(can(i ? 1 : -1));
            // A button that reaches the end passes the focus to the other one.
            auto* pressed = (*buttons)[direction > 0];
            if (!pressed->IsEnabled())
                (*buttons)[direction < 0]->SetFocus();
        };
        for (int i = 0; i < 2; ++i) {
            auto* button = new SymbolButton(panel_, i ? Symbol::Plus : Symbol::Minus, i ? more : less, [=] {
                press(i ? 1 : -1);
            });
            button->SetToolTip(i ? more : less);
            button->apply(colors_);
            button->Enable(can(i ? 1 : -1));
            (*buttons)[i] = button;
        }
        line->Add((*buttons)[0], 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(24)));
        line->Add(shown, 0, wxALIGN_CENTER_VERTICAL);
        line->Add((*buttons)[1], 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(8)));
        rows_->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(zoomed(2)));
        lines_.push_back({(*buttons)[0], (*buttons)[1]});
    }
    // Opens with its right edge under the right edge of the button, as Chrome's does.
    void open() {
        rows_->AddSpacer(FromDIP(zoomed(6)));
        Fit();
        SetPosition(button_->ClientToScreen({button_->GetSize().x - GetSize().x, button_->GetSize().y}));
        wxWindow* first = nullptr;
        for (const auto& line : lines_)
            for (auto* control : line)
                if (!first && control->IsEnabled())
                    first = control;
        Popup(first);
#ifdef __APPLE__
        make_key_window(*this);
        if (first)
            first->SetFocus();
#endif
#ifdef __WXGTK__
        // wxGTK grabs only the pointer for a popup, so keys would still go to the focused main window.
        // wxGTK's ungrab on closing releases this grab too.
        auto* widget = static_cast<GtkWidget*>(GetHandle());
        gdk_seat_grab(gdk_display_get_default_seat(gtk_widget_get_display(widget)),
                      gtk_widget_get_window(widget), GDK_SEAT_CAPABILITY_KEYBOARD, false, nullptr, nullptr,
                      nullptr, nullptr);
#endif
    }

private:
    void OnDismiss() override {
        // The chosen command runs once the menu is gone, so it can move the focus itself.
        if (chosen_)
            owner_->CallAfter(chosen_);
        else
            button_->SetFocus();
        Destroy();
    }
    void key(wxKeyEvent& e) {
        const int code = e.GetKeyCode();
        if (code == WXK_ESCAPE) {
            DismissAndNotify();
            return;
        }
        const bool rows = code == WXK_UP || code == WXK_DOWN, within = code == WXK_LEFT || code == WXK_RIGHT;
        if (!rows && !within && code != WXK_TAB) {
            e.Skip();
            return;
        }
        auto* focus = wxWindow::FindFocus();
        // The controls the key moves between, in order: one per row, those of this row, or all.
        std::vector<wxWindow*> order;
        for (const auto& line : lines_) {
            const bool here = std::ranges::find(line, focus) != line.end();
            if (within && !here)
                continue;
            std::vector<wxWindow*> enabled;
            std::ranges::copy_if(line, std::back_inserter(enabled), &wxWindow::IsEnabled);
            if (enabled.empty())
                continue;
            if (rows)
                order.push_back(here ? focus : enabled.front());
            else
                order.insert(order.end(), enabled.begin(), enabled.end());
        }
        if (order.empty())
            return;
        const bool forward = code == WXK_DOWN || code == WXK_RIGHT || (code == WXK_TAB && !e.ShiftDown());
        const auto at = std::ranges::find(order, focus);
        std::size_t index =
            at == order.end() ? (forward ? order.size() - 1 : 0) : std::size_t(at - order.begin());
        index = (index + (forward ? 1 : order.size() - 1)) % order.size();
        order[index]->SetFocus();
    }
    wxWindow* owner_;
    wxWindow* button_;
    Palette colors_;
    wxPanel* panel_;
    wxBoxSizer* rows_;
    std::vector<std::vector<wxWindow*>> lines_;
    std::function<void()> chosen_;
};
// A menu label's shortcut, as the ⋮ menu writes it.
wxString shortcut_of(const wxString& label) {
    auto shortcut = label.AfterFirst('\t');
#ifdef __WXOSX__
    shortcut.Replace("Ctrl+", ui::utf8("⌘"));
#endif
    return shortcut;
}
} // namespace
void MainFrame::sync_menu() {
    auto& bar = *menu_bar_;
    const auto policy = controls();
    const auto label = ui::utf8(policy.playback_label) + "\tCtrl+P";
    if (bar.GetLabel(Play) != label)
        bar.SetLabel(Play, label);
    bar.Enable(Play, policy.playback_action != PlaybackAction::None);
    const auto stop_label =
        (preparing_day() ? ui::utf8("Avbryt förberedelsen") : wxString("Stoppa")) + "\tCtrl+.";
    if (bar.GetLabel(Stop) != stop_label)
        bar.SetLabel(Stop, stop_label);
    bar.Enable(Stop, policy.stop);
    bar.Enable(Prepare, policy.prepare);
    const auto help = ui::utf8("Spara ljud för " + date_swedish(selected_.date()) +
                               ". Välj morgondagens datum för att förbereda det i kväll.");
    if (bar.GetHelpString(Prepare) != help)
        bar.SetHelpString(Prepare, help);
    // Auxiliary pages suspend the commands for the day and the reading; size, speed and settings stay.
    for (const int id : std::initializer_list<int>{Today, Previous, Next, Pick, Readings, Bible, Plans, Right,
                                                   Study, Parallel, Parallel + 1, Review})
        bar.Enable(id, policy.menus);
    int pane = settings_.word_study ? Study : Right;
    for (int i = 0; i < 2; ++i)
        if (!settings_.word_study && settings_.parallel == parallel[i])
            pane = Parallel + i;
    if (!bar.IsChecked(pane))
        bar.Check(pane, true);
    bar.Enable(Larger, step(zooms, settings_.text_zoom, 1) != settings_.text_zoom);
    bar.Enable(Smaller, step(zooms, settings_.text_zoom, -1) != settings_.text_zoom);
    bar.Enable(Faster, step(rates, settings_.speech_rate, 1) != settings_.speech_rate);
    bar.Enable(Slower, step(rates, settings_.speech_rate, -1) != settings_.speech_rate);
}
void MainFrame::make_menus() {
    auto* view = new wxMenu;
    view->AppendRadioItem(Right, "Bara svenska");
    view->AppendRadioItem(Study, "Svenska och ordstudium");
    for (int i = 0; i < 2; ++i)
        view->AppendRadioItem(Parallel + i, parallel_names[i]);
    view->AppendSeparator();
    view->Append(Larger, ui::utf8("Större text\tCtrl++"));
    view->Append(Smaller, "Mindre text\tCtrl+-");
    view->Append(Normal, "Normal textstorlek\tCtrl+0");
    view->AppendSeparator();
    // macOS moves this item to the application menu, where Settings belongs.
    view->Append(wxID_PREFERENCES, ui::utf8("Inställningar…\tCtrl+,"));
    auto* go = new wxMenu;
    go->Append(Readings, ui::utf8("Dagens läsningar\tCtrl+L"));
    go->Append(Today, "Idag\tCtrl+T");
    go->Append(Previous, ui::utf8("Föregående dag\tCtrl+["));
    go->Append(Next, ui::utf8("Nästa dag\tCtrl+]"));
    go->Append(Pick, ui::utf8("Välj datum…\tCtrl+D"));
    go->AppendSeparator();
    go->Append(Bible, ui::utf8("Gå till bibelställe…\tCtrl+G"));
    go->Append(Plans, ui::utf8("Läsplaner"));
    auto* reading = new wxMenu;
    reading->Append(Play, "Lyssna\tCtrl+P");
    reading->Append(Stop, "Stoppa\tCtrl+.");
    reading->AppendSeparator();
    reading->Append(Faster, "Snabbare");
    reading->Append(Slower, ui::utf8("Långsammare"));
    reading->Append(NormalRate, "Normal hastighet");
    reading->AppendSeparator();
    reading->Append(Prepare, ui::utf8("Förbered ljud för vald dag"));
    reading->Append(Review, ui::utf8("Granska svenskt uttal…"));
#ifdef __WXOSX__
    // macOS moves this item to the application menu, where About belongs, and Hjälp would be empty.
    view->Append(wxID_ABOUT, "Om Perikop");
#endif
    auto* bar = new wxMenuBar;
    bar->Append(view, "Visa");
    bar->Append(go, ui::utf8("Gå"));
    bar->Append(reading, ui::utf8("Uppläsning"));
#ifndef __WXOSX__
    auto* help = new wxMenu;
    help->Append(wxID_ABOUT, "Om Perikop");
    bar->Append(help, ui::utf8("Hjälp"));
#endif
    menu_bar_ = bar;
    SetMenuBar(bar);
    sync_menu();
#ifndef __WXOSX__
    // The ⋮ menu holds the same commands, so Windows and Linux hide the bar, as browsers do.
    // Shortcuts then come from an accelerator table instead of the bar.
    std::vector<wxAcceleratorEntry> entries;
    for (size_t i = 0; i < bar->GetMenuCount(); ++i)
        for (auto* item : bar->GetMenu(i)->GetMenuItems())
            if (const std::unique_ptr<wxAcceleratorEntry> accel{item->GetAccel()})
                entries.emplace_back(accel->GetFlags(), accel->GetKeyCode(), item->GetId());
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
        [this](wxCommandEvent&) {
            show_settings();
        },
        wxID_PREFERENCES);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& event) {
            const int id = event.GetId();
            if (id == Larger || id == Smaller || id == Normal) {
                settings_.text_zoom = id == Normal ? Settings{}.text_zoom
                                                   : step(zooms, settings_.text_zoom, id == Larger ? 1 : -1);
                apply_settings();
                return;
            }
            if (id == Faster || id == Slower || id == NormalRate) {
                settings_.speech_rate = id == NormalRate
                                            ? Settings{}.speech_rate
                                            : step(rates, settings_.speech_rate, id == Faster ? 1 : -1);
                speech_->set_speed(settings_.speech_rate / 100.0);
                apply_settings();
                return;
            }
            if (navigation_.auxiliary()) {
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
                if (controls().stop)
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
            settings_.word_study = id == Study;
            settings_.parallel = id >= Parallel ? parallel[id - Parallel] : "";
            apply_settings();
        },
        Today, Review);
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
    sync_menu();
    auto* popup = new MenuPopup(this, menu_, palette(settings_.theme));
    const auto command = [this](int id) {
        return [this, id] {
            wxCommandEvent event(wxEVT_MENU, id);
            ProcessWindowEvent(event);
        };
    };
    const auto item = [&](int id) {
        const auto* entry = menu_bar_->FindItem(id);
        popup->item(entry->GetItemLabelText(), shortcut_of(entry->GetItemLabel()), entry->IsEnabled(),
                    command(id), entry->GetHelp());
    };
    for (const int id : {Readings, Bible, Pick, Plans})
        item(id);
    popup->separator();
    popup->stepper(
        "Textstorlek", "Mindre text", ui::utf8("Större text"),
        [this] {
            return wxString::Format("%d %%", settings_.text_zoom);
        },
        [this](int direction) {
            return step(zooms, settings_.text_zoom, direction) != settings_.text_zoom;
        },
        [command](int direction) {
            command(direction > 0 ? Larger : Smaller)();
        });
    popup->stepper(
        "Hastighet", ui::utf8("Långsammare"), "Snabbare",
        [this] {
            return rate_label(settings_.speech_rate);
        },
        [this](int direction) {
            return step(rates, settings_.speech_rate, direction) != settings_.speech_rate;
        },
        [command](int direction) {
            command(direction > 0 ? Faster : Slower)();
        });
    popup->separator();
    for (const int id : {Play, Stop, Prepare})
        item(id);
    popup->separator();
    // macOS keeps these two in the application menu, outside the bar's own menus.
    popup->item(ui::utf8("Inställningar…"), shortcut_of("\tCtrl+,"), true, command(wxID_PREFERENCES));
    item(Review);
    popup->item("Om Perikop", {}, true, command(wxID_ABOUT));
    popup->open();
}
} // namespace ortho
