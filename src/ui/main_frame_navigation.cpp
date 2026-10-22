#include "core/reading_position.hpp"
#include "ui/about.hpp"
#include "ui/bible_picker.hpp"
#include "ui/main_frame.hpp"
#include "ui/month_calendar.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include <set>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

namespace ortho {
#ifdef __WXMSW__
WXLRESULT MainFrame::MSWWindowProc(WXUINT message, WXWPARAM wparam, WXLPARAM lparam) {
    if (message == WM_APPCOMMAND && IsEnabled() && !IsBeingDeleted()) {
        const int command = GET_APPCOMMAND_LPARAM(lparam);
        if (command == APPCOMMAND_BROWSER_BACKWARD || command == APPCOMMAND_BROWSER_FORWARD) {
            if (command == APPCOMMAND_BROWSER_BACKWARD)
                back();
            else
                forward();
            return TRUE;
        }
    }
    return wxFrame::MSWWindowProc(message, wparam, lparam);
}
#endif
wxWindow* MainFrame::current_page() const {
    if (!navigation_.auxiliary())
        return nullptr;
    return pages_.at(navigation_.current().id).window;
}
wxString MainFrame::current_page_title() const {
    return navigation_.auxiliary() ? pages_.at(navigation_.current().id).title : wxString{};
}
void MainFrame::present_navigation() {
    const bool reader = navigation_.reader();
    root_->GetSizer()->Show(reader_sizer_, reader);
    scripture_->Show(reader);
    readings_->Show(navigation_.current().screen == Screen::Day);
    study_->Show(reader && settings_.word_study);
    if (auto* page = current_page()) {
        if (!root_->GetSizer()->GetItem(page))
            root_->GetSizer()->Add(page, 1, wxEXPAND);
        page->Show();
        page->SetFocus();
    }
    root_->Layout();
}
void MainFrame::save_navigation() {
    if (restoring_navigation_)
        return;
    Visit visit{navigation_,
                selected_.date(),
                visible_reading_,
                tracked_,
                part_,
                scripture_->scroll_position(),
                readings_->GetViewStart(),
                month_open_,
                scripture_->selections()};
    visits_.insert_or_assign(history_.current(), std::move(visit));
}
void MainFrame::record_navigation() {
    if (restoring_navigation_)
        return;
    history_.visit();
    save_navigation();
    std::set<NavigationState::ViewId> retained;
    for (auto it = visits_.begin(); it != visits_.end();) {
        if (!history_.contains(it->first))
            it = visits_.erase(it);
        else {
            for (const auto& page : it->second.navigation.pages())
                retained.insert(page.id);
            ++it;
        }
    }
    for (auto it = pages_.begin(); it != pages_.end();) {
        if (!retained.contains(it->first)) {
            root_->GetSizer()->Detach(it->second.window);
            it->second.window->Hide();
            it->second.window->Destroy();
            it = pages_.erase(it);
        } else
            ++it;
    }
    update_bar();
}
void MainFrame::restore_navigation() {
    const auto saved = visits_.at(history_.current());
    restoring_navigation_ = true;
    if (auto* page = current_page()) {
        page->Hide();
        root_->GetSizer()->Detach(page);
    }
    const bool changed_reading =
        saved.reading && (!visible_reading_ ||
                          reading_key(corpus_, *saved.reading) != reading_key(corpus_, *visible_reading_));
    if (changed_reading) {
        playback_.select_reading(*speech_);
        show_reading(*saved.reading);
    }
    const bool changed_day = selected_.date() != saved.date;
    const bool changed_month = month_open_ != saved.month_open;
    selected_.select(saved.date);
    month_open_ = saved.month_open;
    if (changed_day || changed_month || saved.navigation.current().screen == Screen::Day)
        refresh_day();
    navigation_.restore(saved.navigation);
    tracked_ = saved.tracked;
    if (saved.reading)
        visible_reading_ = saved.reading;
    if (saved.part != part_ && saved.reading) {
        part_ = saved.part;
        scripture_->open_section(part_);
    }
    present_navigation();
    update_study();
    if (navigation_.current().screen == Screen::Plans)
        fill_plans(static_cast<wxScrolledWindow*>(current_page()));
    if (navigation_.current().screen == Screen::Settings)
        fill_settings(static_cast<wxScrolledWindow*>(current_page()));
    if (changed_reading || scripture_->scroll_position() != saved.scroll) {
        scripture_->restore_scroll(saved.scroll);
        scripture_->clear_selection();
        for (const auto& range : saved.selections)
            scripture_->select_verses(range.first, range.last, true);
    }
    readings_->Scroll(saved.day_scroll);
    if (navigation_.reader())
        scripture_->SetFocus();
    restoring_navigation_ = false;
    refresh_speech();
}
void MainFrame::show_readings(bool record) {
    if (record)
        save_navigation();
    pause_for_navigation();
    close_pages();
    tracked_.reset();
    scripture_->follow_playback(false);
    navigation_.show_root(Screen::Day);
    const auto day_scroll = readings_->GetViewStart();
    refresh_day();
    present_navigation();
    readings_->Scroll(day_scroll);
    update_study();
    refresh_speech();
    if (record &&
        (visits_.empty() || visits_.at(history_.current()).navigation.current().screen != Screen::Day))
        record_navigation();
}
void MainFrame::show_page(wxWindow* page, Screen screen, const wxString& title) {
    save_navigation();
    pause_for_navigation();
    if (auto* previous = current_page()) {
        previous->Hide();
        root_->GetSizer()->Detach(previous);
    }
    const auto view = navigation_.push(screen);
    pages_.emplace(view.id, PageWindow{page, title, screen});
    present_navigation();
    refresh_speech();
    record_navigation();
}
void MainFrame::close_page() {
    back();
}
void MainFrame::close_pages() {
    while (const auto removed = navigation_.pop()) {
        auto* page = pages_.at(removed->id).window;
        root_->GetSizer()->Detach(page);
        page->Hide();
    }
}
void MainFrame::back() {
    if (!history_.can_back())
        return;
    save_navigation();
    pause_for_navigation();
    history_.back();
    restore_navigation();
}
void MainFrame::forward() {
    if (!history_.can_forward())
        return;
    save_navigation();
    pause_for_navigation();
    history_.forward();
    restore_navigation();
}
void MainFrame::about() {
    if (navigation_.current().screen != Screen::About)
        show_page(make_about_page(root_, settings_.theme, resources_), Screen::About, "Om Perikop");
}
void MainFrame::browse_bible() {
    if (navigation_.current().screen == Screen::Bible)
        return;
    auto* page = new BiblePicker(root_, corpus_, settings_, [this](const Reading& reading) {
        open_reading(reading);
    });
    page->on_selection([this] {
        update_bar();
    });
    show_page(page, Screen::Bible, "Bibel");
}
void MainFrame::pick_date() {
    const bool home = navigation_.current().screen == Screen::Day;
    month_open_ = !home || !month_open_;
    if (!home)
        show_readings();
    refresh_day();
    if (month_)
        month_->SetFocus();
}
} // namespace ortho
