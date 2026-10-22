#include "core/reading_display.hpp"
#include "core/source_preferences.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::Statement;
using storage::Transaction;
namespace {
// Stored names, in ColorScheme order. An unknown name keeps the default.
constexpr std::array<const char*, 7> color_schemes{"rose-pine",  "gruvbox",     "nord",     "everforest",
                                                   "catppuccin", "tokyo-night", "solarized"};
} // namespace
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
    // Additive tables preserve existing profiles. Plan-layout migration runs separately with the corpus.
    exec(db_.get(),
         "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;"
         "CREATE TABLE IF NOT EXISTS progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT;"
         "CREATE TABLE IF NOT EXISTS reading_position(key TEXT PRIMARY KEY NOT NULL,section INTEGER NOT NULL,"
         "book TEXT NOT NULL,source TEXT NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,"
         "suffix TEXT NOT NULL,percent INTEGER NOT NULL) STRICT;"
         "CREATE TABLE IF NOT EXISTS bookmark(id INTEGER PRIMARY KEY,source TEXT NOT NULL,"
         "book TEXT NOT NULL,first_chapter INTEGER NOT NULL,first_verse INTEGER NOT NULL,"
         "first_suffix TEXT NOT NULL,last_chapter INTEGER NOT NULL,last_verse INTEGER NOT NULL,"
         "last_suffix TEXT NOT NULL,UNIQUE(source,book,first_chapter,first_verse,first_suffix,"
         "last_chapter,last_verse,last_suffix)) STRICT");
}
void UserDb::bookmark(const std::string& source, const Passage& passage) {
    const bool esther_prelude = source == osb_reference && passage.book == "Esth" &&
                                passage.first.chapter == 1 && passage.first.verse == 1 &&
                                passage.first.suffix.size() == 1 && passage.first.suffix[0] >= 'a' &&
                                passage.first.suffix[0] <= 'r' && passage.last == VerseRef{1, 1};
    if (source.empty() || passage.book.empty() || passage.first.chapter < 1 || passage.first.verse < 1 ||
        passage.last.verse < 1 || (passage.last < passage.first && !esther_prelude))
        throw std::invalid_argument("Invalid bookmark passage");
    Statement query(db_.get(), "INSERT INTO bookmark(source,book,first_chapter,first_verse,first_suffix,"
                               "last_chapter,last_verse,last_suffix) VALUES(?,?,?,?,?,?,?,?) "
                               "ON CONFLICT DO NOTHING");
    query.text(1, source);
    query.text(2, passage.book);
    query.number(3, passage.first.chapter);
    query.number(4, passage.first.verse);
    query.text(5, passage.first.suffix);
    query.number(6, passage.last.chapter);
    query.number(7, passage.last.verse);
    query.text(8, passage.last.suffix);
    query.row();
}
void UserDb::remove_bookmark(int id) {
    Statement query(db_.get(), "DELETE FROM bookmark WHERE id=?");
    query.number(1, id);
    query.row();
}
std::vector<Bookmark> UserDb::bookmarks(const CorpusDb& corpus) const {
    std::vector<Bookmark> result;
    Statement query(db_.get(), "SELECT id,source,book,first_chapter,first_verse,first_suffix,"
                               "last_chapter,last_verse,last_suffix FROM bookmark");
    while (query.row())
        result.push_back({query.number(0),
                          query.text(1),
                          {query.text(2),
                           {query.number(3), query.number(4), query.text(5)},
                           {query.number(6), query.number(7), query.text(8)}}});
    const auto order = [&corpus](const Bookmark& mark) {
        const auto mapped = bookmark_passages(corpus, mark);
        const auto passage = mapped.empty() ? mark.passage : mapped.front();
        const auto* book = corpus.canon_book(passage.book, passage.first.chapter);
        const auto& canon = corpus.canon();
        const auto index = book ? std::size_t(book - canon.data()) : canon.size();
        const auto& refs = corpus.coordinates(osb_reference, passage.book);
        const auto at = std::find(refs.begin(), refs.end(), passage.first) - refs.begin();
        return std::tuple{index, at, mark.id};
    };
    std::ranges::sort(result, {}, order);
    return result;
}
std::optional<ReadingPosition> UserDb::position(const std::string& key) const {
    Statement query(
        db_.get(),
        "SELECT section,book,source,chapter,verse,suffix,percent FROM reading_position WHERE key=?");
    query.text(1, key);
    if (!query.row())
        return {};
    return ReadingPosition{std::size_t(query.number(0)),
                           query.text(1),
                           query.text(2),
                           {query.number(3), query.number(4), query.text(5)},
                           query.number(6)};
}
void UserDb::position(const std::string& key, const ReadingPosition& position) {
    Statement query(
        db_.get(),
        "INSERT INTO reading_position VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(key) DO UPDATE SET "
        "section=excluded.section,book=excluded.book,source=excluded.source,chapter=excluded.chapter,"
        "verse=excluded.verse,suffix=excluded.suffix,percent=excluded.percent");
    query.text(1, key);
    query.number(2, int(position.section));
    query.text(3, position.book);
    query.text(4, position.source);
    query.number(5, position.verse.chapter);
    query.number(6, position.verse.verse);
    query.text(7, position.verse.suffix);
    query.number(8, std::clamp(position.percent, 0, 99));
    query.row();
}
void UserDb::reset_position(const std::string& key) {
    Statement query(db_.get(), "DELETE FROM reading_position WHERE key=?");
    query.text(1, key);
    query.row();
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
    Statement positions(db_.get(), "DELETE FROM reading_position WHERE substr(key,1,length(?1))=?1");
    positions.text(1, prefix);
    positions.row();
    transaction.commit();
}
Settings UserDb::load() const {
    Settings result;
    Statement query(db_.get(), "SELECT key,value FROM settings");
    while (query.row()) {
        const auto key = query.text(0), value = query.text(1);
        if (key == "theme")
            result.theme = value == "dark" ? Theme::Dark : value == "light" ? Theme::Light : Theme::System;
        if (key == "color_scheme")
            for (int i = 0; i < int(color_schemes.size()); ++i)
                if (value == color_schemes[i])
                    result.color_scheme = ColorScheme(i);
        if (key == "calendar")
            result.calendar = value == "old" ? CalendarStyle::Old : CalendarStyle::New;
        if (key == "tradition")
            // Retired Antiochian settings now select the Greek family, including its variants.
            result.tradition = value == "slavic" ? Tradition::Slavic : Tradition::Greek;
        // Older profiles retain their split-pane choice as a source priority.
        if (key == "parallel" && !value.empty()) {
            result.split_view = true;
            if (value.starts_with("el"))
                result.secondary_sources =
                    decode_sources("grc-lxx=1,grc-patriarchal=1", result.secondary_sources);
        }
        if (key == "split_view")
            result.split_view = value == "1";
        if (key == "primary_sources")
            result.primary_sources = decode_sources(value, result.primary_sources);
        if (key == "secondary_sources")
            result.secondary_sources = decode_sources(value, result.secondary_sources);
        // An unreadable number keeps the default.
        int number = 0;
        const bool numeric =
            std::from_chars(value.data(), value.data() + value.size(), number).ec == std::errc{};
        if (key == "text_zoom" && numeric)
            result.text_zoom = std::clamp(number, 75, 200);
        if (key == "speech_rate" && numeric)
            result.speech_rate = std::clamp(number, 25, 200);
        if (key == "speech_voice" &&
            (value == "alice" || value == "bjorn" || value == "espeak" || value == "espeak-f2" ||
             (value.starts_with("espeak:") && value.size() > 7)))
            result.speech_voice = value;
        if (key == "update_channel")
            result.update_channel = value == "nightly" ? UpdateChannel::Nightly : UpdateChannel::Releases;
        if (key == "word_study")
            result.word_study = value == "1";
        if (key == "speech_highlight")
            result.speech_highlight = value != "0";
    }
    if (std::ranges::none_of(result.primary_sources, &SourcePreference::enabled))
        result.primary_sources.front().enabled = true;
    if (result.word_study)
        result.split_view = false;
    return result;
}
void UserDb::save(const Settings& settings) {
    Transaction transaction(db_.get());
    const std::vector<std::pair<std::string, std::string>> values = {
        {"theme", settings.theme == Theme::Dark    ? "dark"
                  : settings.theme == Theme::Light ? "light"
                                                   : "system"},
        {"color_scheme", color_schemes[int(settings.color_scheme)]},
        {"calendar", settings.calendar == CalendarStyle::Old ? "old" : "new"},
        {"tradition", settings.tradition == Tradition::Slavic ? "slavic" : "greek"},
        {"parallel", ""},
        {"split_view", settings.split_view ? "1" : "0"},
        {"primary_sources", encode_sources(settings.primary_sources)},
        {"secondary_sources", encode_sources(settings.secondary_sources)},
        {"text_zoom", std::to_string(settings.text_zoom)},
        {"speech_rate", std::to_string(settings.speech_rate)},
        {"speech_voice", settings.speech_voice},
        {"update_channel", settings.update_channel == UpdateChannel::Nightly ? "nightly" : "releases"},
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
