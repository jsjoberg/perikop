#pragma once
#include <chrono>
#include <compare>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace ortho {
using CivilDate = std::chrono::year_month_day;
CivilDate local_civil_date();
CivilDate shift_date(CivilDate date, int days);
std::string date_iso(CivilDate date);
std::string date_swedish(CivilDate date);
std::expected<CivilDate, std::string> parse_date(const std::string& value);
enum class CalendarStyle { New, Old };
enum class Theme { System, Light, Dark };
enum class ReadingKind { MorningPsalm, Epistle, Gospel, OldTestament, Vespers, EveningPsalm };
struct VerseRef {
    int chapter = 1;
    int verse = 1;
    std::string suffix="";
    auto operator<=>(const VerseRef&) const = default;
};
struct Passage {
    std::string book;
    VerseRef first, last;
    bool contains(VerseRef ref) const { return first <= ref && ref <= last; }
};
std::expected<Passage, std::string> normalize_passage(Passage passage);
struct Reading {
    ReadingKind kind; Passage passage; std::string label;
    std::vector<Passage> additional;
    std::string base_language="sv";
    std::string source_override;
    // The edition whose numbering the passages use; lectionary rules state theirs.
    std::string reference="en-kjv";
    Reading(ReadingKind k,Passage p,std::string name,std::vector<Passage> rest={},std::string language="sv")
        :kind(k),passage(std::move(p)),label(std::move(name)),additional(std::move(rest)),base_language(std::move(language)) {}
    bool contains(VerseRef ref,const std::string& book="") const {
        const auto& selected=book.empty()?passage.book:book;
        if(passage.book==selected&&passage.contains(ref))return true;
        for(const auto& p:additional)if(p.book==selected&&p.contains(ref))return true;
        return false;
    }
    std::vector<Passage> segments() const { auto parts=additional;parts.insert(parts.begin(),passage);return parts; }
};
struct ReadingRule { int id,pdist,month,day,ordering; std::string service,description,tradition; Reading reading; };
struct FeastRule { int pdist,month,day,rank; std::string title,feast,tradition; };
struct OrdoRule { int year,month,day,pdist; std::string service; };
struct Book { std::string code,name; int order; };
struct LiturgicalDay {
    CivilDate civil_date;
    CalendarStyle calendar;
    // Independent fixed-calendar label and distance from Orthodox Pascha.
    // Fixed labels can include leap days that are invalid in the Gregorian calendar.
    std::optional<std::string> fixed_cycle;
    std::optional<std::string> paschal_cycle;
    std::string annotation;
};
struct DayReadings { LiturgicalDay day; std::vector<Reading> readings; };
class Lectionary {
public:
    virtual ~Lectionary() = default;
    virtual DayReadings readings_for(CivilDate, CalendarStyle) const = 0;
};
class CorpusDb;
CivilDate orthodox_pascha(int year);
CivilDate fixed_calendar_date(CivilDate,CalendarStyle);
// Swedish form of an English day or feast title from the calendar tables.
std::optional<std::string> swedish_title(const std::string&);
class AntiochianLectionary final : public Lectionary {
public:
    explicit AntiochianLectionary(const CorpusDb&);
    DayReadings readings_for(CivilDate,CalendarStyle) const override;
private:
    std::vector<ReadingRule> rules_;
    std::vector<FeastRule> feasts_;
    std::vector<OrdoRule> ordos_;
};
bool new_testament_book(const std::string&);
class SelectedDay {
public:
    explicit SelectedDay(CivilDate initial) { select(initial); }
    CivilDate date() const { return date_; }
    void select(CivilDate date);
    void move(int days) { select(shift_date(date_, days)); }
private:
    CivilDate date_;
};
struct Source { int id; std::string code, language, name, versification; };
struct Verse { VerseRef ref; std::string text; std::optional<VerseRef> last; };
enum class AlignmentKind { Same, Renumbered, Split, Merged, Moved, LxxOnly, MtOnly };
struct Alignment { AlignmentKind kind; Passage from, to; };
struct Pronunciation { std::string language, source, spoken, phonemes; int priority; };
struct Settings {
    CalendarStyle calendar = CalendarStyle::New;
    Theme theme = Theme::System;
    int font_size = 19;
    // Left pane language: "sv", "el" or "en". It is also the language read aloud.
    std::string primary = "sv";
    // Right pane language, or empty for one pane. Never the same as primary.
    std::string parallel;
    // The right pane is the Ordstudium lookup panel. Excludes a parallel language.
    bool word_study = false;
    // Playback speed of read-aloud audio, in percent of the voice's own pace.
    int speech_rate = 100;
    // Swedish voice identity. The selected voice survives application restarts.
    std::string speech_voice = "alice";
};
std::string source_for_language(const std::string& language, const std::string& book);
bool deuterocanonical_book(const std::string& book);
// A book as the reader presents it. Old Testament books are numbered by the
// Septuagint (grc-lxx); some occupy only a chapter range of a Greek book.
struct CanonBook {
    std::string code;        // name and picker identity, e.g. "Neh"
    std::string frame_book;  // the book in the framing edition, e.g. "Ezra"
    int first_chapter=1,last_chapter=999;
    bool new_testament=false;
    int offset() const {return first_chapter-1;}
};
// Books in the order of the Orthodox Study Bible.
const std::vector<CanonBook>& osb_canon();
const CanonBook* canon_book(const std::string& frame_book,int chapter);
// The edition whose numbering frames a book: the Septuagint for the Old
// Testament, so every pane shows Old Testament text in LXX order and numbers.
std::string frame_source(const std::string& language,const std::string& book);
}
