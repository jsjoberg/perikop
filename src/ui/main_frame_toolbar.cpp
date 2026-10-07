#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <iterator>
#include <wx/dcbuffer.h>
#include <wx/menu.h>
namespace ortho {
void MainFrame::create_toolbar() {
    // One browser-like bar for the whole program. Controls that do not apply
    // to the day page are dimmed there, not hidden.
    bar_ = new wxPanel(root_);
    bar_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    bar_->Bind(wxEVT_PAINT, &MainFrame::paint_toolbar, this);
    auto* bar = new wxBoxSizer(wxHORIZONTAL);
    back_ = new SymbolButton(bar_, Symbol::Back, "Tillbaka", [this] {
        show_readings();
    });
    back_->SetToolTip(ui::utf8("Tillbaka till dagens läsningar · Ctrl+L"));
    bar->Add(back_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    play_ = new SymbolButton(bar_, Symbol::Listen, "Lyssna", [this] {
        play_or_pause();
    });
    bar->Add(play_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    stop_ = new SymbolButton(bar_, Symbol::Stop, "Stoppa", [this] {
        stop_speech();
    });
    stop_->SetToolTip(ui::utf8("Avsluta uppläsningen · Escape"));
    stop_->Hide();
    bar->Add(stop_, 0, wxALIGN_CENTER_VERTICAL);
    address_ = new AddressBar(bar_, [this] {
        address_clicked();
    });
    address_->SetMinSize(FromDIP(wxSize(80, 30)));
    bar->Add(address_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    // Symbols for the right column; pressing the chosen one again closes it.
    const std::pair<std::string, std::variant<Symbol, wxString>> panes[] = {
        {"study", Symbol::Study}, {"sv", "SV"}, {"el", "GR"}, {"en", "EN"}};
    const char* names[] = {"Ordstudium", "Svenska", "Grekiska", "Engelska"};
    for (std::size_t i = 0; i < std::size(panes); ++i) {
        const auto& [pane, face] = panes[i];
        auto* button = new SymbolButton(bar_, face, names[i], [this, pane = pane] {
            toggle_pane(pane);
        });
        button->SetToolTip(ui::utf8("Höger spalt: ") + ui::utf8(names[i]).Lower());
        bar->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(2));
        panes_.emplace_back(pane, button);
    }
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
    if (scripture_->IsShown() && parts_.size() > 1) {
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
    following_audio_ = false;
    scripture_->follow_playback(false);
    scripture_->open_section(part);
    refresh_speech();
    update_bar();
}
void MainFrame::toggle_pane(const std::string& pane) {
    const bool study = pane == "study";
    const bool chosen = study ? settings_.word_study : settings_.parallel == pane;
    settings_.word_study = study && !chosen;
    settings_.parallel = study || chosen ? std::string{} : pane;
    apply_settings();
}
void MainFrame::update_bar() {
    const bool reader = scripture_->IsShown(), active = active_playback();
    const bool marked = reader && scripture_->selection();
    const bool paused = read_aloud_.playback().state == SpeechState::Paused;
    const wxString play_label = !active  ? (marked ? "Läs markering" : "Lyssna")
                                : paused ? "Fortsätt"
                                         : "Pausa";
    // Only the Swedish text can be read aloud; Greek and English have no voice.
    const bool speakable = active || settings_.primary == "sv";
    if (auto* bar = GetMenuBar()) {
        bar->SetLabel(play_item_, play_label + "\tCtrl+P");
        bar->Enable(play_item_, (reader || active) && speakable);
        bar->Enable(stop_item_, active);
        int pane_item = settings_.word_study ? study_item_ : right_item_;
        for (int i = 0; i < 3; ++i) {
            if (!settings_.word_study && settings_.parallel == column_languages[i])
                pane_item = right_item_ + 1 + i;
            const bool available = settings_.primary != column_languages[i];
            if (bar->IsEnabled(right_item_ + 1 + i) != available)
                bar->Enable(right_item_ + 1 + i, available);
        }
        if (!bar->IsChecked(pane_item))
            bar->Check(pane_item, true);
    }
    const auto tooltip_of = [](wxWindow* control, const wxString& text) {
        if (control->GetToolTipText() != text)
            control->SetToolTip(text);
    };
    play_->face(!active ? Symbol::Listen : paused ? Symbol::Resume : Symbol::Pause);
    if (play_->GetLabel() != play_label)
        play_->SetLabel(play_label);
    tooltip_of(play_, speakable ? play_label + ui::utf8(" · mellanslag")
                                : ui::utf8("Uppläsning finns bara på svenska"));
    play_->Enable((reader || active) && speakable);
    back_->Enable(reader);
    for (auto& [pane, button] : panes_) {
        button->checked(pane == "study" ? settings_.word_study : settings_.parallel == pane);
        button->Enable(reader && pane != settings_.primary);
    }
    if (stop_->IsShown() != active) {
        stop_->Show(active);
        bar_->Layout();
    }
    scripture_->return_button(active   ? (following_audio_ ? wxString{} : ui::utf8("Följ uppläsningen"))
                              : reader ? ui::utf8("Till läsningen")
                                       : wxString{},
                              active);
    // One short line: what is shown, where the reading is, or what it is waiting for.
    const bool dropdown = reader && parts_.size() > 1;
    wxString title, tooltip;
    if (const auto status = read_aloud_.status(corpus_)) {
        title = ui::utf8(status->title);
        tooltip = ui::utf8(status->tooltip);
    } else
        title = !reader    ? ui::utf8("Gå till bibelställe…")
                : dropdown ? parts_[std::min(part_, parts_.size() - 1)]
                           : reading_title_;
    address_->show(title, dropdown);
    if (tooltip.empty())
        tooltip = ui::utf8(dropdown ? "Välj del av läsningen" : "Gå till bibelställe… · Ctrl+G");
    tooltip_of(address_, tooltip);
    address_->progress(active ? read_aloud_.playback().progress : 0);
}
} // namespace ortho
