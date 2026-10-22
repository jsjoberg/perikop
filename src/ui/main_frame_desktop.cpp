#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <wx/display.h>
#include <wx/log.h>
#include <wx/textentry.h>
namespace ortho {
bool MainFrame::edit_enabled(int command) const {
    auto* focus = wxWindow::FindFocus();
    if (!focus || !focus->IsEnabled())
        return false;
    if (const auto* text = dynamic_cast<const wxTextEntryBase*>(focus)) {
        switch (command) {
        case wxID_UNDO:
            return text->CanUndo();
        case wxID_REDO:
            return text->CanRedo();
        case wxID_CUT:
            return text->CanCut();
        case wxID_COPY:
            return text->CanCopy();
        case wxID_PASTE:
            return text->CanPaste();
        case wxID_SELECTALL:
            return text->GetLastPosition() > 0;
        default:
            return false;
        }
    }
    return command == wxID_COPY && navigation_.reader() &&
           (focus == scripture_ || scripture_->IsDescendant(focus)) &&
           (!scripture_->selections().empty() || scripture_->highlighted_verse().has_value());
}
void MainFrame::edit(int command) {
    if (!edit_enabled(command))
        return;
    if (auto* text = dynamic_cast<wxTextEntryBase*>(wxWindow::FindFocus())) {
        switch (command) {
        case wxID_UNDO:
            text->Undo();
            break;
        case wxID_REDO:
            text->Redo();
            break;
        case wxID_CUT:
            text->Cut();
            break;
        case wxID_COPY:
            text->Copy();
            break;
        case wxID_PASTE:
            text->Paste();
            break;
        case wxID_SELECTALL:
            text->SelectAll();
            break;
        }
    } else if (command == wxID_COPY)
        scripture_->copy_selection();
}
void MainFrame::restore_window_placement() {
    const auto saved = user_.window_placement();
    if (!saved)
        return;
    int display = wxDisplay::GetFromPoint({saved->x, saved->y});
    if (display == wxNOT_FOUND)
        display = 0;
    const auto work = wxDisplay(unsigned(display)).GetClientArea();
    // Move onto the target display before converting the saved size for its DPI.
    Move(work.GetTopLeft());
    const auto minimum = FromDIP(wxSize(520, 480));
    const auto size = FromDIP(wxSize(saved->width, saved->height));
    const int width = std::clamp(size.x, std::min(minimum.x, work.width), work.width);
    const int height = std::clamp(size.y, std::min(minimum.y, work.height), work.height);
    SetSize(std::clamp(saved->x, work.x, work.x + work.width - width),
            std::clamp(saved->y, work.y, work.y + work.height - height), width, height);
    normal_window_rect_ = GetRect();
    normal_window_size_dip_ = ToDIP(GetSize());
    window_maximized_ = saved->maximized;
    if (saved->maximized)
        Maximize();
}
void MainFrame::save_window_placement() {
    if (!IsIconized() && !IsFullScreen()) {
        window_maximized_ = IsMaximized();
        if (!window_maximized_) {
            normal_window_rect_ = GetRect();
            normal_window_size_dip_ = ToDIP(GetSize());
        }
    }
    const auto size = normal_window_size_dip_;
    try {
        user_.window_placement(
            {normal_window_rect_.x, normal_window_rect_.y, size.x, size.y, window_maximized_});
    } catch (const std::exception& error) {
        wxLogWarning("Fönstrets storlek och position kunde inte sparas: %s", ui::utf8(error.what()));
    }
}
} // namespace ortho
