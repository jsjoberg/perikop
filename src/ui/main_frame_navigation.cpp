#include "ui/about.hpp"
#include "ui/bible_picker.hpp"
#include "ui/main_frame.hpp"
#include "ui/month_calendar.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include <wx/scrolwin.h>
#include <wx/sizer.h>

namespace ortho {
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
void MainFrame::show_readings() {
    pause_for_navigation();
    close_pages();
    if (tracked_ && navigation_.reader() && part_ + 1 >= parts_.size() && scripture_->end_seen()) {
        const auto item = *tracked_;
        tracked_.reset();
        offer_completion(item, false);
    }
    tracked_.reset();
    scripture_->follow_playback(false);
    navigation_.show_root(Screen::Day);
    present_navigation();
    update_study();
    refresh_speech();
}
void MainFrame::show_page(wxWindow* page, Screen screen, const wxString& title) {
    pause_for_navigation();
    if (auto* previous = current_page()) {
        previous->Hide();
        root_->GetSizer()->Detach(previous);
    }
    const auto view = navigation_.push(screen);
    pages_.emplace(view.id, PageWindow{page, title, screen});
    present_navigation();
    refresh_speech();
}
void MainFrame::close_page() {
    const auto removed = navigation_.pop();
    if (!removed)
        return;
    auto* page = pages_.at(removed->id).window;
    root_->GetSizer()->Detach(page);
    page->Hide();
    page->Destroy();
    pages_.erase(removed->id);
    present_navigation();
}
void MainFrame::close_pages() {
    while (navigation_.auxiliary())
        close_page();
}
void MainFrame::back() {
    if (!navigation_.auxiliary()) {
        show_readings();
        return;
    }
    if (navigation_.current().screen == Screen::Bible && static_cast<BiblePicker*>(current_page())->back())
        return;
    close_page();
    refresh_speech();
    if (navigation_.reader())
        scripture_->SetFocus();
}
void MainFrame::about() {
    if (navigation_.current().screen != Screen::About)
        show_page(make_about_page(root_, settings_.theme, resources_), Screen::About, "Om Perikop");
}
void MainFrame::browse_bible() {
    if (navigation_.current().screen == Screen::Bible)
        return;
    auto* page = new BiblePicker(root_, corpus_, settings_, [this](const Reading& reading) {
        tracked_.reset();
        open_reading(reading);
    });
    page->on_selection([this] {
        update_bar();
    });
    show_page(page, Screen::Bible, "Bibel");
}
void MainFrame::pick_date() {
    if (navigation_.auxiliary())
        return;
    const bool home = navigation_.current().screen == Screen::Day;
    month_open_ = !home || !month_open_;
    if (!home)
        show_readings();
    refresh_day();
    if (month_)
        month_->SetFocus();
}
} // namespace ortho
