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
    auto* rows = new wxBoxSizer(wxVERTICAL);
    cells_ = new PickerGrid(contents_, settings_.theme);
    cells_->on_focus([this](wxRect cell) {
        reveal(cell);
    });
    rows->Add(cells_, 0);
    rows->AddSpacer(FromDIP(32));
    contents_->SetSizer(rows);
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
            rebuild();
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
    return corpus_.frame_source(settings_.primary, book.frame_book);
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
    const auto source = corpus_.source_for_language(settings_.primary, book.code);
    const bool found =
        std::any_of(verses.begin(), verses.begin() + std::min<size_t>(verses.size(), 50), [&](VerseRef ref) {
            return bool(corpus_.parallel_verse(frame(book), source, book.frame_book, ref));
        });
    availability_[book.code] = found;
    return found;
}
void BiblePicker::rebuild() {
    using Block = PickerGrid::Block;
    using State = PickerGrid::State;
    wheel_remainder_ = 0;
    // Every book stays on this page. An open book unfolds its chapters beneath its own row.
    std::vector<Block> blocks;
    for (int part = 0; part < 2; ++part) {
        blocks.push_back(
            {.kind = Block::Kind::Heading, .text = part ? "NYA TESTAMENTET" : "GAMLA TESTAMENTET"});
        Block books{.kind = Block::Kind::Cells, .columns = 8};
        for (const auto& book : corpus_.canon()) {
            if (book.new_testament != bool(part))
                continue;
            const bool has_text = available(book), open = book_ && book_->code == book.code;
            if (open) {
                books.open = books.cells.size();
                books.drawer = book_drawer(book);
            }
            books.cells.push_back({ui::utf8(corpus_.book_abbreviation(book.code)),
                                   ui::utf8(corpus_.book_name(book.code)) +
                                       (has_text ? wxString{} : ui::utf8(" · Ingen text på valt språk")),
                                   has_text, open ? State::Selected : State::Plain, [this, book, open] {
                                       if (open)
                                           show_books();
                                       else
                                           show_chapters(book);
                                   }});
        }
        blocks.push_back(std::move(books));
    }
    cells_->set(std::move(blocks));
    ui::recolor(this, palette(settings_.theme));
    update_grid();
}
std::vector<PickerGrid::Block> BiblePicker::book_drawer(const CanonBook& book) {
    using Block = PickerGrid::Block;
    using State = PickerGrid::State;
    const auto verses = verses_of(book);
    std::vector<Block> drawer;
    drawer.push_back({.kind = Block::Kind::Title,
                      .text = ui::utf8(corpus_.book_name(book.code)),
                      .action_label = building_ ? ui::utf8("Lägg till hela boken") : ui::utf8("Hela boken"),
                      .action = [this, book, verses] {
                          if (!verses.empty())
                              choose_range(book, verses.front(), verses.back());
                      }});
    Block chapters{.kind = Block::Kind::Cells};
    std::vector<VerseRef> in_chapter;
    int previous = -1;
    for (const auto& ref : verses) {
        if (ref.chapter == chapter_)
            in_chapter.push_back(ref);
        if (ref.chapter == previous)
            continue;
        previous = ref.chapter;
        const bool open = ref.chapter == chapter_;
        if (open)
            chapters.open = chapters.cells.size();
        chapters.cells.push_back({wxString::Format("%d", ref.chapter - book.offset()),
                                  {},
                                  true,
                                  open ? State::Selected : State::Plain,
                                  [this, book, open, chapter = ref.chapter] {
                                      if (open)
                                          show_chapters(book);
                                      else
                                          show_verses(book, chapter);
                                  }});
    }
    if (chapters.open) {
        auto& verse_drawer = chapters.drawer;
        verse_drawer.push_back(
            {.kind = Block::Kind::Title,
             .text = ui::utf8("Kapitel ") + wxString::Format("%d", *chapter_ - book.offset()),
             .action_label = building_ ? ui::utf8("Lägg till hela kapitlet") : ui::utf8("Hela kapitlet"),
             .action = [this, book, in_chapter] {
                 if (!in_chapter.empty())
                     choose_range(book, in_chapter.front(), in_chapter.back());
             }});
        // While building, the pending range is shaded and its ends are marked.
        const bool pending = building_ && first_ && range_book_ && range_book_->code == book.code;
        const auto low = pending ? std::min(*first_, last_.value_or(*first_)) : VerseRef{};
        const auto high = pending ? std::max(*first_, last_.value_or(*first_)) : VerseRef{};
        Block numbers{.kind = Block::Kind::Cells};
        for (const auto& ref : in_chapter)
            numbers.cells.push_back({wxString::Format("%d", ref.verse) + ui::utf8(ref.suffix),
                                     {},
                                     true,
                                     !pending || ref < low || high < ref ? State::Plain
                                     : ref == low || ref == high         ? State::Selected
                                                                         : State::Marked,
                                     [this, book, ref] {
                                         select_verse(book, ref);
                                     }});
        verse_drawer.push_back(std::move(numbers));
    }
    drawer.push_back(std::move(chapters));
    return drawer;
}
void BiblePicker::update_grid() {
    const int width = std::clamp(grid_->GetClientSize().x - FromDIP(64), FromDIP(200), FromDIP(680));
    contents_->SetMinSize({width, -1});
    cells_->set_width(width);
    contents_->InvalidateBestSize();
    contents_->Layout();
    grid_->FitInside();
    grid_->Layout();
}
void BiblePicker::reveal(wxRect area) {
    if (area.IsEmpty())
        return;
    update_grid();
    const int unit = FromDIP(12), margin = FromDIP(12);
    const int top =
        grid_->CalcUnscrolledPosition(contents_->GetPosition()).y + cells_->GetPosition().y + area.y - margin;
    const int bottom = top + area.height + 2 * margin;
    const int view = grid_->GetViewStart().y * unit, height = grid_->GetClientSize().y;
    // Bring the end into view without moving the start out of it.
    int target = view;
    if (bottom > view + height)
        target = bottom - height;
    target = std::min(target, top);
    if (target != view)
        grid_->Scroll(0, std::max(0, target > view ? (target + unit - 1) / unit : target / unit));
}
void BiblePicker::show_books() {
    book_.reset();
    chapter_.reset();
    rebuild();
}
void BiblePicker::show_chapters(CanonBook book) {
    book_ = std::move(book);
    chapter_.reset();
    rebuild();
    reveal(cells_->unfolded(1));
}
void BiblePicker::show_verses(CanonBook book, int chapter) {
    book_ = std::move(book);
    chapter_ = chapter;
    rebuild();
    reveal(cells_->unfolded(2));
}
void BiblePicker::set_builder(bool enabled) {
    building_ = enabled;
    builder_toggle_->SetValue(enabled);
    builder_panel_->Show(enabled);
    update_selection();
    rebuild();
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
    rebuild();
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
    Reading reading{corpus_.new_testament_book(passages.front().book) ? ReadingKind::Gospel
                                                                      : ReadingKind::OldTestament,
                    passages.front(),
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
