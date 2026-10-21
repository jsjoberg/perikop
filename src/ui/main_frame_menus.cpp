#include "core/reading_display.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <span>
#include <wx/app.h>
#include <wx/display.h>
#include <wx/eventfilter.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/popupwin.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif
#ifdef __APPLE__
#include "ui/native_menu.hpp"
#endif
#ifdef __WXGTK__
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#endif
namespace ortho {
namespace {
// Mouse events do not bubble from child controls. Scope the filter to this frame,
// excluding dialogs and other windows, and act only on button release.
class NavigationInput final : public wxEventFilter {
public:
    NavigationInput(wxWindow* owner, std::function<void(bool)> navigate)
        : owner_(owner), navigate_(std::move(navigate)) {
        wxEvtHandler::AddFilter(this);
    }
    ~NavigationInput() override {
        wxEvtHandler::RemoveFilter(this);
    }
    int FilterEvent(wxEvent& event) override {
        const auto type = event.GetEventType();
        if (type != wxEVT_AUX1_UP && type != wxEVT_AUX2_UP)
            return Event_Skip;
        auto* window = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (!window || owner_->IsBeingDeleted() || wxGetTopLevelParent(window) != owner_)
            return Event_Skip;
        navigate_(type == wxEVT_AUX2_UP);
        return Event_Processed;
    }

private:
    wxWindow* owner_;
    std::function<void(bool)> navigate_;
};
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
    Faster,
    Slower,
    NormalRate,
    Review,
    CheckUpdates,
    AddBookmark
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
// Chrome's ⋮ menu: rows of commands, and rows with − and + that keep the menu open.
// Up and Down move between rows, Left and Right within one, Tab through every control.
class MenuPopup final : public wxPopupTransientWindow {
public:
    // fill adds the rows; it runs again when the rows are rebuilt.
    MenuPopup(wxWindow* owner, wxWindow* button, const Palette& colors, std::function<void(MenuPopup&)> fill,
              MenuPopup* parent = nullptr)
        : wxPopupTransientWindow(parent ? parent : owner, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS),
          owner_(owner), button_(button), colors_(colors), fill_(std::move(fill)), parent_(parent) {
        SetBackgroundColour(colors.rule);
        if (parent) {
            auto* scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                wxVSCROLL | wxBORDER_NONE);
            scroll->SetScrollRate(0, FromDIP(zoomed(24)));
            panel_ = scroll;
        } else
            panel_ = new wxPanel(this);
        panel_->SetBackgroundColour(colors.paper);
        panel_->SetForegroundColour(colors.ink);
        rows_ = new wxBoxSizer(wxVERTICAL);
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
        track_focus(row);
        row->Enable(enabled);
        if (!help.empty())
            row->SetToolTip(help);
        rows_->Add(row, 0, wxEXPAND);
        lines_.push_back({row});
    }
    void submenu(const wxString& label, std::unique_ptr<wxMenu> contents, const wxString& help) {
        submenu(label, std::shared_ptr<wxMenu>(std::move(contents)), help);
    }
    void submenu(const wxString& label, const std::shared_ptr<wxMenu>& menu, const wxString& help) {
        auto* row = new MenuRow(panel_, label, ui::utf8("›"), [this, menu] {
            open_submenu(menu);
        });
        row->apply(colors_);
        track_focus(row);
        row->SetToolTip(help);
        rows_->Add(row, 0, wxEXPAND);
        lines_.push_back({row});
        submenus_.emplace(row, menu);
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
            track_focus(button);
            button->Enable(can(i ? 1 : -1));
            (*buttons)[i] = button;
        }
        line->Add((*buttons)[0], 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(24)));
        line->Add(shown, 0, wxALIGN_CENTER_VERTICAL);
        line->Add((*buttons)[1], 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(8)));
        rows_->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(zoomed(2)));
        lines_.push_back({(*buttons)[0], (*buttons)[1]});
    }
    void open() {
        if (!built_) {
            build();
            built_ = true;
        }
        return_to_parent_ = false;
        dismissed_ = false;
        // No control is handed to Popup(): on macOS and GTK it would hold a handler that a rebuild destroys.
        Popup();
#ifdef __APPLE__
        make_key_window(*this);
#endif
        focus(0, 0);
        grab_keyboard();
    }
    void Dismiss() override {
        if (!submenu_open_)
            wxPopupTransientWindow::Dismiss();
    }
    bool ProcessLeftDown(wxMouseEvent& event) override {
        auto* window = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (parent_ && window)
            return_to_parent_ =
                parent_->GetScreenRect().Contains(window->ClientToScreen(event.GetPosition()));
        return false;
    }
