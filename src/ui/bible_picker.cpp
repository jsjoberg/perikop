#include "ui/bible_picker.hpp"
#include "core/reading_display.hpp"
#include "ui/controls.hpp"
#include <algorithm>
#ifdef __APPLE__
#include "ui/native_scroll.hpp"
#endif
namespace ortho {
BiblePicker::BiblePicker(wxWindow* parent, const CorpusDb& corpus, Settings settings,
                         std::function<void(Reading)> chosen)
    : wxPanel(parent), corpus_(corpus), settings_(std::move(settings)), chosen_(std::move(chosen)) {
    auto* page = new wxBoxSizer(wxVERTICAL);
    header_ = new wxPanel(this);
    auto* header = new wxBoxSizer(wxHORIZONTAL);
    header->Add(ui::label(header_, "Bibel", 18), 1, wxALIGN_CENTER_VERTICAL);
    builder_toggle_ = new wxCheckBox(header_, wxID_ANY, ui::utf8("Flera intervall"));
    builder_toggle_->SetFont(ui_font());
    builder_toggle_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        set_builder(event.IsChecked());
    });
    header->Add(builder_toggle_, 0, wxALIGN_CENTER_VERTICAL);
    header_->SetSizer(header);
    page->Add(header_, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, FromDIP(24));

    grid_ = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    grid_->SetScrollRate(0, FromDIP(12));
    grid_->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
    grid_->SetMinSize({0, 0});
    contents_ = new wxPanel(grid_);
    rows_ = new wxBoxSizer(wxVERTICAL);
    contents_->SetSizer(rows_);
    auto* centered = new wxBoxSizer(wxVERTICAL);
    centered->Add(contents_, 0, wxALIGN_CENTER_HORIZONTAL);
    grid_->SetSizer(centered);
    grid_->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        update_grid();
        event.Skip();
    });
#ifdef __APPLE__
    // Capture trackpad input before native buttons consume it; remove the listener in the destructor.
    native_scroll_ = install_native_scroll(grid_, [this](double pixels) {
        wheel_remainder_ += pixels;
        const int unit = FromDIP(12), lines = int(wheel_remainder_ / unit);
        if (lines) {
            grid_->Scroll(0, std::max(0, grid_->GetViewStart().y + lines));
            wheel_remainder_ -= lines * unit;
        }
    });
