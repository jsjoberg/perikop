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
// The month and year, such as "oktober 2026".
std::string month_swedish(CivilDate date);
std::expected<CivilDate, std::string> parse_date(const std::string& value);
enum class CalendarStyle { New, Old };
enum class Theme { System, Light, Dark };
// Internal source orders. The interface groups Antiochian under Greek.
enum class Tradition { Antiochian, Greek, Slavic };
enum class ReadingKind {
    MorningPsalm,
    Epistle,
    Gospel,
    OldTestament,
    Vespers,
    EveningPsalm,
    Matins,
    Hours,
    OtherService
};
struct VerseRef {
    int chapter = 1;
    int verse = 1;
    std::string suffix = "";
    auto operator<=>(const VerseRef&) const = default;
};
struct Passage {
    std::string book;
    VerseRef first, last;
    bool contains(VerseRef ref) const {
        return first <= ref && ref <= last;
    }
};
std::expected<Passage, std::string> normalize_passage(Passage passage);
struct Reading {
    ReadingKind kind;
    Passage passage;
    std::vector<Passage> additional;
    std::string base_language = "sv";
    std::string source_override;
    // The edition whose numbering the passages use; lectionary rules state theirs.
    // Empty means each passage already uses the framing edition for its book and language.
    std::string reference = "en-kjv";
    // Liturgical service and occasion. Empty for manually selected Scripture.
    std::string service, occasion;
    // Composite readings retain their published citation without importing adapted liturgical wording.
    std::string citation;
    bool can_open() const {
        return !passage.book.empty();
    }
    Reading(ReadingKind k, Passage p, std::vector<Passage> rest = {}, std::string language = "sv")
        : kind(k), passage(std::move(p)), additional(std::move(rest)), base_language(std::move(language)) {}
    bool contains(VerseRef ref, const std::string& book = "") const {
        const auto& selected = book.empty() ? passage.book : book;
        if (passage.book == selected && passage.contains(ref))
            return true;
        for (const auto& p : additional)
            if (p.book == selected && p.contains(ref))
                return true;
        return false;
    }
    std::vector<Passage> segments() const {
        auto parts = additional;
        parts.insert(parts.begin(), passage);
        return parts;
    }
};
struct ReadingRule {
    int id, pdist, month, day, ordering;
    std::string service, description, tradition;
    Reading reading;
};
struct FeastRule {
    int pdist, month, day, rank;
    std::string title, feast, tradition;
    int id = 0, fast = -1, fast_exception = -1, fast_cap_exempt = -1;
};
struct CommemorationRule {
    int day_id, ordering;
    std::string title, tradition;
    bool new_style, day_native;
};
enum class FastPeriod { None, Day, Lent, Apostles, Dormition, Nativity };
enum class DietaryAllowance { Strict, Wine, WineOil, WineOilCaviar, FishWineOil, MeatFast, Free };
struct Fasting {
    FastPeriod period = FastPeriod::None;
    DietaryAllowance allowance = DietaryAllowance::Strict;
    auto operator<=>(const Fasting&) const = default;
};
std::string fasting_period_label(FastPeriod);
std::string fasting_allowance_label(Fasting);
std::string fasting_abstentions(Fasting);
std::string service_label(const std::string&);
// A published annual assignment of one jurisdiction: "greek" (GOA) or "antiochian".
struct OrdoRule {
    int year, month, day, pdist;
    std::string service, jurisdiction;
};
struct LiturgicalDay {
    CivilDate civil_date;
    CalendarStyle calendar;
    // Independent fixed-calendar label and distance from Orthodox Pascha.
    // Fixed labels can include leap days that are invalid in the Gregorian calendar.
    std::optional<std::string> fixed_cycle;
    std::optional<std::string> paschal_cycle;
    // Swedish names of the day and its feasts, joined by " · ".
    std::string title;
    // Whether a published annual assignment replaced a calculated reading.
    bool annual = false;
    Fasting fasting;
    std::vector<std::string> commemorations;
};
struct DayReadings {
    LiturgicalDay day;
    std::vector<Reading> readings;
    struct Variant {
        // Missing index means an additional reading; missing reading means an omission.
        std::optional<std::size_t> primary_index;
        std::optional<Reading> reading;
        std::string jurisdiction, explanation;
    };
    std::vector<Variant> variants = {};
};
class CorpusDb;
CivilDate orthodox_pascha(int year);
CivilDate fixed_calendar_date(CivilDate, CalendarStyle);
// Swedish form of an English day or feast title from the calendar tables.
std::optional<std::string> swedish_title(const std::string&);
class Lectionary {
public:
    explicit Lectionary(const CorpusDb&);
    DayReadings readings_for(CivilDate, CalendarStyle, Tradition) const;
    // Every applicable service reading, including saints' readings beside the daily cycle.
    DayReadings service_readings_for(CivilDate, CalendarStyle, Tradition) const;
    // Greek includes differing Antiochian readings. Slavic currently has one verified source order.
    DayReadings readings_with_variants(CivilDate, CalendarStyle, Tradition) const;

private:
    DayReadings calculate(CivilDate, CalendarStyle, Tradition, bool full) const;
    std::vector<ReadingRule> rules_;
    std::vector<FeastRule> feasts_;
    std::vector<OrdoRule> ordos_;
    std::vector<CommemorationRule> commemorations_;
};
class SelectedDay {
public:
    explicit SelectedDay(CivilDate initial) {
        select(initial);
    }
    CivilDate date() const {
        return date_;
    }
    void select(CivilDate date);
    void move(int days) {
        select(shift_date(date_, days));
    }

private:
    CivilDate date_;
};
struct Source {
    int id;
    std::string code, language, name, versification;
};
struct Verse {
    VerseRef ref;
    std::string text;
    std::optional<VerseRef> last;
};
enum class AlignmentKind { Same, Renumbered, Split, Merged, Moved, LxxOnly, MtOnly };
struct Alignment {
    AlignmentKind kind;
    Passage from, to;
};
struct Pronunciation {
    std::string language, source, spoken, phonemes;
    int priority;
};
struct Settings {
    CalendarStyle calendar = CalendarStyle::New;
    Tradition tradition = Tradition::Greek;
    Theme theme = Theme::System;
    int font_size = 19;
    // Left pane language: "sv", "el" or "en". It is also the language read aloud.
    std::string primary = "sv";
    // Right pane language, or empty for one pane. Never the same as primary.
    std::string parallel;
    // The right pane is the Ordstudium lookup panel. Excludes a parallel language.
    bool word_study = false;
    // Read-aloud tints the line being read. The margin marker shows either way.
    bool speech_highlight = false;
    // Playback speed of read-aloud audio, in percent of the voice's own pace.
    int speech_rate = 100;
    // Swedish voice identity. The selected voice survives application restarts.
    std::string speech_voice = "alice";
};
// The language of an edition, such as "el" for grc-lxx: "sv", "el" or "en".
std::string source_language(const std::string& source);
// A book as the reader presents it. Old Testament books are numbered by the
// Septuagint (grc-lxx); some occupy only a chapter range of a Greek book.
struct CanonBook {
    std::string code;       // name and picker identity, e.g. "Neh"
    std::string frame_book; // the book in the framing edition, e.g. "Ezra"
    int first_chapter = 1, last_chapter = 999;
    bool new_testament = false;
    int offset() const {
        return first_chapter - 1;
    }
};
} // namespace ortho
