#include "storage/database.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include "speech/speech.hpp"
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
    sqlite3_extended_result_codes(raw, 1);
    if (sqlite3_busy_timeout(raw, 3000) != SQLITE_OK ||
        sqlite3_db_config(raw, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr) != SQLITE_OK ||
        sqlite3_db_config(raw, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(raw));
    return result;
}
class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            sqlite3_finalize(stmt_);
            throw std::runtime_error(sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    void text(int index, const std::string& value) { if (sqlite3_bind_text64(stmt_, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_)); }
    void number(int index, int value) { if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_)); }
    void blob(int index, const std::vector<unsigned char>& value) { if(sqlite3_bind_blob64(stmt_,index,value.data(),value.size(),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db_)); }
    std::vector<unsigned char> blob(int column) const {
        const auto* bytes=static_cast<const unsigned char*>(sqlite3_column_blob(stmt_,column));
        const int count=sqlite3_column_bytes(stmt_,column);
        return bytes&&count?std::vector<unsigned char>(bytes,bytes+count):std::vector<unsigned char>{};
    }
    void reset() { if (sqlite3_reset(stmt_) != SQLITE_OK || sqlite3_clear_bindings(stmt_) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_)); }
    bool row() {
        const int status = sqlite3_step(stmt_);
        if (status == SQLITE_ROW) return true;
        if (status == SQLITE_DONE) return false;
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    int number(int column) const { return sqlite3_column_int(stmt_, column); }
    std::string text(int column) const {
        const auto* value = sqlite3_column_text(stmt_, column);
        return value ? std::string(reinterpret_cast<const char*>(value), sqlite3_column_bytes(stmt_, column)) : "";
    }
private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};
void exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
}
int pragma_number(sqlite3* db, const char* sql) {
    Statement query(db, sql);
    if (!query.row()) throw std::runtime_error("Missing database metadata");
    return query.number(0);
}
class Transaction {
public:
    explicit Transaction(sqlite3* db) : db_(db) { exec(db_, "BEGIN IMMEDIATE"); }
    ~Transaction() { if (!committed_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
    void commit() { exec(db_, "COMMIT"); committed_=true; }
private:
    sqlite3* db_; bool committed_=false;
};
}
void SqliteCloser::operator()(sqlite3* db) const { if (db) sqlite3_close(db); }
CorpusDb::CorpusDb(const std::filesystem::path& path) : db_(open(path, SQLITE_OPEN_READONLY)) {
    exec(db_.get(), "PRAGMA query_only=ON; PRAGMA foreign_keys=ON; PRAGMA cache_size=-8192");
    if (pragma_number(db_.get(), "PRAGMA user_version") != 3 ||
        pragma_number(db_.get(), "PRAGMA application_id") != 0x4f525443)
        throw std::runtime_error("Unsupported corpus schema");
}
bool CorpusDb::read_only() const { return sqlite3_db_readonly(db_.get(), "main") == 1; }
std::vector<WordExample> CorpusDb::word_examples(const std::string& word,int limit) const {
    Statement q(db_.get(),"SELECT book.code,chapter,verse,verse_suffix,text FROM verse JOIN book ON book.id=book_id JOIN source ON source.id=source_id WHERE source.code='sv1917' ORDER BY book.canonical_order,chapter,verse,verse_suffix");
    std::vector<WordExample> result;
    while(q.row()&&int(result.size())<limit)if(contains_speech_word(q.text(4),word)) {
        auto value=verse("sv1917",q.text(0),{q.number(1),q.number(2),q.text(3)});
        if(value)result.push_back({q.text(0),*value});
    }
    return result;
}
PronunciationReviewDb::PronunciationReviewDb(const std::filesystem::path& path) {
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    db_=open(path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
    const int version=pragma_number(db_.get(),"PRAGMA user_version"),identity=pragma_number(db_.get(),"PRAGMA application_id");
    if(version>1||(identity!=0&&identity!=0x4f525450)||(version==1&&identity!=0x4f525450))throw std::runtime_error("Unsupported pronunciation review database");
    exec(db_.get(),"PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA cache_size=-256; PRAGMA wal_autocheckpoint=64");
    if(!version) {
        Transaction transaction(db_.get());
        exec(db_.get(),"CREATE TABLE review(key TEXT PRIMARY KEY NOT NULL,form TEXT NOT NULL,status TEXT NOT NULL CHECK(status IN ('approved','corrected','deferred')),spoken TEXT NOT NULL,note TEXT NOT NULL,updated INTEGER NOT NULL,CHECK(status!='corrected' OR length(trim(spoken))>0)) STRICT; PRAGMA application_id=1330795600; PRAGMA user_version=1");
        transaction.commit();
    }
}
std::vector<PronunciationDecision> PronunciationReviewDb::decisions() const {
    Statement q(db_.get(),"SELECT form,status,spoken,note FROM review ORDER BY key");std::vector<PronunciationDecision> result;
    while(q.row())result.push_back({q.text(0),q.text(1),q.text(2),q.text(3)});
    return result;
}
void PronunciationReviewDb::save(const PronunciationDecision& value) {
    if(value.form.empty()||value.form.find_first_of("\t\r\n")!=std::string::npos)throw std::runtime_error("Invalid pronunciation word");
    Transaction transaction(db_.get());
    Statement q(db_.get(),"INSERT INTO review VALUES(?,?,?,?,?,unixepoch()) ON CONFLICT(key) DO UPDATE SET form=excluded.form,status=excluded.status,spoken=excluded.spoken,note=excluded.note,updated=excluded.updated");
    q.text(1,pronunciation_key(value.form));q.text(2,value.form);q.text(3,value.status);q.text(4,value.spoken);q.text(5,value.note);
    q.row();transaction.commit();
}
std::vector<Pronunciation> PronunciationReviewDb::overrides() const {
    std::vector<Pronunciation> result;
    for(const auto& value:decisions()) {
        if(value.status=="corrected")result.push_back({"sv",value.form,value.spoken,"",1000});
    }
    return result;
}
void PronunciationReviewDb::export_tsv(const std::filesystem::path& path) const {
    const auto clean=[](std::string text){for(char& c:text)if(c=='\t'||c=='\r'||c=='\n')c=' ';return text;};
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("Cannot open pronunciation export");
    out<<"# form\tstatus\tspoken\tnote\n";
    for(const auto& value:decisions())out<<clean(value.form)<<'\t'<<value.status<<'\t'<<clean(value.spoken)<<'\t'<<clean(value.note)<<'\n';
    out.close();if(!out)throw std::runtime_error("Cannot write pronunciation export");
}
std::vector<Source> CorpusDb::sources() const {
    Statement query(db_.get(), "SELECT id,code,language,name,versification FROM source ORDER BY id");
    std::vector<Source> result;
    while (query.row()) result.push_back({query.number(0), query.text(1), query.text(2), query.text(3), query.text(4)});
    return result;
}
std::expected<Verse, std::string> CorpusDb::verse(const std::string& source, const std::string& book, VerseRef ref) const {
    Statement query(db_.get(), "SELECT verse,last_verse,text,verse_suffix FROM verse JOIN source ON source.id=verse.source_id JOIN book ON book.id=verse.book_id WHERE source.code=? AND book.code=? AND chapter=? AND verse<=? AND last_verse>=? AND verse_suffix=? ORDER BY verse DESC LIMIT 1");
    query.text(1, source); query.text(2, book); query.number(3,ref.chapter); query.number(4,ref.verse); query.number(5,ref.verse);
    query.text(6,ref.suffix);
    if (!query.row()) return std::unexpected("Text saknas i denna utgåva");
    return Verse{{ref.chapter,query.number(0),query.text(3)}, query.text(2), query.number(0)==query.number(1)?std::nullopt:std::optional<VerseRef>{{ref.chapter,query.number(1)}}};
}
std::vector<VerseRef> CorpusDb::coordinates(const std::string& source, const std::string& book) const {
    Statement query(db_.get(), "SELECT chapter,verse,verse_suffix FROM verse JOIN source ON source.id=verse.source_id JOIN book ON book.id=verse.book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix");
    query.text(1, source); query.text(2, book);
    std::vector<VerseRef> result;
    while (query.row()) result.push_back({query.number(0), query.number(1),query.text(2)});
    return result;
}
std::vector<VerseRef> CorpusDb::paragraph_starts(const std::string& source, const std::string& book) const {
    Statement query(db_.get(), "SELECT chapter,verse,verse_suffix FROM paragraph JOIN source ON source.id=source_id JOIN book ON book.id=book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix");
    query.text(1,source);query.text(2,book);
    std::vector<VerseRef> result;
    while(query.row())result.push_back({query.number(0),query.number(1),query.text(2)});
    return result;
}
std::optional<Alignment> CorpusDb::alignment(const std::string& from, const std::string& to, const Passage& passage) const {
    Statement query(db_.get(), "SELECT kind,from_first_chapter,from_first_verse,from_first_suffix,from_last_chapter,from_last_verse,from_last_suffix,"
        "coalesce(target.code,''),coalesce(to_first_chapter,0),coalesce(to_first_verse,0),coalesce(to_first_suffix,''),coalesce(to_last_chapter,0),coalesce(to_last_verse,0),coalesce(to_last_suffix,'') "
        "FROM alignment JOIN source f ON f.id=from_source JOIN source t ON t.id=to_source JOIN book ON book.id=book_id LEFT JOIN book target ON target.id=to_book_id "
        "WHERE f.code=? AND t.code=? AND book.code=? AND (from_first_chapter,from_first_verse,from_first_suffix)<=(?,?,?) AND (from_last_chapter,from_last_verse,from_last_suffix)>=(?,?,?) "
        "ORDER BY from_first_chapter DESC,from_first_verse DESC,from_first_suffix DESC LIMIT 1");
    query.text(1,from); query.text(2,to); query.text(3,passage.book);
    query.number(4,passage.first.chapter); query.number(5,passage.first.verse); query.text(6,passage.first.suffix);
    query.number(7,passage.last.chapter); query.number(8,passage.last.verse); query.text(9,passage.last.suffix);
    if (!query.row()) return {};
    return Alignment{static_cast<AlignmentKind>(query.number(0)),
        {passage.book,{query.number(1),query.number(2),query.text(3)},{query.number(4),query.number(5),query.text(6)}},
        {query.text(7),{query.number(8),query.number(9),query.text(10)},{query.number(11),query.number(12),query.text(13)}}};
}
bool CorpusDb::same_numbering(const std::string& from, const std::string& to, const std::string& book) const {
    auto list = sources();
    const auto f = std::find_if(list.begin(),list.end(),[&](auto& s){return s.code==from;});
    const auto t = std::find_if(list.begin(),list.end(),[&](auto& s){return s.code==to;});
    return f!=list.end() && t!=list.end() && (f->versification==t->versification || new_testament_book(book));
}
std::vector<std::pair<std::string,VerseRef>> CorpusDb::counterparts(const std::string& from, const std::string& to, const std::string& book, VerseRef ref) const {
    const auto map = alignment(from, to, {book,ref,ref});
    if (!map) {
        // Same-coordinate lookup is only safe inside matching versification systems.
        if (!same_numbering(from,to,book)) return {};
        return {{book,ref}};
    }
    if (map->to.book.empty()) return {};
    std::vector<std::pair<std::string,VerseRef>> result;
    if (map->from.first==map->from.last || map->kind==AlignmentKind::Merged) {
        for (auto target : coordinates(to,map->to.book)) if (map->to.contains(target)) result.emplace_back(map->to.book,target);
        return result;
    }
    // A range with one numbering offset.
    return {{map->to.book,{ref.chapter+map->to.first.chapter-map->from.first.chapter,ref.verse+map->to.first.verse-map->from.first.verse}}};
}
std::expected<Verse, std::string> CorpusDb::parallel_verse(const std::string& from, const std::string& to, const std::string& book, VerseRef ref) const {
    if (from == to) return verse(to,book,ref);
    const auto map = alignment(from, to, {book,ref,ref});
    if (map && map->to.book.empty()) return std::unexpected(to=="grc-lxx" ? "Saknas i Septuaginta" : "Saknas i denna utgåva");
    if (map && map->kind==AlignmentKind::Merged && ref!=map->from.first) return std::unexpected("Ingår i föregående vers");
    if (!map && !same_numbering(from,to,book)) return std::unexpected("Ingen belagd textmappning");
    const auto targets = counterparts(from,to,book,ref);
    if (targets.empty()) return std::unexpected("Text saknas i denna utgåva");
    Verse result{targets.front().second, "", targets.size()>1 ? std::optional<VerseRef>{targets.back().second} : std::nullopt};
    for (const auto& [target_book,target] : targets) {
        auto part = verse(to,target_book,target);
        if (!part) return std::unexpected(part.error());
        if (!result.text.empty()) result.text += " ";
        result.text += part->text;
        if (targets.size()==1) result.last = part->last;
    }
    return result;
}
std::vector<Passage> CorpusDb::map_passage(const std::string& from, const std::string& to, const Passage& passage) const {
    if (from == to) return {passage};
    std::vector<std::pair<std::string,VerseRef>> found;
    for (auto ref : coordinates(from,passage.book)) if (passage.contains(ref))
        for (const auto& target : counterparts(from,to,passage.book,ref))
            if (std::find(found.begin(),found.end(),target)==found.end()) found.push_back(target);
    // Verses that the target only reorders within one run, as Philippians 1:16-17 in
    // the 1917 Bible, read as one range in the target's own order.
    if (!found.empty() && std::all_of(found.begin(),found.end(),[&](const auto& t){return t.first==found.front().first;})) {
        const auto refs = coordinates(to,found.front().first);
        const auto index = [&](VerseRef ref){return std::find(refs.begin(),refs.end(),ref)-refs.begin();};
        std::vector<std::ptrdiff_t> at;
        for (const auto& target : found) at.push_back(index(target.second));
        if (!std::is_sorted(at.begin(),at.end()) && std::ranges::none_of(at,[&](auto i){return i==std::ptrdiff_t(refs.size());})) {
            const auto [low,high] = std::minmax_element(at.begin(),at.end());
            bool run = true;
            for (auto i=*low; i<=*high && run; ++i)
                run = std::find(at.begin(),at.end(),i)!=at.end() || counterparts(to,from,found.front().first,refs[std::size_t(i)]).empty();
            if (run) std::ranges::sort(found,{},[&](const auto& target){return index(target.second);});
        }
    }
    // Join targets that are adjacent in the target edition, including verses between
    // them that only the target tradition has, such as lettered Septuagint additions.
    std::vector<Passage> result;
    std::map<std::string,std::vector<VerseRef>> editions;
    for (const auto& [book,ref] : found) {
        const auto& refs = editions.try_emplace(book,coordinates(to,book)).first->second;
        const auto at = std::find(refs.begin(),refs.end(),ref);
        if (at==refs.end()) continue;
        if (!result.empty() && result.back().book==book) {
            const auto previous = std::find(refs.begin(),at,result.back().last);
            if (previous!=at && std::all_of(previous+1,at,[&](VerseRef between){return counterparts(to,from,book,between).empty();})) {
                result.back().last=ref;
                continue;
            }
        }
        result.push_back({book,ref,ref});
    }
    return result;
}
Reading CorpusDb::localize(Reading reading) const {
    if (!reading.source_override.empty()) return reading;
    std::vector<Passage> parts;
    for (const auto& segment : reading.segments()) {
        const auto mapped = map_passage(reading.reference,frame_source(reading.base_language,segment.book),segment);
        // An edition without the book keeps the reference coordinates; the reader falls back to another edition.
        const auto chosen = mapped.empty() ? std::vector<Passage>{segment} : mapped;
        parts.insert(parts.end(), chosen.begin(), chosen.end());
    }
    reading.passage = parts.front();
    reading.additional.assign(parts.begin()+1,parts.end());
    return reading;
}
StudyDb::StudyDb(const std::filesystem::path& path) : db_(open(path, SQLITE_OPEN_READONLY)) {
    exec(db_.get(), "PRAGMA query_only=ON");
    if (pragma_number(db_.get(), "PRAGMA user_version") != 2 ||
        pragma_number(db_.get(), "PRAGMA application_id") != 0x4f525354)
        throw std::runtime_error("Unsupported word-study schema");
}
std::vector<DalinEntry> StudyDb::swedish(const std::string& form) const {
    Statement query(db_.get(), "SELECT headword,gram,definition FROM sv_word JOIN dalin ON dalin.id=sv_word.dalin WHERE form=? ORDER BY dalin.id");
    query.text(1,form); std::vector<DalinEntry> result;
    while (query.row()) result.push_back({query.text(0),query.text(1),query.text(2)});
    return result;
}
std::optional<std::pair<VerseRef,int>> StudyDb::greek_link(const std::string& book, VerseRef ref, const std::string& form, int occurrence) const {
    Statement query(db_.get(), "SELECT greek_chapter,greek_verse,greek_suffix,position FROM sv_greek WHERE book=? AND chapter=? AND verse=? AND form=? AND occurrence=?");
    query.text(1,book); query.number(2,ref.chapter); query.number(3,ref.verse); query.text(4,form); query.number(5,occurrence);
    if (!query.row()) return std::nullopt;
    return std::pair{VerseRef{query.number(0),query.number(1),query.text(2)},query.number(3)};
}
std::optional<StrongsEntry> StudyDb::strongs(const std::string& strong) const {
    Statement query(db_.get(), "SELECT strong,lemma,transliteration,gloss,definition FROM strongs WHERE strong=?");
    query.text(1,strong);
    if (!query.row()) return std::nullopt;
    return StrongsEntry{query.text(0),query.text(1),query.text(2),query.text(3),query.text(4)};
}
std::vector<GreekWord> StudyDb::greek_words(const std::string& book, VerseRef ref) const {
    Statement query(db_.get(), "SELECT surface,strong FROM greek_word WHERE book=? AND chapter=? AND verse=? AND suffix=? ORDER BY position");
    query.text(1,book); query.number(2,ref.chapter); query.number(3,ref.verse); query.text(4,ref.suffix);
    std::vector<GreekWord> result;
    while (query.row()) result.push_back({query.text(0),query.text(1)});
    return result;
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
    Statement q(db_.get(),"SELECT reading_rule.id,pdist,month,day,ordering,service,description,tradition,label,source.code FROM reading_rule JOIN source ON source.id=reference ORDER BY ordering,reading_rule.id");
    std::vector<ReadingRule> result;
    while(q.row()) {
        const auto service=q.text(5);
        const auto kind=service=="Epistle"?ReadingKind::Epistle:service=="Gospel"?ReadingKind::Gospel:service=="Vespers"?ReadingKind::Vespers:ReadingKind::OldTestament;
        ReadingRule rule{q.number(0),q.number(1),q.number(2),q.number(3),q.number(4),service,q.text(6),q.text(7),{kind,{},q.text(8),{}}};
        rule.reading.reference=q.text(9);
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
    const int version=pragma_number(db_.get(), "PRAGMA user_version");
    const int identity=pragma_number(db_.get(), "PRAGMA application_id");
    if ((version!=0&&version!=1) || (identity!=0&&identity!=0x4f525455) || (version==1&&identity!=0x4f525455)) throw std::runtime_error("Unsupported settings database");
    exec(db_.get(), "PRAGMA foreign_keys=ON; PRAGMA cache_size=-256");
    { Statement journal(db_.get(), "PRAGMA journal_mode=WAL");
      if (!journal.row() || journal.text(0)!="wal") throw std::runtime_error("Cannot enable settings write-ahead log"); }
    exec(db_.get(), "PRAGMA synchronous=FULL; PRAGMA wal_autocheckpoint=64; PRAGMA journal_size_limit=262144");
#ifdef __APPLE__
    exec(db_.get(), "PRAGMA fullfsync=ON; PRAGMA checkpoint_fullfsync=ON");
#endif
    // Version zero is the original settings table. Preserve every stored value.
    Transaction migration(db_.get());
    if (pragma_number(db_.get(), "PRAGMA user_version")==0) {
        exec(db_.get(), "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
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
        const auto key=query.text(0), value=query.text(1);
        if (key=="theme") result.theme=value=="dark"?Theme::Dark:value=="light"?Theme::Light:Theme::System;
        if (key=="calendar") result.calendar=value=="old"?CalendarStyle::Old:CalendarStyle::New;
        if (key=="primary" && (value=="sv" || value=="el" || value=="en")) result.primary=value;
        // The three-pane mode "el,en" is retired; it keeps its Greek pane.
        if (key=="parallel" && (value.empty() || value=="sv" || value=="el" || value=="en" || value=="el,en")) result.parallel=value=="el,en"?"el":value;
        if (key=="font_size") { try { result.font_size=std::clamp(std::stoi(value),14,28); } catch (...) {} }
        if (key=="speech_rate") { try { result.speech_rate=std::clamp(std::stoi(value),25,200); } catch (...) {} }
        if (key=="speech_voice" && (value=="alice" || value=="bjorn")) result.speech_voice=value;
        if (key=="word_study") result.word_study=value=="1";
    }
    if (result.parallel==result.primary) result.parallel.clear();
    if (result.word_study) result.parallel.clear();
    return result;
}
void UserDb::save(const Settings& settings) {
    Transaction transaction(db_.get());
        const std::vector<std::pair<std::string,std::string>> values={
            {"theme",settings.theme==Theme::Dark?"dark":settings.theme==Theme::Light?"light":"system"},
            {"calendar",settings.calendar==CalendarStyle::Old?"old":"new"},
            {"primary",settings.primary},{"parallel",settings.parallel},{"font_size",std::to_string(settings.font_size)},
            {"speech_rate",std::to_string(settings.speech_rate)},{"speech_voice",settings.speech_voice},
            {"word_study",settings.word_study?"1":"0"}};
        Statement query(db_.get(), "INSERT INTO settings VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
        for (const auto& [key,value]:values) {
            query.text(1,key); query.text(2,value); query.row();
            query.reset();
        }
    transaction.commit();
}
SpeechCache::SpeechCache(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    db_=open(path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
    exec(db_.get(),"PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON; PRAGMA cache_size=-2048; PRAGMA wal_autocheckpoint=256");
    const int version=pragma_number(db_.get(),"PRAGMA user_version");
    const int identity=pragma_number(db_.get(),"PRAGMA application_id");
    if(version>1||(identity!=0&&identity!=0x4f525453))throw std::runtime_error("Unsupported speech cache");
    if(!version) {
        Transaction transaction(db_.get());
        exec(db_.get(),"CREATE TABLE audio(model TEXT NOT NULL,language TEXT NOT NULL,text TEXT NOT NULL,pcm_f32le BLOB NOT NULL CHECK(length(pcm_f32le)>0 AND length(pcm_f32le)%4=0),used INTEGER NOT NULL,PRIMARY KEY(model,language,text)) STRICT; PRAGMA application_id=1330795603; PRAGMA user_version=1");
        transaction.commit();
    } else if(identity!=0x4f525453)throw std::runtime_error("Unsupported speech cache identity");
}
std::optional<std::vector<float>> SpeechCache::load(const std::string& model,const std::string& language,const std::string& text) {
    Statement query(db_.get(),"SELECT pcm_f32le FROM audio WHERE model=? AND language=? AND text=?");
    query.text(1,model);query.text(2,language);query.text(3,text);
    if(!query.row())return {};
    const auto bytes=query.blob(0);
    if(bytes.empty()||bytes.size()%4||bytes.size()>16*1024*1024)return {};
    std::vector<float> samples; samples.reserve(bytes.size()/4);
    for(size_t i=0;i<bytes.size();i+=4) {
        const uint32_t bits=uint32_t(bytes[i])|(uint32_t(bytes[i+1])<<8)|(uint32_t(bytes[i+2])<<16)|(uint32_t(bytes[i+3])<<24);
        const float sample=std::bit_cast<float>(bits);
        if(!std::isfinite(sample))return {};
        samples.push_back(sample);
    }
    Statement touch(db_.get(),"UPDATE audio SET used=unixepoch() WHERE model=? AND language=? AND text=?");
    touch.text(1,model);touch.text(2,language);touch.text(3,text);touch.row();
    return samples;
}
void SpeechCache::save(const std::string& model,const std::string& language,const std::string& text,const std::vector<float>& samples) {
    if(samples.empty()||samples.size()>4*1024*1024)throw std::runtime_error("Invalid speech audio length");
    std::vector<unsigned char> bytes;bytes.reserve(samples.size()*4);
    for(const float sample:samples) {
        if(!std::isfinite(sample))throw std::runtime_error("Invalid speech audio sample");
        const auto bits=std::bit_cast<uint32_t>(sample);
        for(int shift=0;shift<32;shift+=8)bytes.push_back(static_cast<unsigned char>(bits>>shift));
    }
    Transaction transaction(db_.get());
    Statement insert(db_.get(),"INSERT INTO audio VALUES(?,?,?,?,unixepoch()) ON CONFLICT(model,language,text) DO UPDATE SET pcm_f32le=excluded.pcm_f32le,used=excluded.used");
    insert.text(1,model);insert.text(2,language);insert.text(3,text);insert.blob(4,bytes);insert.row();
    // At most 256 MiB of audio. SQLite reuses evicted pages for future entries.
    while(pragma_number(db_.get(),"SELECT coalesce(sum(length(pcm_f32le)),0) FROM audio")>256*1024*1024)
        exec(db_.get(),"DELETE FROM audio WHERE (model,language,text)=(SELECT model,language,text FROM audio ORDER BY used,model,language,text LIMIT 1)");
    transaction.commit();
}
}
