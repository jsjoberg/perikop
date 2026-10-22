#pragma once

#include "storage/database.hpp"
#include "ui/picker_grid.hpp"
#include <functional>
#include <map>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace ortho {
class BiblePicker final : public wxPanel {
public:
    BiblePicker(wxWindow* parent, const CorpusDb&, Settings, std::function<void(Reading)> chosen);
    ~BiblePicker() override;
    // Collapse verses, then chapters, before returning to the previous main view.
    bool back();
    // The added parts and then the current selection, each a part of one reading.
    std::vector<Passage> parts() const;
    std::vector<wxString> part_labels() const;
    bool stacked() const {
        return !stack_.empty();
    }
    // Opens the parts as one reading.
    void open_selection();
    void on_selection(std::function<void()> callback) {
        selection_changed_ = std::move(callback);
    }

private:
    friend class MainFrame;
    std::vector<VerseRef> verses_of(const CanonBook&) const;
    bool available(const CanonBook&) const;
    void rebuild();
    std::vector<PickerGrid::Block> book_drawer(const CanonBook&);
    void show_books();
    void show_chapters(CanonBook);
    void show_verses(CanonBook, int chapter);
    void select_book(CanonBook);
    void select_chapter(CanonBook, int chapter);
    void select_verse(CanonBook, VerseRef);
    enum class SelectionUnit { Book, Chapter, Verse };
    void select_single_range(const CanonBook&, VerseRef first, VerseRef last, SelectionUnit, bool reset);
    void add_selection();
    void clear_stack();
    void update_selection();
    void update_grid();
    // Scrolls the least that shows this part of the grid.
    void reveal(wxRect);
    void open_passages(const std::vector<Passage>&);

    const CorpusDb& corpus_;
    Settings settings_;
    std::function<void(Reading)> chosen_;
    std::function<void()> selection_changed_;
    mutable std::map<std::string, bool> availability_;
    wxScrolledWindow* grid_ = nullptr;
    wxPanel* contents_ = nullptr;
    PickerGrid* cells_ = nullptr;
    std::optional<CanonBook> book_;
    std::optional<int> chapter_;
    void* native_scroll_ = nullptr;
    double wheel_remainder_ = 0;
    std::optional<CanonBook> range_book_;
    std::optional<VerseRef> first_, last_;
    std::optional<Passage> anchor_;
    SelectionUnit anchor_unit_ = SelectionUnit::Book;
    // Selections added with Lägg till, in reading order.
    std::vector<Passage> stack_;
};
} // namespace ortho
