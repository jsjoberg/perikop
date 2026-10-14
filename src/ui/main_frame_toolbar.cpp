#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <wx/sizer.h>
namespace ortho {
void MainFrame::create_toolbar() {
    // One browser-like bar for the whole program: playback symbols, the address field,
    // the right-column symbols and the ⋮ menu. Controls that do not apply are dimmed, not hidden.
    bar_ = new wxPanel(root_);
    bar_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    bar_->Bind(wxEVT_PAINT, &MainFrame::paint_toolbar, this);
    auto* bar = new wxBoxSizer(wxHORIZONTAL);
    back_ = new SymbolButton(bar_, Symbol::Back, "Tillbaka", [this] {
        back();
    });
    back_->SetToolTip(ui::utf8("Tillbaka till dagens läsningar, Ctrl+L"));
    bar->Add(back_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    // Lyssna starts a reading or resumes a paused one; Pausa only pauses.
    play_ = new SymbolButton(bar_, Symbol::Play, "Lyssna", [this] {
        play_or_pause();
    });
    bar->Add(play_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    pause_ = new SymbolButton(bar_, Symbol::Pause, "Pausa", [this] {
        play_or_pause();
    });
    pause_->SetToolTip(ui::utf8("Pausa, mellanslag"));
    bar->Add(pause_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    stop_ = new SymbolButton(bar_, Symbol::Stop, "Stoppa", [this] {
        stop_speech();
    });
    stop_->SetToolTip(ui::utf8("Avsluta uppläsningen, Escape"));
    bar->Add(stop_, 0, wxALIGN_CENTER_VERTICAL);
    address_ = new AddressBar(bar_, [this] {
        address_clicked();
    });
    address_->SetMinSize(FromDIP(wxSize(80, 30)));
    bar->Add(address_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    // Symbols for the right column; pressing the chosen one again closes it.
    const std::pair<std::string, std::variant<Symbol, wxString>> panes[] = {
        {"study", Symbol::Study}, {"el", "GR"}, {"en", "EN"}};
    const char* names[] = {"Svenska och ordstudium", "Svenska och grekiska", "Svenska och engelska"};
    for (std::size_t i = 0; i < std::size(panes); ++i) {
        const auto& [pane, face] = panes[i];
        auto* button = new SymbolButton(
            bar_, face, names[i],
            [this, pane = pane] {
                toggle_pane(pane);
            },
            true);
        button->SetToolTip(ui::utf8(names[i]));
        bar->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(2));
        panes_.emplace_back(pane, button);
    }
    menu_ = new SymbolButton(bar_, Symbol::Menu, "Meny", [this] {
        show_menu();
    });
    menu_->SetToolTip(ui::utf8("Meny"));
    bar->Add(menu_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    auto* inset = new wxBoxSizer(wxVERTICAL);
    inset->Add(bar, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
    inset->Add(0, 0, 0, wxBOTTOM, FromDIP(1));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(inset, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(7));
    bar_->SetSizer(outer);
}
void MainFrame::paint_toolbar(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(bar_);
    const auto colors = palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));
    dc.Clear();
    const auto size = bar_->GetClientSize();
    dc.SetPen(wxPen(colors.rule, 1));
    dc.DrawLine(0, size.y - 1, size.x, size.y - 1);
}
void MainFrame::address_clicked() {
    if (controls().address_action == AddressAction::CloseBrowse) {
        close_page();
        refresh_speech();
        if (navigation_.reader())
            scripture_->SetFocus();
        return;
    }
    if (controls().address_action == AddressAction::ChoosePart) {
        enum { First = wxID_HIGHEST + 1000 };
        wxMenu menu;
        for (std::size_t i = 0; i < parts_.size(); ++i)
            menu.AppendRadioItem(First + int(i), parts_[i]);
        menu.Check(First + int(part_), true);
        const int chosen = address_->GetPopupMenuSelectionFromUser(menu, 0, address_->GetSize().y);
        if (chosen != wxID_NONE)
            select_part(std::size_t(chosen - First));
    } else
        browse_bible();
}
void MainFrame::select_part(size_t part) {
    part_ = part;
    playback_.forget_location();
    scripture_->follow_playback(false);
    scripture_->open_section(part);
    refresh_speech();
}
void MainFrame::toggle_pane(const std::string& pane) {
    if (navigation_.auxiliary())
        return;
    const bool study = pane == "study";
    const bool chosen = study ? settings_.word_study : settings_.parallel == pane;
    settings_.word_study = study && !chosen;
    settings_.parallel = study || chosen ? std::string{} : pane;
    apply_settings();
}
ReaderControls MainFrame::controls() const {
    const bool available = std::ranges::any_of(day_.readings,
                                               [](const Reading& reading) {
                                                   return reading.can_open();
                                               }) ||
                           std::ranges::any_of(day_.variants, [](const DayReadings::Variant& variant) {
                               return variant.reading && variant.reading->can_open();
                           });
    return reader_controls({.screen = navigation_.current().screen,
                            .playback = playback_.playback().state,
                            .preparing = preparing_day(),
                            .selection = navigation_.reader() && !scripture_->selections().empty(),
                            .several_parts = parts_.size() > 1,
                            .daily_readings = available});
}
void MainFrame::update_bar() {
    const auto policy = controls();
    const bool auxiliary = navigation_.auxiliary(), reader = navigation_.reader(),
               active = playback_.active();
    const bool preparing = preparing_day();
    const auto listen_label = ui::utf8(policy.listen_label);
    for (size_t i = 0; menu_bar_ && i < menu_bar_->GetMenuCount(); ++i)
        sync_menu(*menu_bar_->GetMenu(i));
    // A hidden bar keeps its items' state; only an attached one can dim a whole menu.
    if (menu_bar_ && GetMenuBar())
        for (size_t i = 0; i < menu_bar_->GetMenuCount(); ++i) {
            bool enabled = policy.menus;
#ifndef __WXOSX__
            // About stays available from auxiliary pages through the Help menu.
            enabled = enabled || menu_bar_->GetMenu(i)->FindItem(wxID_ABOUT);
#endif
            if (menu_bar_->IsEnabledTop(i) != enabled)
                menu_bar_->EnableTop(i, enabled);
        }
    const auto tooltip_of = [](wxWindow* control, const wxString& text) {
        if (control->GetToolTipText() != text)
            control->SetToolTip(text);
    };
    if (play_->GetLabel() != listen_label)
        play_->SetLabel(listen_label);
    tooltip_of(play_, listen_label + ui::utf8(", mellanslag"));
    play_->Enable(policy.listen);
    pause_->Enable(policy.pause);
    stop_->Enable(policy.stop);
    study_->allow_listening(policy.word_audio);
    tooltip_of(stop_, preparing ? ui::utf8("Avbryt förberedelsen, Ctrl+.") : ui::utf8("Stoppa, Ctrl+."));
    back_->Enable(policy.back);
    tooltip_of(back_,
               auxiliary ? ui::utf8("Tillbaka, Escape") : ui::utf8("Tillbaka till dagens läsningar, Ctrl+L"));
    const auto* picker =
        navigation_.current().screen == Screen::Bible ? static_cast<BiblePicker*>(current_page()) : nullptr;
    address_->Enable();
    for (auto& [pane, button] : panes_) {
        button->checked(pane == "study" ? settings_.word_study : settings_.parallel == pane);
        button->Enable(policy.columns);
    }
    scripture_->return_button(
        active   ? (scripture_->follows_playback() ? wxString{} : ui::utf8("Följ uppläsningen"))
        : reader ? ui::utf8("Till läsningen")
                 : wxString{},
        active);
    // One short line: what is shown, where the reading is, or what it is waiting for.
    const bool dropdown = policy.address_dropdown;
    wxString title, tooltip;
    if (picker) {
        const auto labels = picker->part_labels();
        title = labels.empty() ? current_page_title() : labels.back();
        tooltip = ui::utf8("Stäng bibelbläddraren och gå tillbaka");
    } else if (auxiliary)
        title = current_page_title();
    else if (playback_.prepared_day() && (preparing || !reader)) {
        const auto preparation = speech_->preparation();
        title = ui::utf8(preparation.status + ", " + date_swedish(*playback_.prepared_day()));
        tooltip = ui::utf8(preparation.status +
                           "\n\nFörbereder den valda dagens svenska läsningar och alternativ med den valda "
                           "rösten. Sparat ljud återanvänds vid uppläsning.");
    } else if (!reader)
        title = ui::utf8("Dagens läsningar");
    else if (const auto status = playback_.status(corpus_)) {
        title = ui::utf8(status->title);
        tooltip = ui::utf8(status->tooltip);
    } else
        title = dropdown ? parts_[std::min(part_, parts_.size() - 1)] : reading_title_;
    address_->show(title, dropdown);
    if (tooltip.empty())
        tooltip = ui::utf8(dropdown ? "Välj del av läsningen" : "Gå till bibelställe…, Ctrl+G");
    tooltip_of(address_, tooltip);
    const auto preparation = speech_ ? speech_->preparation() : SpeechPreparation{};
    address_->progress(preparing && preparation.total ? double(preparation.completed) / preparation.total
                       : reader && active             ? playback_.playback().progress
                                                      : 0);
}
} // namespace ortho
