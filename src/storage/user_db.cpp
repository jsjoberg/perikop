#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::pragma_number;
using storage::Statement;
using storage::Transaction;
UserDb::UserDb(const std::filesystem::path& path) {
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    db_ = open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    const int version = pragma_number(db_.get(), "PRAGMA user_version");
    const int identity = pragma_number(db_.get(), "PRAGMA application_id");
    if ((version != 0 && version != 1) || (identity != 0 && identity != 0x4f525455) ||
        (version == 1 && identity != 0x4f525455))
        throw std::runtime_error("Unsupported settings database");
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
    // Version zero is the original settings table. Preserve every stored value.
    Transaction migration(db_.get());
    if (pragma_number(db_.get(), "PRAGMA user_version") == 0) {
        exec(db_.get(),
             "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
             "ALTER TABLE settings RENAME TO settings_legacy;"
             "CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;"
             "INSERT INTO settings SELECT key,value FROM settings_legacy; DROP TABLE settings_legacy;"
             "PRAGMA application_id=1330795605; PRAGMA user_version=1");
    }
    migration.commit();
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
        if (key == "primary" && (value == "sv" || value == "el" || value == "en"))
            result.primary = value;
        // The three-pane mode "el,en" is retired; it keeps its Greek pane.
        if (key == "parallel" &&
            (value.empty() || value == "sv" || value == "el" || value == "en" || value == "el,en"))
            result.parallel = value == "el,en" ? "el" : value;
        if (key == "font_size") {
            try {
                result.font_size = std::clamp(std::stoi(value), 14, 28);
            } catch (...) {
            }
        }
        if (key == "speech_rate") {
            try {
                result.speech_rate = std::clamp(std::stoi(value), 25, 200);
            } catch (...) {
            }
        }
        if (key == "speech_voice" && (value == "alice" || value == "bjorn"))
            result.speech_voice = value;
        if (key == "word_study")
            result.word_study = value == "1";
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
        {"primary", settings.primary},
        {"parallel", settings.parallel},
        {"font_size", std::to_string(settings.font_size)},
        {"speech_rate", std::to_string(settings.speech_rate)},
        {"speech_voice", settings.speech_voice},
        {"word_study", settings.word_study ? "1" : "0"}};
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
