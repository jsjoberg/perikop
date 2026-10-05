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
    auto operator<=>(const VerseRef&) const = default;
};
struct Passage {
    std::string book;
    VerseRef first, last;
    bool contains(VerseRef ref) const { return first <= ref && ref <= last; }
};
std::expected<Passage, std::string> normalize_passage(Passage passage);
struct Reading { ReadingKind kind; Passage passage; std::string label; };
struct LiturgicalDay {
    CivilDate civil_date;
    CalendarStyle calendar;
    // Fixed and movable cycles deliberately unresolved; no fabricated offset/rules.
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
class FixtureLectionary final : public Lectionary {
public:
    DayReadings readings_for(CivilDate, CalendarStyle) const override;
};
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
    std::string parallel = "el";
};
std::string source_for_language(const std::string& language, const std::string& book);
}
