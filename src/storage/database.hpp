#pragma once
#include "core/model.hpp"
#include <filesystem>
#include <memory>
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
    std::optional<Alignment> alignment(const std::string& from, const std::string& to, const Passage&) const;
    std::expected<Verse, std::string> parallel_verse(const std::string& from, const std::string& to, const std::string& book, VerseRef) const;
    std::vector<Pronunciation> pronunciations(const std::string& language) const;
    std::string book_name(const std::string& book, const std::string& language = "sv") const;
    bool read_only() const;
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
}
