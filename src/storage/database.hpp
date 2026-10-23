#pragma once
#include "core/model.hpp"
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>
struct sqlite3;
namespace ortho {
struct SqliteCloser {
    void operator()(sqlite3*) const;
};
using DatabaseHandle = std::unique_ptr<sqlite3, SqliteCloser>;
// parallel_verse's error for a verse that the edition's previous verse already includes.
inline constexpr const char* merged_verse = "Ingår i föregående vers";
inline constexpr const char* osb_reference = "osb-v1";
struct WordExample {
    std::string book;
    Verse verse;
};
struct DalinEntry {
    std::string headword, gram, definition;
};
struct BiblicalEntry {
    std::string id, headword, definition, url;
};
struct ModernWordEntry {
    std::string id, headword, gram, associations, match;
};
class CorpusDb {
public:
    explicit CorpusDb(const std::filesystem::path& path);
    // Optional, locally imported Scripture. The bundled corpus stays read-only.
    void load_imported(const std::filesystem::path&);
    bool has_imported() const {
        return bool(imported_);
    }
    std::vector<Source> sources() const;
    std::expected<Verse, std::string> verse(const std::string& source, const std::string& book,
                                            VerseRef ref) const;
    const std::vector<VerseRef>& coordinates(const std::string& source, const std::string& book) const;
    // Printed order can differ from numeric order: Esther 1a–1r precede 1:1 in OSB.
    int compare(const std::string& source, const std::string& book, VerseRef, VerseRef) const;
    bool contains(const std::string& source, const Passage&, VerseRef) const;
    std::vector<VerseRef> paragraph_starts(const std::string& source, const std::string& book) const;
    std::optional<Alignment> alignment(const std::string& from, const std::string& to, const Passage&) const;
    std::expected<Verse, std::string> parallel_verse(const std::string& from, const std::string& to,
                                                     const std::string& book, VerseRef) const;
    // The verses of another edition that translate one verse; empty when it has none.
    std::vector<std::pair<std::string, VerseRef>> counterparts(const std::string& from, const std::string& to,
                                                               const std::string& book, VerseRef) const;
    // Passages in another edition's numbering, joined where the target verses are adjacent.
    std::vector<Passage> map_passage(const std::string& from, const std::string& to, const Passage&) const;
    // Restate any supported source references in the shared OSB authority.
    Reading localize(Reading) const;
    std::vector<Pronunciation> pronunciations(const std::string& language) const;
    std::vector<WordExample> word_examples(const std::string& word, int limit = 3) const;
    std::string book_name(const std::string& book, const std::string& language = "sv") const;
    bool read_only() const;
    // Book data from books.tsv, loaded when the corpus opens. Unknown books have no flags.
    bool new_testament_book(const std::string& book) const;
    bool deuterocanonical_book(const std::string& book) const;
    // Whether each verse starts its own paragraph, as in the Psalms.
    bool stanza_book(const std::string& book, int chapter = 0) const;
    // Short Swedish name of a book, such as "1 Mos", for grids and plan parts.
    std::string book_abbreviation(const std::string& book) const;
    // Books and named Daniel sections in the OSB reference metadata.
    const std::vector<CanonBook>& canon() const {
        return canon_;
    }
    const CanonBook* canon_book(const std::string& book, int chapter) const;
    // Familiar Swedish names; reference_name gives the corresponding OSB English name.
    std::string title(const std::string& book) const;
    std::string short_title(const std::string& book) const;
    std::string reference_name(const std::string& book) const;
    // The edition a pane shows for a language; empty for an unknown language.
    std::string source_for_language(const std::string& language, const std::string& book) const;
    std::vector<ReadingRule> reading_rules() const;
    std::vector<FeastRule> feast_rules() const;
    std::vector<OrdoRule> ordo_rules() const;
    // Word study for a lowercase Swedish 1917 form: Dalin, Nyström's biblical articles,
    // and SALDO. The entries are resolved when the corpus is built.
    std::vector<DalinEntry> swedish(const std::string& form) const;
    std::vector<BiblicalEntry> biblical(const std::string& form) const;
    std::vector<ModernWordEntry> saldo(const std::string& form) const;

private:
    bool same_numbering(const std::string& from, const std::string& to, const std::string& book) const;
    // A book of one edition, in chapter, verse and letter order, read in one query when first needed.
    // Readers ask verse by verse; one query per verse made a parallel Psalter take 100 ms.
    struct BookText {
        std::vector<VerseRef> coordinates;
        std::vector<int> last_verses;
        std::vector<std::string> texts;
        std::map<VerseRef, std::size_t> order;
    };
    const BookText& book_text(const std::string& source, const std::string& book) const;
    // The alignment rows from one edition to another for a book, ordered by their first verse.
    const std::vector<Alignment>& book_alignment(const std::string& from, const std::string& to,
                                                 const std::string& book) const;
    DatabaseHandle db_;
    DatabaseHandle imported_;
    DatabaseHandle study_;
    DatabaseHandle reference_;
    void load_reference(const std::filesystem::path&);
    using ReferenceTargets = std::vector<std::pair<std::string, VerseRef>>;
    using ReferenceLinks = std::map<VerseRef, ReferenceTargets>;
    const ReferenceLinks& reference_links(const std::string& from, const std::string& to,
                                          const std::string& book) const;
    sqlite3* study_database(const char* table, const std::string& form) const;
    // Loaded books are never changed or removed, so references to them stay valid.
    mutable std::mutex cache_mutex_;
    mutable std::map<std::pair<std::string, std::string>, BookText> texts_;
    mutable std::map<std::tuple<std::string, std::string, std::string>, std::vector<Alignment>> alignments_;
    mutable std::map<std::tuple<std::string, std::string, std::string>, ReferenceLinks> reference_links_;
    // Each edition's versification system, by source code. The corpus is read-only.
    std::map<std::string, std::string> versification_;
    struct BookInfo {
        std::string abbreviation;
        bool new_testament, deuterocanonical, stanzas;
        std::array<std::string, 3> names; // Swedish, English, Greek.
    };
    std::map<std::string, BookInfo> books_;
    std::vector<CanonBook> canon_;
};
struct WindowPlacement {
    int x, y;          // Desktop coordinates; negative values are valid on secondary monitors.
    int width, height; // Unmaximized size in device-independent pixels.
    bool maximized = false;
    bool operator==(const WindowPlacement&) const = default;
};
class UserDb {
public:
    explicit UserDb(const std::filesystem::path& path);
    Settings load() const;
    void save(const Settings&);
    std::optional<WindowPlacement> window_placement() const;
    void window_placement(const WindowPlacement&);
    // Keys of what the reader has marked as read, such as "plan:nt:v3:7".
    std::set<std::string> completed() const;
    void complete(const std::string& key, bool done = true);
    // Forgets every mark whose key starts with the prefix, such as a whole plan.
    void forget(const std::string& prefix);
    std::optional<ReadingPosition> position(const std::string& key) const;
    void position(const std::string& key, const ReadingPosition&);
    void reset_position(const std::string& key);
    // One-time transfer from the original plan ranges; retains the legacy rows.
    void migrate_reading_plans(const CorpusDb&);
    std::vector<Bookmark> bookmarks(const CorpusDb&) const;
    void bookmark(const std::string& source, const Passage&);
    void remove_bookmark(int id);

private:
    void migrate_legacy_plans(const CorpusDb&);
    void migrate_osb(const CorpusDb&);
    DatabaseHandle db_;
};
struct PronunciationDecision {
    std::string form, status, spoken, note;
};
class PronunciationReviewDb {
public:
    explicit PronunciationReviewDb(const std::filesystem::path&);
    std::vector<PronunciationDecision> decisions() const;
    void save(const PronunciationDecision&);
    std::vector<Pronunciation> overrides() const;
    void export_tsv(const std::filesystem::path&) const;

private:
    DatabaseHandle db_;
};
} // namespace ortho
