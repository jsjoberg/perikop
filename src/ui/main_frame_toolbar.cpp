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
    bar->Add(back_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(2)));
    forward_ = new SymbolButton(bar_, Symbol::Next, ui::utf8("Framåt"), [this] {
        forward();
    });
    bar->Add(forward_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(2)));
    // One focusable control changes its face and action with playback state.
    play_ = new SymbolButton(bar_, Symbol::Play, "Lyssna", [this] {
        play_or_pause();
    });
    bar->Add(play_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(2)));
    stop_ = new SymbolButton(bar_, Symbol::Stop, "Stoppa", [this] {
        stop_speech();
    });
    stop_->SetToolTip(ui::utf8("Stoppa och börja om, Ctrl+."));
    bar->Add(stop_, 0, wxALIGN_CENTER_VERTICAL);
    address_ = new AddressBar(bar_, [this] {
        address_clicked();
    });
    bar->Add(address_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(zoomed(10)));
    const std::pair<std::string, Symbol> panes[] = {{"study", Symbol::Study}, {"split", Symbol::Split}};
    const char* names[] = {"Ordstudium", "Delad läsning"};
    for (std::size_t i = 0; i < std::size(panes); ++i) {
        const auto& [pane, face] = panes[i];
        auto* button = new SymbolButton(
            bar_, face, ui::utf8(names[i]),
            [this, pane = pane] {
                toggle_pane(pane);
            },
            true);
        button->SetToolTip(ui::utf8(names[i]));
        bar->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(2)));
        panes_.emplace_back(pane, button);
    }
    menu_ = new SymbolButton(bar_, Symbol::Menu, "Meny", [this] {
        show_menu();
    });
    menu_->SetToolTip(ui::utf8("Meny"));
    bar->Add(menu_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(8)));
    auto* inset = new wxBoxSizer(wxVERTICAL);
    inset->Add(bar, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(zoomed(8)));
    inset->Add(0, 0, 0, wxBOTTOM, FromDIP(zoomed(1)));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(inset, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(zoomed(7)));
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
    pause_for_navigation();
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
    const bool chosen = study ? settings_.word_study : settings_.split_view;
    settings_.word_study = study && !chosen;
    settings_.split_view = !study && !chosen;
    apply_settings();
}
ReaderControls MainFrame::controls() const {
    auto result = reader_controls({.screen = navigation_.current().screen,
                                   .playback = playback_.playback().state,
                                   .selection = navigation_.reader() &&
                                                speech_language_supported(scripture_->primary_language()) &&
                                                !scripture_->selections().empty(),
                                   .several_parts = parts_.size() > 1,
                                   .speakable = speakable_ && !scripture_->base_source().empty() &&
                                                speech_language_supported(scripture_->primary_language())});
    if (navigation_.reader() &&
        (scripture_->base_source().empty() || !speech_language_supported(scripture_->primary_language()))) {
        result.listen = result.pause = false;
        result.playback_action = PlaybackAction::None;
    }
    if (navigation_.reader() && !active_playback() && has_reading_position()) {
        result.stop = true;
        if (result.playback_action != PlaybackAction::None && scripture_->selections().empty()) {
            result.listen_label = "Fortsätt";
            result.playback_label = "Fortsätt";
        }
    }
    return result;
}
void MainFrame::update_bar() {
    const auto policy = controls();
    const bool auxiliary = navigation_.auxiliary(), reader = navigation_.reader(),
               active = playback_.active();
    const auto listen_label = ui::utf8(policy.playback_label);
    if (menu_bar_)
        sync_menu();
    const auto tooltip_of = [](wxWindow* control, const wxString& text) {
        if (control->GetToolTipText() != text)
            control->SetToolTip(text);
    };
    if (play_->GetLabel() != listen_label)
        play_->SetLabel(listen_label);
    tooltip_of(play_, reader && !active && !policy.listen ? ui::utf8("Ingen svensk text att läsa upp")
                                                          : listen_label + ui::utf8(", mellanslag"));
    play_->face(policy.playback_action == PlaybackAction::Pause ? Symbol::Pause : Symbol::Play);
    play_->SetName(listen_label);
    play_->Enable(policy.playback_action != PlaybackAction::None);
    stop_->Enable(policy.stop);
    study_->allow_listening(policy.word_audio);
    tooltip_of(stop_, ui::utf8("Stoppa och börja om, Ctrl+."));
    back_->Enable(history_.can_back());
    forward_->Enable(history_.can_forward());
#ifdef __WXOSX__
    tooltip_of(back_, ui::utf8("Tillbaka, ⌘["));
    tooltip_of(forward_, ui::utf8("Framåt, ⌘]"));
#else
    tooltip_of(back_, "Tillbaka, Alt+Left");
    tooltip_of(forward_, ui::utf8("Framåt, Alt+Right"));
#endif
    const auto* picker =
        navigation_.current().screen == Screen::Bible ? static_cast<BiblePicker*>(current_page()) : nullptr;
    address_->Enable();
    for (auto& [pane, button] : panes_) {
        button->checked(pane == "study" ? settings_.word_study : settings_.split_view);
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
    else if (!reader)
        title = ui::utf8("Dagens läsningar");
    else if (const auto status = playback_.status(corpus_)) {
        title = ui::utf8(status->title);
        tooltip = ui::utf8(status->tooltip);
    } else
        title = dropdown ? parts_[std::min(part_, parts_.size() - 1)] : reading_title_;
    address_->show(title, dropdown);
    if (tooltip.empty())
        tooltip = ui::utf8(dropdown ? "Välj del av läsningen" : "Gå till bibelställe…, Ctrl+L");
    tooltip_of(address_, tooltip);
    address_->progress(reader && active ? playback_.playback().progress : 0);
}
} // namespace ortho
