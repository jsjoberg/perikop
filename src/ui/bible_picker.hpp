#pragma once

#include "storage/database.hpp"
#include <functional>
#include <map>
#include <wx/checkbox.h>
#include <wx/listbox.h>
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

private:
    friend class MainFrame;
    using Action = std::pair<wxString, std::function<void()>>;
    std::string frame(const CanonBook&) const;
    std::vector<VerseRef> verses_of(const CanonBook&) const;
    bool available(const CanonBook&) const;
    void append_cells(const std::vector<Action>&, int columns);
    void rebuild();
    void show_books();
    void show_chapters(CanonBook);
    void show_verses(CanonBook, int chapter);
    void select_verse(CanonBook, VerseRef);
    void choose_range(const CanonBook&, VerseRef first, VerseRef last);
    void set_builder(bool);
    void add_range(const CanonBook&, VerseRef first, VerseRef last);
    void update_selection();
    void update_grid();
    void scroll_to(wxWindow*);
    void open_selection();
    void open_passages(const std::vector<Passage>&);

    const CorpusDb& corpus_;
    Settings settings_;
    std::function<void(Reading)> chosen_;
    mutable std::map<std::string, bool> availability_;
    wxPanel* header_ = nullptr;
    wxCheckBox* builder_toggle_ = nullptr;
    wxPanel* builder_panel_ = nullptr;
    bool building_ = false;
    wxStaticText* range_label_ = nullptr;
    wxScrolledWindow* grid_ = nullptr;
    wxPanel* contents_ = nullptr;
    wxBoxSizer* rows_ = nullptr;
    std::vector<std::pair<wxGridSizer*, int>> tables_;
    wxStaticText* note_ = nullptr;
    wxStaticText* chapters_label_ = nullptr;
    wxStaticText* verses_label_ = nullptr;
    std::optional<CanonBook> book_;
    std::optional<int> chapter_;
    void* native_scroll_ = nullptr;
    double wheel_remainder_ = 0;
    wxListBox* ranges_ = nullptr;
    wxButton* add_ = nullptr;
    wxButton* remove_ = nullptr;
    wxButton* open_ = nullptr;
    std::optional<CanonBook> range_book_;
    std::optional<VerseRef> first_, last_;
    std::vector<Passage> selected_;
};
} // namespace ortho
