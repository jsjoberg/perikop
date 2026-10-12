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
    // The address field names the page, so the grid starts at the top.
    auto* page = new wxBoxSizer(wxVERTICAL);
    page->AddSpacer(FromDIP(24));

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

    SetSizer(page);
    rebuild();
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
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
    return corpus_.frame_source("sv", book.frame_book);
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
    const auto source = corpus_.source_for_language("sv", book.code);
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
            const bool open = book_ && book_->code == book.code;
            if (open) {
                books.open = books.cells.size();
                books.drawer = book_drawer(book);
            }
            // Books without Swedish text open too, for their Greek and English text.
            books.cells.push_back(
                {ui::utf8(book.abbreviation),
                 ui::utf8(book.name) + (available(book) ? wxString{} : ui::utf8(" · Svenska saknas")), true,
                 open ? State::Selected : State::Plain, [this, book, open] {
                     if (open)
                         show_books();
                     else
                         select_book(book);
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
    const bool pending = first_ && range_book_ && range_book_->code == book.code;
    const auto low = pending ? std::min(*first_, last_.value_or(*first_)) : VerseRef{};
    const auto high = pending ? std::max(*first_, last_.value_or(*first_)) : VerseRef{};
    // Lägg till makes the selection the next part of one reading, as Del 1, Del 2 and so on.
    const bool selection = first_ && range_book_;
    std::vector<Block> drawer;
    drawer.push_back({.kind = Block::Kind::Title,
                      .text = ui::utf8(book.name),
                      .actions = {{ui::utf8("Lägg till"),
                                   [this] {
                                       add_selection();
                                   },
                                   selection},
                                  {"Rensa",
                                   [this] {
                                       clear_stack();
                                   },
                                   !stack_.empty()},
                                  {ui::utf8("Öppna urval"),
                                   [this] {
                                       // Opening closes this page, so it waits for the click to end.
                                       CallAfter([this] {
                                           open_selection();
                                       });
                                   },
                                   selection || !stack_.empty()}}});
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
        chapters.cells.push_back(
            {wxString::Format("%d", ref.chapter - book.offset()),
             {},
             true,
             pending && low.chapter <= ref.chapter && ref.chapter <= high.chapter
                 ? (ref.chapter == low.chapter || ref.chapter == high.chapter ? State::Selected
                                                                              : State::Marked)
             : open ? State::Selected
                    : State::Plain,
             [this, book, open, chapter = ref.chapter] {
                 if (open)
                     show_chapters(book);
                 else
                     select_chapter(book, chapter);
             }});
    }
    if (chapters.open && chapter_) {
        auto& verse_drawer = chapters.drawer;
        verse_drawer.push_back(
            {.kind = Block::Kind::Title,
             .text = ui::utf8("Kapitel ") + wxString::Format("%d", *chapter_ - book.offset())});
        // Shade the selected range and mark its ends.
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
void BiblePicker::select_book(CanonBook book) {
    const auto verses = verses_of(book);
    if (!verses.empty())
        select_single_range(book, verses.front(), verses.back(), SelectionUnit::Book, true);
    show_chapters(std::move(book));
}
void BiblePicker::select_chapter(CanonBook book, int chapter) {
    auto verses = verses_of(book);
    std::erase_if(verses, [chapter](const auto& ref) {
        return ref.chapter != chapter;
    });
    if (!verses.empty())
        select_single_range(book, verses.front(), verses.back(), SelectionUnit::Chapter,
                            anchor_unit_ == SelectionUnit::Book);
    show_verses(std::move(book), chapter);
}
void BiblePicker::select_single_range(const CanonBook& book, VerseRef first, VerseRef last,
                                      SelectionUnit unit, bool reset) {
    if (reset || !anchor_ || !range_book_ || range_book_->code != book.code) {
        anchor_ = Passage{book.frame_book, first, last};
        anchor_unit_ = unit;
    }
    range_book_ = book;
    first_ = std::min(anchor_->first, first);
    last_ = std::max(anchor_->last, last);
    update_selection();
}
void BiblePicker::select_verse(CanonBook book, VerseRef ref) {
    // A verse starts a precise range within the default book or chapter selection.
    // Later clicks keep this anchor and adjust the other end, including across chapters.
    select_single_range(
        book, ref, ref, SelectionUnit::Verse,
        anchor_unit_ == SelectionUnit::Book ||
            (anchor_unit_ == SelectionUnit::Chapter && first_ && last_ && first_->chapter == last_->chapter));
    rebuild();
}
void BiblePicker::add_selection() {
    if (!first_ || !range_book_)
        return;
    stack_.push_back({range_book_->frame_book, *first_, last_.value_or(*first_)});
    // The next selection starts afresh and becomes the next part.
    first_.reset();
    last_.reset();
    anchor_.reset();
    anchor_unit_ = SelectionUnit::Book;
    update_selection();
    rebuild();
}
void BiblePicker::clear_stack() {
    stack_.clear();
    update_selection();
    rebuild();
}
void BiblePicker::update_selection() {
    if (selection_changed_)
        selection_changed_();
}
std::vector<Passage> BiblePicker::parts() const {
    auto result = stack_;
    if (first_ && range_book_)
        result.push_back({range_book_->frame_book, *first_, last_.value_or(*first_)});
    return result;
}
std::vector<wxString> BiblePicker::part_labels() const {
    // As for a reading in several parts: Del 1 · Ps 22:1–6.
    std::vector<wxString> result;
    const auto all = parts();
    for (std::size_t i = 0; i < all.size(); ++i)
        result.push_back(stack_.empty() ? ui::utf8(passage_label(corpus_, {all[i]}))
                                        : wxString::Format(ui::utf8("Del %d · "), int(i + 1)) +
                                              ui::utf8(passage_label(corpus_, {all[i]})));
    return result;
}
void BiblePicker::open_selection() {
    open_passages(parts());
}
void BiblePicker::open_passages(const std::vector<Passage>& passages) {
    if (passages.empty())
        return;
    Reading reading{corpus_.new_testament_book(passages.front().book) ? ReadingKind::Gospel
                                                                      : ReadingKind::OldTestament,
                    passages.front(),
                    {passages.begin() + 1, passages.end()},
                    "sv"};
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
