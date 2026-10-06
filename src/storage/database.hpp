#pragma once
#include "core/model.hpp"
#include <filesystem>
#include <memory>
#include <utility>
struct sqlite3;
namespace ortho {
struct SqliteCloser {
    void operator()(sqlite3*) const;
};
using DatabaseHandle = std::unique_ptr<sqlite3, SqliteCloser>;
struct WordExample {
    std::string book;
    Verse verse;
};
class CorpusDb {
public:
    explicit CorpusDb(const std::filesystem::path& path);
    std::vector<Source> sources() const;
    std::expected<Verse, std::string> verse(const std::string& source, const std::string& book,
                                            VerseRef ref) const;
    std::vector<VerseRef> coordinates(const std::string& source, const std::string& book) const;
    std::vector<VerseRef> paragraph_starts(const std::string& source, const std::string& book) const;
    std::optional<Alignment> alignment(const std::string& from, const std::string& to, const Passage&) const;
    std::expected<Verse, std::string> parallel_verse(const std::string& from, const std::string& to,
                                                     const std::string& book, VerseRef) const;
    // The verses of another edition that translate one verse; empty when it has none.
    std::vector<std::pair<std::string, VerseRef>> counterparts(const std::string& from, const std::string& to,
                                                               const std::string& book, VerseRef) const;
    // Passages in another edition's numbering, joined where the target verses are adjacent.
    std::vector<Passage> map_passage(const std::string& from, const std::string& to, const Passage&) const;
    // The reading in its framing numbering: the Septuagint's for the Old Testament.
    Reading localize(Reading) const;
    std::vector<Pronunciation> pronunciations(const std::string& language) const;
    std::vector<WordExample> word_examples(const std::string& word, int limit = 3) const;
    std::string book_name(const std::string& book, const std::string& language = "sv") const;
    bool read_only() const;
    std::vector<Book> books(const std::string& language = "sv") const;
    std::vector<ReadingRule> reading_rules() const;
    std::vector<FeastRule> feast_rules() const;
    std::vector<OrdoRule> ordo_rules() const;

private:
    bool same_numbering(const std::string& from, const std::string& to, const std::string& book) const;
    DatabaseHandle db_;
};
struct DalinEntry {
    std::string headword, gram, definition;
};
struct BiblicalEntry {
    std::string id, headword, definition, url;
};
struct StrongsEntry {
    std::string strong, lemma, transliteration, gloss, definition;
};
struct GreekWord {
    std::string surface, strong;
};
// Word-study data from tools/lexicon/build_study.py: Dalin and biblical articles
// for Swedish 1917 forms, and Strong's tags for the Greek New Testament.
class StudyDb {
public:
    explicit StudyDb(const std::filesystem::path& path);
    // Dalin entries for a lowercase Swedish 1917 word form.
    std::vector<DalinEntry> swedish(const std::string& form) const;
    // Nyström's historical biblical articles for a lowercase Swedish 1917 form.
    std::vector<BiblicalEntry> biblical(const std::string& form) const;
    std::optional<StrongsEntry> strongs(const std::string& strong) const;
    // The tagged words of a grc-patriarchal verse, in text order.
    std::vector<GreekWord> greek_words(const std::string& book, VerseRef) const;
    // The Greek word that the word aligner links to an occurrence of a lowercase
    // form in a sv1917 New Testament verse: its Greek verse and position there.
    std::optional<std::pair<VerseRef, int>> greek_link(const std::string& book, VerseRef,
                                                       const std::string& form, int occurrence) const;

private:
    DatabaseHandle db_;
};
class UserDb {
public:
    explicit UserDb(const std::filesystem::path& path);
    Settings load() const;
    void save(const Settings&);

private:
    DatabaseHandle db_;
};
// Speech audio has its own bounded database. It never changes Scripture or preferences.
class SpeechCache {
public:
    explicit SpeechCache(const std::filesystem::path&);
    std::optional<std::vector<float>> load(const std::string& model, const std::string& language,
                                           const std::string& text);
    void save(const std::string& model, const std::string& language, const std::string& text,
              const std::vector<float>&);

private:
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
