#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::Statement;
using storage::Transaction;
UserDb::UserDb(const std::filesystem::path& path) {
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    db_ = open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    exec(db_.get(), "PRAGMA foreign_keys=ON; PRAGMA cache_size=-256");
    {
        Statement journal(db_.get(), "PRAGMA journal_mode=WAL");
        if (!journal.row() || journal.text(0) != "wal")
            throw std::runtime_error("Cannot enable settings write-ahead log");
    }
    exec(db_.get(),
         "PRAGMA synchronous=FULL; PRAGMA wal_autocheckpoint=64; PRAGMA journal_size_limit=262144");
#ifdef __APPLE__
    exec(db_.get(), "PRAGMA fullfsync=ON; PRAGMA checkpoint_fullfsync=ON");
#endif
    // No schema versions or migrations until Perikop has users: the tables are created as they are now.
    exec(db_.get(),
         "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;"
         "CREATE TABLE IF NOT EXISTS progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT");
}
std::set<std::string> UserDb::completed() const {
    std::set<std::string> result;
    Statement query(db_.get(), "SELECT key FROM progress");
    while (query.row())
        result.insert(query.text(0));
    return result;
}
void UserDb::complete(const std::string& key, bool done) {
    Transaction transaction(db_.get());
    Statement query(db_.get(),
                    done ? "INSERT INTO progress VALUES(?,date('now','localtime')) ON CONFLICT(key) "
                           "DO NOTHING"
                         : "DELETE FROM progress WHERE key=?");
    query.text(1, key);
    query.row();
    transaction.commit();
}
void UserDb::forget(const std::string& prefix) {
    Transaction transaction(db_.get());
    Statement query(db_.get(), "DELETE FROM progress WHERE substr(key,1,length(?1))=?1");
    query.text(1, prefix);
    query.row();
    transaction.commit();
}
Settings UserDb::load() const {
    Settings result;
    Statement query(db_.get(), "SELECT key,value FROM settings");
    while (query.row()) {
        const auto key = query.text(0), value = query.text(1);
        if (key == "theme")
            result.theme = value == "dark" ? Theme::Dark : value == "light" ? Theme::Light : Theme::System;
        if (key == "calendar")
            result.calendar = value == "old" ? CalendarStyle::Old : CalendarStyle::New;
        if (key == "tradition")
            // Retired Antiochian settings now select the Greek family, including its variants.
            result.tradition = value == "slavic" ? Tradition::Slavic : Tradition::Greek;
        if (key == "primary" && (value == "sv" || value == "el" || value == "en"))
            result.primary = value;
        // The three-pane mode "el,en" is retired; it keeps its Greek pane.
        if (key == "parallel" &&
            (value.empty() || value == "sv" || value == "el" || value == "en" || value == "el,en"))
            result.parallel = value == "el,en" ? "el" : value;
        // An unreadable number keeps the default.
        int number = 0;
        const bool numeric =
            std::from_chars(value.data(), value.data() + value.size(), number).ec == std::errc{};
        if (key == "font_size" && numeric)
            result.font_size = std::clamp(number, 14, 28);
        if (key == "speech_rate" && numeric)
            result.speech_rate = std::clamp(number, 25, 200);
        if (key == "speech_voice" && (value == "alice" || value == "bjorn"))
            result.speech_voice = value;
        if (key == "word_study")
            result.word_study = value == "1";
        if (key == "speech_highlight")
            result.speech_highlight = value != "0";
    }
    if (result.parallel == result.primary)
        result.parallel.clear();
    if (result.word_study)
        result.parallel.clear();
    return result;
}
void UserDb::save(const Settings& settings) {
    Transaction transaction(db_.get());
    const std::vector<std::pair<std::string, std::string>> values = {
        {"theme", settings.theme == Theme::Dark    ? "dark"
                  : settings.theme == Theme::Light ? "light"
                                                   : "system"},
        {"calendar", settings.calendar == CalendarStyle::Old ? "old" : "new"},
        {"tradition", settings.tradition == Tradition::Slavic ? "slavic" : "greek"},
        {"primary", settings.primary},
        {"parallel", settings.parallel},
        {"font_size", std::to_string(settings.font_size)},
        {"speech_rate", std::to_string(settings.speech_rate)},
        {"speech_voice", settings.speech_voice},
        {"word_study", settings.word_study ? "1" : "0"},
        {"speech_highlight", settings.speech_highlight ? "1" : "0"}};
    Statement query(db_.get(),
                    "INSERT INTO settings VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    for (const auto& [key, value] : values) {
        query.text(1, key);
        query.text(2, value);
        query.row();
        query.reset();
    }
    transaction.commit();
}
} // namespace ortho