#ifdef __WXMSW__
    bool MSWHandleMessage(WXLRESULT* result, WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override {
        if (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) {
            // wxMSW defers dismissal, but a submenu can restore this popup before
            // that callback runs. Recheck activation instead of closing a reopened menu.
            CallAfter([this] {
                auto* active = wxGetActiveWindow();
                if (dismissed_ || submenu_open_ || !IsShown() || active == this ||
                    (active && IsDescendant(active)))
                    return;
                DismissAndNotify();
                if (auto* owner = MSWGetOwner(); owner && wxGetActiveWindow() != owner)
                    ::SendMessage(static_cast<HWND>(owner->GetHandle()), WM_NCACTIVATE, FALSE, 0);
            });
            // NOLINTNEXTLINE(bugprone-parent-virtual-call): replace only the transient deactivation handler.
            return wxPopupWindow::MSWHandleMessage(result, message, wParam, lParam);
        }
        return wxPopupTransientWindow::MSWHandleMessage(result, message, wParam, lParam);
    }
#endif

    // Builds the rows again at the current text size, with the focus on the same control.
    void rebuild() {
        std::size_t line = 0, column = 0;
        for (std::size_t i = 0; i < lines_.size(); ++i)
            if (const auto at = std::ranges::find(lines_[i], wxWindow::FindFocus()); at != lines_[i].end()) {
                line = i;
                column = std::size_t(at - lines_[i].begin());
            }
        // The popup holds the focus while its controls are replaced, so it stays open.
        SetFocus();
        for (const auto& [row, popup] : children_) {
            (void)row;
            popup->Destroy();
        }
        children_.clear();
        rows_->Clear(false);
        panel_->DestroyChildren();
        focused_ = nullptr;
        lines_.clear();
        submenus_.clear();
        build();
        focus(line, column);
    }

private:
    void track_focus(wxWindow* control) {
        control->Bind(wxEVT_SET_FOCUS, [this, control](wxFocusEvent& event) {
            focused_ = control;
            event.Skip();
        });
        control->Bind(wxEVT_ENTER_WINDOW, [this, control](wxMouseEvent& event) {
            focused_ = control;
            event.Skip();
        });
    }
    void show_without_grab(bool show) {
        // NOLINTNEXTLINE(bugprone-parent-virtual-call): only the deepest submenu owns input grabs.
        wxPopupWindow::Show(show);
    }
    void finish(std::function<void()> action) {
        if (dismissed_)
            return;
        submenu_open_ = false;
        chosen_ = std::move(action);
        DismissAndNotify();
    }
    void resume(wxWindow* row) {
        CallAfter([this, row] {
            if (dismissed_)
                return;
            show_without_grab(false);
            Popup();
#ifdef __APPLE__
            make_key_window(*this);
#endif
            focused_ = row;
            row->SetFocus();
            grab_keyboard();
            submenu_open_ = false;
        });
    }
    void open_submenu(const std::shared_ptr<wxMenu>& contents) {
        if (submenu_open_)
            return;
        const auto entry = std::ranges::find_if(submenus_, [&](const auto& item) {
            return item.second == contents;
        });
        if (entry == submenus_.end())
            return;
        auto*& child = children_[entry->first];
        if (!child)
            child = new MenuPopup(
                owner_, entry->first, colors_,
                [contents, owner = owner_](MenuPopup& popup) {
                    for (const auto* item : contents->GetMenuItems()) {
                        if (item->IsSeparator()) {
                            popup.separator();
                        } else if (auto* nested = item->GetSubMenu()) {
                            popup.submenu(item->GetItemLabelText(), std::shared_ptr<wxMenu>(contents, nested),
                                          item->GetHelp());
                        } else {
                            popup.item(
                                item->GetItemLabelText(), {}, item->IsEnabled(),
                                [contents, owner, id = item->GetId()] {
                                    wxMenu* handler = nullptr;
                                    contents->FindItem(id, &handler);
                                    wxCommandEvent event(wxEVT_MENU, id);
                                    if (!handler || !handler->GetEventHandler()->ProcessEvent(event))
                                        owner->GetEventHandler()->ProcessEvent(event);
                                },
                                item->GetHelp());
                        }
                    }
                },
                this);
        Dismiss();
        submenu_open_ = true;
        show_without_grab(true);
        child->open();
    }
    // Opens with its right edge under the right edge of the button, as Chrome's does.
    void build() {
        panel_->SetFont(ui_font());
        rows_->AddSpacer(FromDIP(zoomed(6)));
        fill_(*this);
        rows_->AddSpacer(FromDIP(zoomed(6)));
        panel_->InvalidateBestSize();
        if (parent_) {
            const int display = wxDisplay::GetFromWindow(owner_);
            const auto area = wxDisplay(display == wxNOT_FOUND ? 0U : unsigned(display)).GetClientArea();
            auto size = rows_->CalcMin();
            if (size.y > area.height - 2 * FromDIP(1)) {
                size.x += FromDIP(16);
                size.y = area.height - 2 * FromDIP(1);
            }
            panel_->SetMinSize(size);
        }
        Fit();
        auto position = button_->ClientToScreen({button_->GetSize().x - GetSize().x, button_->GetSize().y});
        if (parent_) {
            const int display = wxDisplay::GetFromWindow(owner_);
            const auto area = wxDisplay(display == wxNOT_FOUND ? 0U : unsigned(display)).GetClientArea();
            static_cast<wxScrolledWindow*>(panel_)->FitInside();
            const auto parent = parent_->GetScreenRect();
            position = button_->ClientToScreen({button_->GetSize().x, 0});
            position.x = parent.GetRight() + 1;
            if (position.x + GetSize().x > area.GetRight())
                position.x = std::max(area.x, parent.x - GetSize().x);
            position.y = std::clamp(position.y, area.y, area.GetBottom() - GetSize().y + 1);
        }
#if defined(__WXGTK__) && defined(GDK_WINDOWING_WAYLAND)
        // Wayland places a popup only as it is shown, so a shown one moves by showing it again.
        if (IsShown() && position != GetPosition() && GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
            Hide();
            SetPosition(position);
            Show();
            grab_keyboard();
            return;
        }
#endif
        SetPosition(position);
    }
    void grab_keyboard() {
#ifdef __WXGTK__
        // wxGTK grabs only the pointer for a popup, so keys would still go to the focused main window.
        // wxGTK's ungrab on hiding releases this grab too.
        auto* widget = static_cast<GtkWidget*>(GetHandle());
        gdk_seat_grab(gdk_display_get_default_seat(gtk_widget_get_display(widget)),
                      gtk_widget_get_window(widget), GDK_SEAT_CAPABILITY_KEYBOARD, false, nullptr, nullptr,
                      nullptr, nullptr);
#endif
    }
    // That control if it is enabled, else the first enabled one.
    void focus(std::size_t line, std::size_t column) {
        if (line < lines_.size() && column < lines_[line].size() && lines_[line][column]->IsEnabled()) {
            focused_ = lines_[line][column];
            lines_[line][column]->SetFocus();
            return;
        }
        for (const auto& controls : lines_)
            for (auto* control : controls)
                if (control->IsEnabled()) {
                    focused_ = control;
                    control->SetFocus();
                    return;
                }
    }
    void OnDismiss() override {
        if (submenu_open_ || dismissed_)
            return;
        dismissed_ = true;
        // The chosen command runs once the menu is gone, so it can move the focus itself.
        if (parent_) {
            if (!chosen_ && return_to_parent_)
                parent_->resume(button_);
            else
                parent_->finish(chosen_);
            // Keep cancelled submenus for reuse until the root closes. Destroying
            // a Cocoa key window here would steal focus back from the parent.
            return;
        } else if (chosen_)
            owner_->CallAfter(chosen_);
        else
            button_->SetFocus();
        Destroy();
    }
    void key(wxKeyEvent& e) {
        const int code = e.GetKeyCode();
        if (code == WXK_ESCAPE || (parent_ && code == WXK_LEFT)) {
            return_to_parent_ = true;
            DismissAndNotify();
            return;
        }
        const bool rows = code == WXK_UP || code == WXK_DOWN, within = code == WXK_LEFT || code == WXK_RIGHT;
        if (!rows && !within && code != WXK_TAB) {
            e.Skip();
            return;
        }
        auto* focus = wxWindow::FindFocus();
        if (!std::ranges::any_of(lines_, [focus](const auto& line) {
                return std::ranges::find(line, focus) != line.end();
            }))
            focus = focused_;
        if (code == WXK_RIGHT)
            if (const auto submenu = submenus_.find(focus); submenu != submenus_.end()) {
                open_submenu(submenu->second);
                return;
            }
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
        focused_ = order[index];
        order[index]->SetFocus();
    }
    wxWindow* owner_;
    wxWindow* button_;
    Palette colors_;
    wxPanel* panel_;
    wxBoxSizer* rows_;
    std::function<void(MenuPopup&)> fill_;
    std::vector<std::vector<wxWindow*>> lines_;
    std::map<wxWindow*, std::shared_ptr<wxMenu>> submenus_;
    std::map<wxWindow*, MenuPopup*> children_;
    wxWindow* focused_ = nullptr;
    bool submenu_open_ = false, return_to_parent_ = false, built_ = false, dismissed_ = false;
    std::function<void()> chosen_;
    MenuPopup* parent_;
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
    bar.Enable(AddBookmark, navigation_.reader() && scripture_->marked_reading().has_value());
    const auto label = ui::utf8(policy.playback_label) + "\tCtrl+Shift+P";
    if (bar.GetLabel(Play) != label)
        bar.SetLabel(Play, label);
    bar.Enable(Play, policy.playback_action != PlaybackAction::None);
    bar.Enable(Stop, policy.stop);
    bar.Enable(wxID_BACKWARD, history_.can_back());
    bar.Enable(wxID_FORWARD, history_.can_forward());
    const int pane = settings_.word_study ? Study : settings_.split_view ? Parallel : Right;
    if (!bar.IsChecked(pane))
        bar.Check(pane, true);
    bar.Enable(Larger, step(zooms, settings_.text_zoom, 1) != settings_.text_zoom);
    bar.Enable(Smaller, step(zooms, settings_.text_zoom, -1) != settings_.text_zoom);
    bar.Enable(Faster, step(rates, settings_.speech_rate, 1) != settings_.speech_rate);
    bar.Enable(Slower, step(rates, settings_.speech_rate, -1) != settings_.speech_rate);
}
void MainFrame::make_menus() {
    auto* view = new wxMenu;
    view->AppendRadioItem(Right, "En text");
    view->AppendRadioItem(Study, "Ordstudium");
    view->AppendRadioItem(Parallel, ui::utf8("Delad läsning"));
    view->AppendSeparator();
    view->Append(Larger, ui::utf8("Större text\tCtrl++"));
    view->Append(Smaller, "Mindre text\tCtrl+-");
    view->Append(Normal, "Normal textstorlek\tCtrl+0");
    view->AppendSeparator();
    // macOS moves this item to the application menu, where Settings belongs.
    view->Append(wxID_PREFERENCES, ui::utf8("Inställningar…\tCtrl+,"));
    auto* go = new wxMenu;
#ifdef __WXOSX__
    go->Append(wxID_BACKWARD, "Tillbaka\tCtrl+[");
    go->Append(wxID_FORWARD, ui::utf8("Framåt\tCtrl+]"));
#else
    go->Append(wxID_BACKWARD, "Tillbaka\tAlt+Left");
    go->Append(wxID_FORWARD, ui::utf8("Framåt\tAlt+Right"));
#endif
    go->AppendSeparator();
    go->Append(Readings, ui::utf8("Dagens läsningar\tCtrl+Shift+L"),
               ui::utf8("Visa den valda dagens läsningar"));
    go->Append(Today, "Idag\tCtrl+Shift+T");
    go->Append(Previous, ui::utf8("Föregående dag\tCtrl+PageUp"));
    go->Append(Next, ui::utf8("Nästa dag\tCtrl+PageDown"));
    go->Append(Pick, ui::utf8("Välj datum…\tCtrl+Shift+D"), ui::utf8("Välj en dag i kalendern"));
    go->AppendSeparator();
    go->Append(Bible, ui::utf8("Gå till bibelställe…\tCtrl+L"),
               ui::utf8("Bläddra fram en bok, ett kapitel och verser"));
    go->Append(Plans, ui::utf8("Läsplaner"), ui::utf8("Visa alla läsplaner och hur långt du har läst"));
    auto* reading = new wxMenu;
    reading->Append(Play, "Lyssna\tCtrl+Shift+P",
                    ui::utf8("Läs upp den öppna läsningen, eller pausa och fortsätt"));
    reading->Append(Stop, "Stoppa\tCtrl+.", ui::utf8("Avsluta uppläsningen"));
    reading->AppendSeparator();
    reading->Append(Faster, "Snabbare");
    reading->Append(Slower, ui::utf8("Långsammare"));
    reading->Append(NormalRate, "Normal hastighet");
    reading->AppendSeparator();
    reading->Append(CheckUpdates, ui::utf8("Sök efter uppdateringar…"),
                    ui::utf8("Sök på den valda uppdateringskanalen"));
    reading->Append(Review, ui::utf8("Granska svenskt uttal…"),
                    ui::utf8("Lyssna på och rätta hur ord uttalas vid uppläsning"));
#ifdef __WXOSX__
    // macOS moves this item to the application menu, where About belongs, and Hjälp would be empty.
    view->Append(wxID_ABOUT, "Om Perikop");
#endif
    auto* bar = new wxMenuBar;
    bar->Append(view, "Visa");
    bar->Append(go, ui::utf8("Gå"));
    bar->Append(bookmark_menu(), ui::utf8("Bokmärken"));
    bar->Append(reading, ui::utf8("Uppläsning"));
#ifndef __WXOSX__
    // macOS has its own Quit with ⌘Q in the application menu.
    view->AppendSeparator();
    view->Append(wxID_EXIT, "Avsluta\tCtrl+Q");
    auto* help = new wxMenu;
    help->Append(wxID_ABOUT, "Om Perikop");
    bar->Append(help, ui::utf8("Hjälp"));
#endif
    menu_bar_ = bar;
#ifdef __WXOSX__
    SetMenuBar(bar);
#endif
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            show_updates();
        },
        CheckUpdates);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            if (navigation_.reader())
                if (const auto reading = scripture_->marked_reading())
                    add_bookmarks(*reading);
        },
        AddBookmark);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            back();
        },
        wxID_BACKWARD);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            forward();
        },
        wxID_FORWARD);
    navigation_input_ = std::make_unique<NavigationInput>(this, [this](bool ahead) {
        if (ahead)
            forward();
        else
            back();
    });
    sync_menu();