#endif
    page->Add(grid_, 1, wxEXPAND);

    builder_panel_ = new wxPanel(this);
    auto* builder = new wxBoxSizer(wxVERTICAL);
    range_label_ = ui::label(builder_panel_, "", 11);
    builder->Add(range_label_, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(12));
    add_ = ui::button(builder_panel_, ui::utf8("Lägg till intervall"), [this] {
        if (range_book_ && first_) {
            add_range(*range_book_, *first_, last_.value_or(*first_));
            first_.reset();
            last_.reset();
            update_selection();
        }
    });
    builder->Add(add_, 0, wxBOTTOM, FromDIP(12));
    ranges_ = new wxListBox(builder_panel_, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 76)));
    ranges_->SetName(ui::utf8("Valda intervall i uppläsningsordning"));
    ranges_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        update_selection();
    });
    builder->Add(ranges_, 0, wxEXPAND);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    remove_ = ui::button(builder_panel_, "Ta bort", [this] {
        const int index = ranges_->GetSelection();
        if (index != wxNOT_FOUND) {
            selected_.erase(selected_.begin() + index);
            ranges_->Delete(unsigned(index));
            update_selection();
        }
    });
    actions->Add(remove_, 0);
    actions->AddStretchSpacer();
    open_ = ui::button(builder_panel_, ui::utf8("Öppna urval"), [this] {
        CallAfter([this] {
            open_selection();
        });
    });
    actions->Add(open_, 0);
    builder->Add(actions, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(12));
    builder_panel_->SetSizer(builder);
    page->Add(builder_panel_, 0, wxALIGN_CENTER_HORIZONTAL);
    builder_panel_->Hide();
    SetSizer(page);
    rebuild();
    update_selection();
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        const int width = std::clamp(GetClientSize().x - FromDIP(64), FromDIP(200), FromDIP(680));
        header_->SetMinSize({width, -1});
        builder_panel_->SetMinSize({width, -1});
        update_selection();
        update_grid();
        event.Skip();
    });
}
BiblePicker::~BiblePicker() {
#ifdef __APPLE__
    remove_native_scroll(native_scroll_);
#endif
}
std::string BiblePicker::frame(const CanonBook& book) const {
    return frame_source(settings_.primary, book.frame_book);
}
std::vector<VerseRef> BiblePicker::verses_of(const CanonBook& book) const {
    std::vector<VerseRef> result;
    for (const auto& ref : corpus_.coordinates(frame(book), book.frame_book))
        if (book.first_chapter <= ref.chapter && ref.chapter <= book.last_chapter)
            result.push_back(ref);
    return result;
}
bool BiblePicker::available(const CanonBook& book) const {
    if (const auto known = availability_.find(book.code); known != availability_.end())
        return known->second;
    const auto verses = verses_of(book);
    const auto source = source_for_language(settings_.primary, book.code);
    const bool found =
        std::any_of(verses.begin(), verses.begin() + std::min<size_t>(verses.size(), 50), [&](VerseRef ref) {
            return bool(corpus_.parallel_verse(frame(book), source, book.frame_book, ref));
        });
    availability_[book.code] = found;
    return found;
}
void BiblePicker::append_cells(const std::vector<Action>& items, int columns) {
    auto* table = new wxGridSizer(columns, FromDIP(4), FromDIP(4));
    tables_.emplace_back(table, columns);
    rows_->Add(table, 0, wxEXPAND);
    for (const auto& [label, action] : items) {
        auto* cell = ui::button(contents_, label, [this, action] {
            // Rebuilding a section must wait until the clicked button's handler returns.
            CallAfter(action);
        });
        cell->SetMinSize(FromDIP(wxSize(58, 30)));
        table->Add(cell, 0, wxEXPAND);
    }
}
void BiblePicker::rebuild() {
    wheel_remainder_ = 0;
    tables_.clear();
    note_ = chapters_label_ = verses_label_ = nullptr;
    rows_->Clear(true);
    // All books stay on this page. The chosen book and chapter append their own sections below them.
    for (int part = 0; part < 2; ++part) {
        rows_->Add(ui::label(contents_, part ? "NYA TESTAMENTET" : "GAMLA TESTAMENTET", 9), 0,
                   wxTOP | wxBOTTOM, FromDIP(12));
        std::vector<Action> books;
        std::vector<const CanonBook*> canon;
        for (const auto& book : osb_canon())
            if (book.new_testament == bool(part)) {
                books.emplace_back(ui::utf8(book_abbreviation(book.code)) +
                                       (deuterocanonical_book(book.code) ? "*" : ""),
                                   [this, book] {
                                       show_chapters(book);
                                   });
                canon.push_back(&book);
            }
        append_cells(books, 8);
        size_t i = 0;
        for (auto* item : tables_.back().first->GetChildren()) {
            auto* cell = static_cast<wxButton*>(item->GetWindow());
            const auto& book = *canon[i++];
            const bool has_text = available(book);
            cell->Enable(has_text);
            cell->SetToolTip(ui::utf8(corpus_.book_name(book.code)) +
                             (has_text ? wxString{} : ui::utf8(" · Ingen text på valt språk")));
            if (book_ && book_->code == book.code) {
                auto font = cell->GetFont();
                font.SetWeight(wxFONTWEIGHT_BOLD);
                cell->SetFont(font);
            }
        }
    }
    note_ = ui::label(contents_, "", 10);
    rows_->Add(note_, 0, wxTOP, FromDIP(12));
    if (book_) {
        const auto book = *book_;
        const auto verses = verses_of(book);
        chapters_label_ =
            ui::label(contents_, ui::utf8(corpus_.book_name(book.code)) + ui::utf8(" · Kapitel"), 18);
        rows_->Add(chapters_label_, 0, wxTOP | wxBOTTOM, FromDIP(28));
        rows_->Add(ui::button(contents_,
                              building_ ? ui::utf8("Lägg till hela boken") : ui::utf8("Hela boken"),
                              [this, book, verses] {
                                  CallAfter([this, book, verses] {
                                      if (!verses.empty())
                                          choose_range(book, verses.front(), verses.back());
                                  });
                              }),
                   0, wxBOTTOM, FromDIP(12));
        std::vector<Action> chapters;
        int previous = -1;
        for (const auto& ref : verses)
            if (ref.chapter != previous) {
                previous = ref.chapter;
                chapters.emplace_back(wxString::Format("%d", ref.chapter - book.offset()),
                                      [this, book, chapter = ref.chapter] {
                                          show_verses(book, chapter);
                                      });
            }
        append_cells(chapters, 10);
        if (chapter_) {
            verses_label_ =
                ui::label(contents_,
                          ui::utf8(corpus_.book_name(book.code)) +
                              wxString::Format(" %d", *chapter_ - book.offset()) + ui::utf8(" · Verser"),
                          18);
            rows_->Add(verses_label_, 0, wxTOP | wxBOTTOM, FromDIP(28));
            std::vector<VerseRef> in_chapter;
            std::vector<Action> numbers;
            for (const auto& ref : verses)
                if (ref.chapter == *chapter_) {
                    in_chapter.push_back(ref);
                    numbers.emplace_back(wxString::Format("%d", ref.verse) + ui::utf8(ref.suffix),
                                         [this, book, ref] {
                                             select_verse(book, ref);
                                         });
                }
            rows_->Add(ui::button(contents_,
                                  building_ ? ui::utf8("Lägg till hela kapitlet") : ui::utf8("Hela kapitlet"),
                                  [this, book, in_chapter] {
                                      CallAfter([this, book, in_chapter] {
                                          if (!in_chapter.empty())
                                              choose_range(book, in_chapter.front(), in_chapter.back());
                                      });
                                  }),
                       0, wxBOTTOM, FromDIP(12));
            append_cells(numbers, 10);
        }
    }
    rows_->AddSpacer(FromDIP(32));
    ui::recolor(this, palette(settings_.theme));
    note_->SetForegroundColour(palette(settings_.theme).muted);
    update_grid();
}
void BiblePicker::update_grid() {
    const int width = std::clamp(grid_->GetClientSize().x - FromDIP(64), FromDIP(200), FromDIP(680));
    contents_->SetMinSize({width, -1});
    const int columns = std::max(1, width / FromDIP(64));
    for (auto& [table, maximum] : tables_)
        table->SetCols(std::min(columns, maximum));
    if (note_) {
        note_->SetLabel(ui::utf8("Böcker med * finns i Septuaginta men inte i den hebreiska bibeln.\n"
                                 "Gråa böcker har ingen text på valt språk."));
        note_->Wrap(width);
    }
    contents_->InvalidateBestSize();
    grid_->FitInside();
}
void BiblePicker::scroll_to(wxWindow* target) {
    update_grid();
    const int y =
        target ? grid_->CalcUnscrolledPosition(contents_->GetPosition()).y + target->GetPosition().y : 0;
    grid_->Scroll(0, y / FromDIP(12));
}
void BiblePicker::show_books() {
    book_.reset();
    chapter_.reset();
    rebuild();
    scroll_to(nullptr);
}
void BiblePicker::show_chapters(CanonBook book) {
    book_ = std::move(book);
    chapter_.reset();
    rebuild();
    scroll_to(chapters_label_);
}
void BiblePicker::show_verses(CanonBook book, int chapter) {
    book_ = std::move(book);
    chapter_ = chapter;
    rebuild();
    scroll_to(verses_label_);
}
void BiblePicker::set_builder(bool enabled) {
    building_ = enabled;
    builder_toggle_->SetValue(enabled);
    builder_panel_->Show(enabled);
    update_selection();
    rebuild();
    scroll_to(verses_label_ ? verses_label_ : chapters_label_);
}
void BiblePicker::select_verse(CanonBook book, VerseRef ref) {
    if (!building_) {
        choose_range(book, ref, ref);
        return;
    }
    if (!first_ || last_ || !range_book_ || range_book_->code != book.code) {
        range_book_ = std::move(book);
        first_ = ref;
        last_.reset();
    } else
        last_ = ref;
    update_selection();
}
void BiblePicker::choose_range(const CanonBook& book, VerseRef first, VerseRef last) {
    if (building_)
        add_range(book, first, last);
    else
        open_passages({{book.frame_book, first, last}});
}
void BiblePicker::add_range(const CanonBook& book, VerseRef first, VerseRef last) {
    if (last < first)
        std::swap(first, last);
    const Passage passage{book.frame_book, first, last};
    selected_.push_back(passage);
    ranges_->Append(ui::utf8(passage_label(corpus_, {passage})));
    ranges_->SetSelection(int(selected_.size() - 1));
    update_selection();
}
void BiblePicker::update_selection() {
    if (first_ && range_book_) {
        auto first = *first_, last = last_.value_or(first);
        if (last < first)
            std::swap(first, last);
        range_label_->SetLabel(
            ui::utf8(passage_label(corpus_, {{range_book_->frame_book, first, last}})) +
            (last_ ? wxString{} : ui::utf8(" · Välj slutvers eller lägg till denna vers.")));
    } else
        range_label_->SetLabel(
            ui::utf8("Välj start- och slutvers. Lägg till fler intervall i önskad ordning."));
    range_label_->Wrap(std::max(200, builder_panel_->GetMinSize().x));
    add_->Enable(bool(first_));
    remove_->Enable(ranges_->GetSelection() != wxNOT_FOUND);
    open_->Enable(!selected_.empty());
    Layout();
    grid_->FitInside();
}
void BiblePicker::open_selection() {
    open_passages(selected_);
}
void BiblePicker::open_passages(const std::vector<Passage>& passages) {
    if (passages.empty())
        return;
    Reading reading{new_testament_book(passages.front().book) ? ReadingKind::Gospel
                                                              : ReadingKind::OldTestament,
                    passages.front(),
                    passage_label(corpus_, passages),
                    {passages.begin() + 1, passages.end()},
                    settings_.primary};
    reading.reference.clear();
    auto chosen = chosen_; // The callback can destroy this page.
    chosen(std::move(reading));
}
bool BiblePicker::back() {
    if (chapter_ && book_) {
        show_chapters(*book_);
        return true;
    }
    if (book_) {
        show_books();
        return true;
    }
    return false;
}
} // namespace ortho
