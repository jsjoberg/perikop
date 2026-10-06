#pragma once
#include "core/model.hpp"
#include <filesystem>
#include <memory>
#include <utility>
struct sqlite3;
namespace ortho {
struct SqliteCloser { void operator()(sqlite3*) const; };
using DatabaseHandle = std::unique_ptr<sqlite3, SqliteCloser>;
class CorpusDb {
public:
    explicit CorpusDb(const std::filesystem::path& path);
    std::vector<Source> sources() const;
    std::expected<Verse, std::string> verse(const std::string& source, const std::string& book, VerseRef ref) const;
    std::vector<VerseRef> coordinates(const std::string& source, const std::string& book) const;
    std::vector<VerseRef> paragraph_starts(const std::string& source, const std::string& book) const;
    std::optional<Alignment> alignment(const std::string& from, const std::string& to, const Passage&) const;
    std::expected<Verse, std::string> parallel_verse(const std::string& from, const std::string& to, const std::string& book, VerseRef) const;
    // Passages in another edition's numbering, joined where the target verses are adjacent.
    std::vector<Passage> map_passage(const std::string& from, const std::string& to, const Passage&) const;
    // The reading in the numbering of the edition that opens it.
    Reading localize(Reading) const;
    std::vector<Pronunciation> pronunciations(const std::string& language) const;
    std::string book_name(const std::string& book, const std::string& language = "sv") const;
    bool read_only() const;
    std::vector<Book> books(const std::string& language="sv") const;
    std::vector<ReadingRule> reading_rules() const;
    std::vector<FeastRule> feast_rules() const;
    std::vector<OrdoRule> ordo_rules() const;
private:
    bool same_numbering(const std::string& from, const std::string& to, const std::string& book) const;
    std::vector<std::pair<std::string,VerseRef>> counterparts(const std::string& from, const std::string& to, const std::string& book, VerseRef) const;
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
    std::optional<std::vector<float>> load(const std::string& model, const std::string& language, const std::string& text);
    void save(const std::string& model, const std::string& language, const std::string& text, const std::vector<float>&);
private:
    DatabaseHandle db_;
};
}
