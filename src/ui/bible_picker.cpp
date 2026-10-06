#include "ui/bible_picker.hpp"
#include "core/reading_display.hpp"
#include "ui/controls.hpp"
#include <algorithm>
#include <map>
#include <wx/dialog.h>
#include <wx/sizer.h>
namespace ortho {
namespace {
// Short Swedish book names for the Bible picker grid.
std::string book_abbreviation(const std::string& book) {
    static const std::map<std::string, std::string> names = {
        {"Gen", "1 Mos"},    {"Exod", "2 Mos"},   {"Lev", "3 Mos"},     {"Num", "4 Mos"},
        {"Deut", "5 Mos"},   {"Josh", "Jos"},     {"Judg", "Dom"},      {"Ruth", "Rut"},
        {"1Sam", "1 Sam"},   {"2Sam", "2 Sam"},   {"1Kgs", "1 Kung"},   {"2Kgs", "2 Kung"},
        {"1Chr", "1 Krön"},  {"2Chr", "2 Krön"},  {"Ezra", "Esra"},     {"Neh", "Neh"},
        {"Esth", "Est"},     {"Job", "Job"},      {"Ps", "Ps"},         {"Prov", "Ords"},
        {"Eccl", "Pred"},    {"Song", "Höga v"},  {"Isa", "Jes"},       {"Jer", "Jer"},
        {"Lam", "Klag"},     {"Ezek", "Hes"},     {"Dan", "Dan"},       {"Hos", "Hos"},
        {"Joel", "Joel"},    {"Amos", "Am"},      {"Obad", "Ob"},       {"Jonah", "Jona"},
        {"Micah", "Mika"},   {"Nah", "Nah"},      {"Hab", "Hab"},       {"Zeph", "Sef"},
        {"Hag", "Hagg"},     {"Zech", "Sak"},     {"Mal", "Mal"},       {"Tob", "Tob"},
        {"Jdt", "Judit"},    {"EsthGr", "T Est"}, {"Wis", "Vish"},      {"Sir", "Syr"},
        {"Baruch", "Bar"},   {"EpJer", "Jer br"}, {"PrAzar", "Asarj"},  {"Sus", "Sus"},
        {"Bel", "Bel"},      {"DanGr", "Dan gr"}, {"1Macc", "1 Mack"},  {"2Macc", "2 Mack"},
        {"3Macc", "3 Mack"}, {"4Macc", "4 Mack"}, {"1Esd", "1 Esd"},    {"2Esd", "2 Esd"},
        {"PrMan", "Man"},    {"Ps151", "Ps 151"}, {"Matt", "Matt"},     {"Mark", "Mark"},
        {"Luke", "Luk"},     {"John", "Joh"},     {"Acts", "Apg"},      {"Rom", "Rom"},
        {"1Cor", "1 Kor"},   {"2Cor", "2 Kor"},   {"Gal", "Gal"},       {"Eph", "Ef"},
        {"Phil", "Fil"},     {"Col", "Kol"},      {"1Thess", "1 Tess"}, {"2Thess", "2 Tess"},
        {"1Tim", "1 Tim"},   {"2Tim", "2 Tim"},   {"Titus", "Tit"},     {"Philemon", "Filem"},
        {"Heb", "Hebr"},     {"James", "Jak"},    {"1Peter", "1 Petr"}, {"2Peter", "2 Petr"},
        {"1John", "1 Joh"},  {"2John", "2 Joh"},  {"3John", "3 Joh"},   {"Jude", "Jud"},
        {"Rev", "Upp"}};
    const auto it = names.find(book);
    return it == names.end() ? book : it->second;
}
} // namespace
std::optional<Reading> pick_bible_reading(wxWindow* parent, const CorpusDb& corpus,
                                          const Settings& settings) {
    // Three grids, as in most Bible apps: book, then chapter, then verse.
    // Books follow the OSB order; the Old Testament uses Septuagint numbers.
    wxDialog dialog(parent, wxID_ANY, ui::utf8("Gå till bibelställe"), wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* header = new wxBoxSizer(wxHORIZONTAL);
    std::function<void()> go_back;
    // Each step replaces go_back, so call a copy.
    auto* back = ui::button(&dialog, ui::utf8("‹ Tillbaka"), [&] {
        if (auto action = go_back)
            action();
    });
    auto* title = ui::label(&dialog, "", 13);
    header->Add(back, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, dialog.FromDIP(12));
    header->Add(title, 1, wxALIGN_CENTER_VERTICAL);
    sizer->Add(header, 0, wxEXPAND | wxALL, dialog.FromDIP(16));
    auto* grid = new wxPanel(&dialog);
    sizer->Add(grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(16));
    dialog.SetSizer(sizer);
    std::optional<Reading> chosen;
    std::function<void()> show_books;
    std::function<void(CanonBook)> show_chapters;
    const auto frame = [&](const CanonBook& book) {
        return frame_source(settings.primary, book.frame_book);
    };
    // Framing verses of a book, within its chapters of the framing book.
    const auto verses_of = [&](const CanonBook& book) {
        std::vector<VerseRef> result;
        for (const auto& ref : corpus.coordinates(frame(book), book.frame_book))
            if (book.first_chapter <= ref.chapter && ref.chapter <= book.last_chapter)
                result.push_back(ref);
        return result;
    };
    // A book is available when the left pane's edition has text for its opening verses;
    // the first verses alone can be missing, as Sirach's prologue is in Swedish.
    const auto available = [&](const CanonBook& book) {
        const auto verses = verses_of(book);
        const auto source = source_for_language(settings.primary, book.code);
        return std::any_of(verses.begin(), verses.begin() + std::min<size_t>(verses.size(), 50),
                           [&](VerseRef ref) {
                               return bool(corpus.parallel_verse(frame(book), source, book.frame_book, ref));
                           });
    };
    // Opens the chosen verses; Lyssna then reads all of them.
    const auto choose = [&](const CanonBook& book, VerseRef first, VerseRef last) {
        Reading reading{book.new_testament ? ReadingKind::Gospel : ReadingKind::OldTestament,
                        {book.frame_book, first, last},
                        passage_label(corpus, {{book.frame_book, first, last}}),
                        {},
                        settings.primary};
        reading.reference = frame(book);
        chosen = reading;
        dialog.EndModal(wxID_OK);
    };
    // `whole` is an optional button before the numbers, for the whole book or chapter.
    const auto cells = [&](const std::vector<std::pair<wxString, std::function<void()>>>& items, int columns,
                           const std::vector<std::pair<size_t, wxString>>& groups,
                           const std::pair<wxString, std::function<void()>>& whole = {}) {
        grid->DestroyChildren();
        auto* rows = new wxBoxSizer(wxVERTICAL);
        if (whole.second)
            rows->Add(ui::button(grid, whole.first,
                                 [&dialog, action = whole.second] {
                                     dialog.CallAfter(action);
                                 }),
                      0, wxBOTTOM, dialog.FromDIP(10));
        size_t group = 0;
        wxGridSizer* table = nullptr;
        for (size_t i = 0; i < items.size(); ++i) {
            if (!table || (group < groups.size() && groups[group].first == i)) {
                if (group < groups.size() && groups[group].first == i) {
                    rows->Add(ui::label(grid, groups[group].second, 9), 0, wxTOP | wxBOTTOM,
                              dialog.FromDIP(8));
                    ++group;
                }
                table = new wxGridSizer(columns, dialog.FromDIP(4), dialog.FromDIP(4));
                rows->Add(table, 0, wxEXPAND);
            }
            // The grid is rebuilt by a click, so the clicked button must outlive its handler.
            auto* cell = ui::button(grid, items[i].first, [&dialog, action = items[i].second] {
                dialog.CallAfter(action);
            });
            cell->SetMinSize(dialog.FromDIP(wxSize(58, 30)));
            table->Add(cell, 0, wxEXPAND);
        }
        grid->SetSizer(rows);
        ui::recolor(&dialog, palette(settings.theme));
        dialog.Layout();
        dialog.Fit();
        // Keep the width of the book grid, so the dialog does not jump between steps.
        if (dialog.GetMinSize().x < 0)
            dialog.SetMinSize(wxSize(dialog.GetSize().x, -1));
    };
    show_books = [&] {
        back->Hide();
        go_back = {};
        title->SetLabel("Välj bok");
        std::vector<std::pair<wxString, std::function<void()>>> items;
        std::vector<std::pair<size_t, wxString>> groups;
        std::vector<std::pair<wxString, const CanonBook*>> cells_books;
        for (int part = 0; part < 2; ++part) {
            groups.emplace_back(items.size(), part == 0 ? "GAMLA TESTAMENTET" : "NYA TESTAMENTET");
            for (const auto& book : osb_canon())
                if (book.new_testament == bool(part)) {
                    items.emplace_back(ui::utf8(book_abbreviation(book.code)), [&, book] {
                        show_chapters(book);
                    });
                    cells_books.emplace_back(items.back().first, &book);
                }
        }
        cells(items, 8, groups);
        // Not yet available in the left pane's language: shown, but greyed out.
        for (auto* child : grid->GetChildren())
            if (auto* cell = wxDynamicCast(child, wxButton))
                for (const auto& [name, book] : cells_books)
                    if (cell->GetLabel() == name) {
                        cell->SetToolTip(ui::utf8(corpus.book_name(book->code)));
                        if (!available(*book)) {
                            cell->Disable();
                            cell->SetToolTip(
                                ui::utf8(corpus.book_name(book->code) + " · saknas ännu på detta språk"));
                        }
                    }
    };
    show_chapters = [&](CanonBook book) {
        back->Show();
        go_back = show_books;
        title->SetLabel(ui::utf8(corpus.book_name(book.code)));
        const auto verses = verses_of(book);
        std::vector<int> chapters;
        for (const auto& ref : verses)
            if (chapters.empty() || chapters.back() != ref.chapter)
                chapters.push_back(ref.chapter);
        std::vector<std::pair<wxString, std::function<void()>>> items;
        for (int chapter : chapters)
            items.emplace_back(wxString::Format("%d", chapter - book.offset()), [&, book, chapter, verses] {
                title->SetLabel(ui::utf8(corpus.book_name(book.code)) +
                                wxString::Format(" %d", chapter - book.offset()));
                go_back = [&, book] {
                    show_chapters(book);
                };
                std::vector<std::pair<wxString, std::function<void()>>> numbers;
                std::vector<VerseRef> in_chapter;
                for (const auto& ref : verses)
                    if (ref.chapter == chapter) {
                        in_chapter.push_back(ref);
                        numbers.emplace_back(wxString::Format("%d", ref.verse) + ui::utf8(ref.suffix),
                                             [&, book, ref] {
                                                 choose(book, ref, ref);
                                             });
                    }
                cells(numbers, 10, {}, {ui::utf8("Hela kapitlet"), [&, book, in_chapter] {
                                            choose(book, in_chapter.front(), in_chapter.back());
                                        }});
            });
        cells(items, 10, {}, {ui::utf8("Hela boken"), [&, book, verses] {
                                  choose(book, verses.front(), verses.back());
                              }});
    };
    show_books();
    dialog.Centre();
    if (dialog.ShowModal() == wxID_OK)
        return chosen;
    return std::nullopt;
}
} // namespace ortho
