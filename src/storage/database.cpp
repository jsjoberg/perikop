#include "storage/database.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <algorithm>
namespace ortho {
namespace {
std::string utf8_path(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
DatabaseHandle open(const std::filesystem::path& path, int flags) {
    sqlite3* raw = nullptr;
    const int status = sqlite3_open_v2(utf8_path(path).c_str(), &raw, flags, nullptr);
    DatabaseHandle result{raw};
    if (status != SQLITE_OK) throw std::runtime_error("Cannot open database: " + std::string(raw ? sqlite3_errmsg(raw) : "out of memory"));
    return result;
}
class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    void text(int index, const std::string& value) { if (sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_)); }
    void number(int index, int value) { sqlite3_bind_int(stmt_, index, value); }
    bool row() {
        const int status = sqlite3_step(stmt_);
        if (status == SQLITE_ROW) return true;
        if (status == SQLITE_DONE) return false;
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    int number(int column) const { return sqlite3_column_int(stmt_, column); }
    std::string text(int column) const {
        const auto* value = sqlite3_column_text(stmt_, column);
        return value ? reinterpret_cast<const char*>(value) : "";
    }
private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};
void exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
}
}
void SqliteCloser::operator()(sqlite3* db) const { if (db) sqlite3_close(db); }
CorpusDb::CorpusDb(const std::filesystem::path& path) : db_(open(path, SQLITE_OPEN_READONLY)) {
    exec(db_.get(), "PRAGMA query_only=ON");
    Statement version(db_.get(), "PRAGMA user_version");
    if (!version.row() || version.number(0) != 1) throw std::runtime_error("Unsupported corpus schema");
}
bool CorpusDb::read_only() const { return sqlite3_db_readonly(db_.get(), "main") == 1; }
std::vector<Source> CorpusDb::sources() const {
    Statement query(db_.get(), "SELECT id,code,language,name,versification FROM source ORDER BY id");
    std::vector<Source> result;
    while (query.row()) result.push_back({query.number(0), query.text(1), query.text(2), query.text(3), query.text(4)});
    return result;
}
std::expected<Verse, std::string> CorpusDb::verse(const std::string& source, const std::string& book, VerseRef ref) const {
    Statement query(db_.get(), "SELECT text FROM verse JOIN source ON source.id=verse.source_id JOIN book ON book.id=verse.book_id WHERE source.code=? AND book.code=? AND chapter=? AND verse=?");
    query.text(1, source); query.text(2, book); query.number(3,ref.chapter); query.number(4,ref.verse);
    if (!query.row()) return std::unexpected("Text saknas i denna utgåva");
    return Verse{ref, query.text(0), {}};
}
std::vector<VerseRef> CorpusDb::coordinates(const std::string& source, const std::string& book) const {
    Statement query(db_.get(), "SELECT chapter,verse FROM verse JOIN source ON source.id=verse.source_id JOIN book ON book.id=verse.book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse");
    query.text(1, source); query.text(2, book);
    std::vector<VerseRef> result;
    while (query.row()) result.push_back({query.number(0), query.number(1)});
    return result;
}
std::optional<Alignment> CorpusDb::alignment(const std::string& from, const std::string& to, const Passage& passage) const {
    Statement query(db_.get(), "SELECT kind,from_first_chapter,from_first_verse,from_last_chapter,from_last_verse,to_first_chapter,to_first_verse,to_last_chapter,to_last_verse FROM alignment JOIN source f ON f.id=from_source JOIN source t ON t.id=to_source JOIN book ON book.id=book_id WHERE f.code=? AND t.code=? AND book.code=? AND (from_first_chapter,from_first_verse)<=(?,?) AND (from_last_chapter,from_last_verse)>=(?,?) ORDER BY from_first_chapter DESC,from_first_verse DESC LIMIT 1");
    query.text(1,from); query.text(2,to); query.text(3,passage.book);
    query.number(4,passage.first.chapter); query.number(5,passage.first.verse);
    query.number(6,passage.last.chapter); query.number(7,passage.last.verse);
    if (!query.row()) return {};
    return Alignment{static_cast<AlignmentKind>(query.number(0)),
        {passage.book,{query.number(1),query.number(2)},{query.number(3),query.number(4)}},
        {passage.book,{query.number(5),query.number(6)},{query.number(7),query.number(8)}}};
}
std::expected<Verse, std::string> CorpusDb::parallel_verse(const std::string& from, const std::string& to, const std::string& book, VerseRef ref) const {
    const auto map = alignment(from, to, {book,ref,ref});
    if (map) {
        if (map->kind == AlignmentKind::Split && map->from.first == map->from.last) {
            Verse result{map->to.first, "", map->to.last};
            for (auto target : coordinates(to,book)) if (map->to.contains(target)) {
                auto part=verse(to,book,target);
                if (!part) return std::unexpected(part.error());
                if (!result.text.empty()) result.text += " ";
                result.text += part->text;
            }
            if (result.text.empty()) return std::unexpected("Text saknas i denna utgåva");
            return result;
        }
        if (map->kind != AlignmentKind::Same && map->kind != AlignmentKind::Renumbered)
            return std::unexpected("Versindelningen skiljer sig");
        ref.chapter += map->to.first.chapter - map->from.first.chapter;
        ref.verse += map->to.first.verse - map->from.first.verse;
    } else {
        // Same-coordinate lookup is only safe inside matching versification systems.
        auto list = sources();
        const auto f = std::find_if(list.begin(),list.end(),[&](auto& s){return s.code==from;});
        const auto t = std::find_if(list.begin(),list.end(),[&](auto& s){return s.code==to;});
        if (f==list.end() || t==list.end()) return std::unexpected("Okänd textkälla");
        if(book=="Ps"&&from!=to)return std::unexpected("Versmappning saknas · öppna utgåvan via Bibel");
        const bool nt = new_testament_book(book);
        if (f->versification != t->versification && !nt) return std::unexpected("Ingen belagd textmappning");
    }
    return verse(to,book,ref);
}
std::vector<Pronunciation> CorpusDb::pronunciations(const std::string& language) const {
    Statement query(db_.get(), "SELECT language,source,spoken,phonemes,priority FROM pronunciation WHERE language=? ORDER BY priority DESC,length(source) DESC");
    query.text(1,language); std::vector<Pronunciation> result;
    while (query.row()) result.push_back({query.text(0),query.text(1),query.text(2),query.text(3),query.number(4)});
    return result;
}
std::string CorpusDb::book_name(const std::string& book, const std::string& language) const {
    const char* sql = language=="el" ? "SELECT name_el FROM book WHERE code=?" : language=="en" ? "SELECT name_en FROM book WHERE code=?" : "SELECT name_sv FROM book WHERE code=?";
    Statement query(db_.get(), sql); query.text(1,book);
    return query.row() ? query.text(0) : book;
}
std::vector<Book> CorpusDb::books(const std::string& language) const {
    Statement q(db_.get(),"SELECT code,CASE WHEN ?='en' THEN name_en WHEN ?='el' AND name_el<>'' THEN name_el ELSE name_sv END,canonical_order FROM book WHERE EXISTS(SELECT 1 FROM verse WHERE book_id=book.id) ORDER BY canonical_order");
    q.text(1,language);q.text(2,language);std::vector<Book> result;
    while(q.row())result.push_back({q.text(0),q.text(1),q.number(2)});
    return result;
}
std::vector<ReadingRule> CorpusDb::reading_rules() const {
    Statement q(db_.get(),"SELECT id,pdist,month,day,ordering,service,description,tradition,label FROM reading_rule ORDER BY ordering,id");
    std::vector<ReadingRule> result;
    while(q.row()) {
        const auto service=q.text(5);
        const auto kind=service=="Epistle"?ReadingKind::Epistle:service=="Gospel"?ReadingKind::Gospel:service=="Vespers"?ReadingKind::Vespers:ReadingKind::OldTestament;
        ReadingRule rule{q.number(0),q.number(1),q.number(2),q.number(3),q.number(4),service,q.text(6),q.text(7),{kind,{},q.text(8),{}}};
        Statement parts(db_.get(),"SELECT book,first_chapter,first_verse,last_chapter,last_verse FROM reading_segment WHERE rule_id=? ORDER BY ordering");
        parts.number(1,rule.id);bool first=true;
        while(parts.row()) {
            Passage p{parts.text(0),{parts.number(1),parts.number(2)},{parts.number(3),parts.number(4)}};
            if(first){rule.reading.passage=p;first=false;}else rule.reading.additional.push_back(p);
        }
        result.push_back(std::move(rule));
    }
    return result;
}
std::vector<FeastRule> CorpusDb::feast_rules() const {
    Statement q(db_.get(),"SELECT pdist,month,day,coalesce(rank,-100),title,feast,tradition FROM feast_rule ORDER BY id");
    std::vector<FeastRule> result;
    while(q.row())result.push_back({q.number(0),q.number(1),q.number(2),q.number(3),q.text(4),q.text(5),q.text(6)});
    return result;
}
std::vector<OrdoRule> CorpusDb::ordo_rules() const {
    Statement q(db_.get(),"SELECT year,month,day,pdist,service FROM ordo_rule");std::vector<OrdoRule> result;
    while(q.row())result.push_back({q.number(0),q.number(1),q.number(2),q.number(3),q.text(4)});
    return result;
}
UserDb::UserDb(const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    db_=open(path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
    exec(db_.get(), "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)");
}
Settings UserDb::load() const {
    Settings result;
    Statement query(db_.get(), "SELECT key,value FROM settings");
    while (query.row()) {
        const auto key=query.text(0), value=query.text(1);
        if (key=="theme") result.theme=value=="dark"?Theme::Dark:value=="light"?Theme::Light:Theme::System;
        if (key=="calendar") result.calendar=value=="old"?CalendarStyle::Old:CalendarStyle::New;
        if (key=="parallel" && (value.empty() || value=="el" || value=="en" || value=="el,en")) result.parallel=value;
        if (key=="font_size") { try { result.font_size=std::clamp(std::stoi(value),14,28); } catch (...) {} }
    }
    return result;
}
void UserDb::save(const Settings& settings) {
    exec(db_.get(), "BEGIN IMMEDIATE");
    try {
        const std::vector<std::pair<std::string,std::string>> values={
            {"theme",settings.theme==Theme::Dark?"dark":settings.theme==Theme::Light?"light":"system"},
            {"calendar",settings.calendar==CalendarStyle::Old?"old":"new"},
            {"parallel",settings.parallel},{"font_size",std::to_string(settings.font_size)}};
        for (const auto& [key,value]:values) {
            Statement query(db_.get(), "INSERT INTO settings VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
            query.text(1,key); query.text(2,value); query.row();
        }
        exec(db_.get(), "COMMIT");
    } catch (...) { exec(db_.get(),"ROLLBACK"); throw; }
}
}