#ifndef __WXOSX__
    // Windows and Linux use the overflow menu and shortcuts without attaching a native bar.
    std::vector<wxAcceleratorEntry> entries;
    for (size_t i = 0; i < bar->GetMenuCount(); ++i)
        for (auto* item : bar->GetMenu(i)->GetMenuItems())
            if (const std::unique_ptr<wxAcceleratorEntry> accel{item->GetAccel()})
                entries.emplace_back(accel->GetFlags(), accel->GetKeyCode(), item->GetId());
    SetAcceleratorTable(wxAcceleratorTable(int(entries.size()), entries.data()));
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
        [this](wxCommandEvent&) {
            Close();
        },
        wxID_EXIT);
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
            if (id == Review) {
                review_pronunciation();
                return;
            }
            settings_.word_study = id == Study;
            settings_.split_view = id == Parallel;
            apply_settings();
        },
        Today, Review);
}
void MainFrame::show_menu() {
    sync_menu();
    const auto command = [this](int id) {
        return [this, id] {
            wxCommandEvent event(wxEVT_MENU, id);
            ProcessWindowEvent(event);
        };
    };
    auto* popup = new MenuPopup(this, menu_, palette(settings_.theme), [this, command](MenuPopup& popup) {
        const auto item = [&](int id) {
            const auto* entry = menu_bar_->FindItem(id);
            popup.item(entry->GetItemLabelText(), shortcut_of(entry->GetItemLabel()), entry->IsEnabled(),
                       command(id), entry->GetHelp());
        };
        for (const int id : {Readings, Bible, Pick, Plans})
            item(id);
        popup.submenu(ui::utf8("Bokmärken"), std::unique_ptr<wxMenu>(bookmark_menu()),
                      ui::utf8("Öppna och ta bort sparade bokmärken"));
        popup.separator();
        popup.stepper(
            "Textstorlek", "Mindre text", ui::utf8("Större text"),
            [this] {
                return wxString::Format("%d %%", settings_.text_zoom);
            },
            [this](int direction) {
                return step(zooms, settings_.text_zoom, direction) != settings_.text_zoom;
            },
            [command, &popup](int direction) {
                command(direction > 0 ? Larger : Smaller)();
                // The menu follows the new size too. The pressed button is replaced, so not from its handler.
                popup.CallAfter([&popup] {
                    popup.rebuild();
                });
            });
        popup.stepper(
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
        popup.separator();
        for (const int id : {Play, Stop})
            item(id);
        popup.separator();
        // macOS keeps these two in the application menu, outside the bar's own menus.
        popup.item(ui::utf8("Inställningar…"), shortcut_of("\tCtrl+,"), true, command(wxID_PREFERENCES),
                   ui::utf8("Färgschema, tema, kalender, läsordning och röst"));
        item(CheckUpdates);
        item(Review);
        popup.item("Om Perikop", {}, true, command(wxID_ABOUT), ui::utf8("Version, källor och licenser"));
        // Last, as in Chrome.
        popup.separator();
        popup.item("Avsluta", shortcut_of("\tCtrl+Q"), true, command(wxID_EXIT), "Avsluta Perikop");
    });
    popup->open();
}
bool MainFrame::smoke_menu() {
    int opened = 0, removed = 0;
    auto contents = std::make_shared<wxMenu>();
    auto* entry = contents->Append(wxID_ANY, "John 1:2–5");
    contents->Bind(
        wxEVT_MENU,
        [&](wxCommandEvent&) {
            ++opened;
        },
        entry->GetId());
    auto* deletion = new wxMenu;
    auto* remove = deletion->Append(wxID_ANY, "John 1:2–5");
    deletion->Bind(
        wxEVT_MENU,
        [&](wxCommandEvent&) {
            ++removed;
        },
        remove->GetId());
    contents->AppendSubMenu(deletion, ui::utf8("Ta bort bokmärke…"));
    wxWeakRef<MenuPopup> root =
        new MenuPopup(this, menu_, palette(settings_.theme), [contents](MenuPopup& popup) {
            popup.submenu(ui::utf8("Bokmärken"), contents, {});
        });
    const auto key = [](wxWindow* window, int code) {
        wxKeyEvent event(wxEVT_CHAR_HOOK);
        event.m_keyCode = code;
        window->GetEventHandler()->ProcessEvent(event);
    };
    const auto child_of = [](wxWindow* window) -> MenuPopup* {
        for (auto* child : window->GetChildren())
            if (auto* popup = dynamic_cast<MenuPopup*>(child); popup && popup->IsShown())
                return popup;
        return nullptr;
    };
    const auto alive = [&](const char* stage) {
        if (root)
            return true;
        std::cerr << "Submenu check failed: main menu closed " << stage << '\n';
        return false;
    };
    // Finish closing any modeless dialogs before testing popup focus.
    wxTheApp->Yield(true);
    root->open();
    wxTheApp->Yield(true);
    if (!alive("on opening"))
        return false;
    key(root.get(), WXK_RIGHT);
    wxTheApp->Yield(true);
    if (!alive("on opening Bookmarks"))
        return false;
    auto* child = child_of(root.get());
    bool ok = true;
    const auto check = [&](bool condition, const char* label) {
        if (!condition) {
            std::cerr << "Submenu check failed: " << label << '\n';
            ok = false;
        }
    };
    check(root->IsShown() && child && child->GetSize().x > FromDIP(100) &&
              !root->GetScreenRect().Intersects(child->GetScreenRect()),
          "parent and child visible beside each other");
    if (!ok) {
        std::cerr << "Root shown=" << root->IsShown() << " rect=" << root->GetPosition().x << ','
                  << root->GetPosition().y << ',' << root->GetSize().x << ',' << root->GetSize().y << '\n';
        if (child)
            std::cerr << "Child rect=" << child->GetPosition().x << ',' << child->GetPosition().y << ','
                      << child->GetSize().x << ',' << child->GetSize().y << '\n';
    }
    if (child) {
        key(child, WXK_DOWN);
        key(child, WXK_RIGHT);
        wxTheApp->Yield(true);
        if (!alive("on opening removal submenu"))
            return false;
        auto* nested = child_of(child);
        check(root->IsShown() && child->IsShown() && nested, "nested removal menu opens");
        if (nested) {
            key(nested, WXK_LEFT);
            wxTheApp->Yield(true);
            if (!alive("on returning from removal submenu"))
                return false;
            check(root->IsShown() && child->IsShown(), "Left returns to parent");
        }
        key(child, WXK_ESCAPE);
        wxTheApp->Yield(true);
        if (!alive("on returning to main menu"))
            return false;
        check(root->IsShown() && !child_of(root.get()), "Escape returns to main menu");
    }
    key(root.get(), WXK_RIGHT);
    child = child_of(root.get());
    if (child) {
        MenuRow* first = nullptr;
        for (auto* panel : child->GetChildren())
            for (auto* control : panel->GetChildren())
                if (auto* row = dynamic_cast<MenuRow*>(control); row && row->IsEnabled() && !first)
                    first = row;
        check(first != nullptr, "first bookmark row available");
        if (first)
            for (const auto type : {wxEVT_KEY_DOWN, wxEVT_KEY_UP}) {
                wxKeyEvent event(type);
                event.m_keyCode = WXK_RETURN;
                first->GetEventHandler()->ProcessEvent(event);
            }
        wxTheApp->Yield(true);
    } else {
        ok = false;
        key(root.get(), WXK_ESCAPE);
    }
    check(opened == 1 && removed == 0 && (!root || !root->IsShown()),
          "selection closes all menus and dispatches once");
    if (!ok)
        std::cerr << "Opened=" << opened << " removed=" << removed
                  << " root shown=" << bool(root && root->IsShown()) << '\n';
    return ok;
}
wxMenu* MainFrame::bookmark_menu() {
    auto* menu = new wxMenu;
    menu->Append(AddBookmark, ui::utf8("Lägg till bokmärke"), ui::utf8("Spara den markerade texten"));
    menu->Enable(AddBookmark, navigation_.reader() && scripture_->marked_reading().has_value());
    menu->AppendSeparator();
    const auto marks = user_.bookmarks(corpus_);
    if (marks.empty()) {
        menu->Append(wxID_ANY, ui::utf8("Inga bokmärken"))->Enable(false);
        return menu;
    }
    auto* remove = new wxMenu;
    for (const auto& mark : marks) {
        const auto label = ui::utf8(passage_label(corpus_, {mark.passage}));
        auto* entry = menu->Append(wxID_ANY, label, ui::utf8("Öppna det sparade bibelstället"));
        menu->Bind(
            wxEVT_MENU,
            [this, mark](wxCommandEvent&) {
                CallAfter([this, mark] {
                    open_bookmark(mark);
                });
            },
            entry->GetId());
        auto* deletion = remove->Append(wxID_ANY, label, ui::utf8("Ta bort bokmärket efter bekräftelse"));
        remove->Bind(
            wxEVT_MENU,
            [this, mark](wxCommandEvent&) {
                CallAfter([this, mark] {
                    remove_bookmark(mark);
                });
            },
            deletion->GetId());
    }
    menu->AppendSeparator();
    menu->AppendSubMenu(remove, ui::utf8("Ta bort bokmärke…"),
                        ui::utf8("Välj vilket bokmärke som ska tas bort"));
    return menu;
}
void MainFrame::refresh_bookmarks() {
    const int index = menu_bar_->FindMenu(ui::utf8("Bokmärken"));
    if (index != wxNOT_FOUND) {
        delete menu_bar_->Replace(std::size_t(index), bookmark_menu(), ui::utf8("Bokmärken"));
        menu_bar_->SetMenuLabel(std::size_t(index), ui::utf8("Bokmärken"));
    }
}
void MainFrame::add_bookmarks(const Reading& reading) {
    try {
        for (const auto& passage : reading.segments())
            user_.bookmark(reading.reference, passage);
        refresh_bookmarks();
    } catch (const std::exception& error) {
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Bokmärket kunde inte sparas"), wxOK | wxICON_ERROR,
                     this);
    }
}
void MainFrame::open_bookmark(const Bookmark& bookmark) {
    Reading reading{corpus_.new_testament_book(bookmark.passage.book) ? ReadingKind::Gospel
                                                                      : ReadingKind::OldTestament,
                    bookmark.passage};
    reading.reference = bookmark.source;
    open_reading(reading);
    scripture_->open_section(0);
    part_ = 0;
    const auto& passage = scripture_->reading().passage;
    scripture_->select_verses(passage.first, passage.last);
    scripture_->center_passage();
    update_bar();
    save_navigation();
}
void MainFrame::remove_bookmark(const Bookmark& bookmark) {
    wxMessageDialog question(this,
                             ui::utf8("Vill du ta bort bokmärket för ") +
                                 ui::utf8(passage_label(corpus_, {bookmark.passage})) + "?",
                             ui::utf8("Ta bort bokmärke"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    question.SetYesNoLabels(ui::utf8("Ta bort"), ui::utf8("Avbryt"));
    if (question.ShowModal() != wxID_YES)
        return;
    try {
        user_.remove_bookmark(bookmark.id);
        refresh_bookmarks();
    } catch (const std::exception& error) {
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Bokmärket kunde inte tas bort"), wxOK | wxICON_ERROR,
                     this);
    }
}
} // namespace ortho
