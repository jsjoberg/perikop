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
// Dates exposed by the application calendar. The engine retains earlier arithmetic for cycle tests.
inline constexpr int first_calendar_year = 2019;
constexpr bool selectable_date(CivilDate date) {
    return date.ok() && int(date.year()) >= first_calendar_year && int(date.year()) <= 9999;
}
enum class CalendarStyle { New, Old };
enum class Theme { System, Light, Dark };
// Colour schemes; each has a light and a dark variant. Theme picks between them.
enum class ColorScheme { RosePine, Gruvbox, Nord, Everforest, Catppuccin, TokyoNight, Solarized };
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
// A resumable verse in the reading's framing edition, never a position inside a sentence.
struct ReadingPosition {
    std::size_t section = 0;
    std::string book, source;
    VerseRef verse;
    int percent = 0;
    bool operator==(const ReadingPosition&) const = default;
};
struct Bookmark {
    int id;
    std::string source;
    Passage passage;
};
struct Reading {
    ReadingKind kind;
    Passage passage;
    std::vector<Passage> additional;
    std::string base_language = "sv";
    std::string source_override;
    // The edition whose numbering the passages use; lectionary rules state theirs.
    // Empty means each passage already uses the framing edition for its book and language.
    std::string reference;
    // Liturgical service and occasion. Empty for manually selected Scripture.
    std::string service, occasion;
    // Official source of this dated assignment, or of a composite reading's verse list;
    // separate from the Bible text edition.
    std::string assignment_source_url;
    // Composite readings keep their published short citation, never adapted liturgical wording;
    // one without a cited verse list cannot open.
    std::string citation;
    bool can_open() const {
        return !passage.book.empty();
    }
    Reading(ReadingKind k, Passage p, std::vector<Passage> rest = {}, std::string language = "sv")
        : kind(k), passage(std::move(p)), additional(std::move(rest)), base_language(std::move(language)) {}
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
    // Zero is recurring; a civil year scopes a published New Calendar chart row.
    int year = 0;
};
struct FeastRule {
    int pdist, month, day, rank;
    std::string tradition;
    std::string cycle_title;
};
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
    // The day and week in the reading cycle, without fixed feast headings.
    std::string cycle_title;
    // Whether a published annual assignment replaced a calculated reading.
    bool annual = false;
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
    std::string book = "";
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
enum class UpdateChannel { Releases, Nightly };
struct SourcePreference {
    std::string source;
    bool enabled = true;
    bool operator==(const SourcePreference&) const = default;
};
struct Settings {
    static constexpr int text_zoom_steps[] = {75, 80, 90, 100, 110, 125, 150, 175, 200};
    // eSpeak speaks 80–450 words per minute, around its normal 150.
    static constexpr int speech_rate_steps[] = {60, 75, 100, 125, 150, 175, 200, 250, 300};
    CalendarStyle calendar = CalendarStyle::New;
    Tradition tradition = Tradition::Greek;
    Theme theme = Theme::System;
    ColorScheme color_scheme = ColorScheme::RosePine;
    // Text size in percent, for every text in the program.
    int text_zoom = 100;
    bool split_view = false;
    // First enabled edition containing the chapter wins. Greek is opt-in for the main text.
    std::vector<SourcePreference> primary_sources{{"sv1917", true},
                                                  {"en-lxx2012", true},
                                                  {"en-web", true},
                                                  {"grc-lxx", false},
                                                  {"grc-patriarchal", false}};
    std::vector<SourcePreference> secondary_sources{{"en-lxx2012", true},
                                                    {"en-web", true},
                                                    {"grc-lxx", true},
                                                    {"grc-patriarchal", true},
                                                    {"sv1917", false}};
    // The right pane is the Ordstudium lookup panel. Excludes split view.
    bool word_study = false;
    // Read-aloud tints the line being read. The margin marker shows either way.
    bool speech_highlight = false;
    // Read-aloud speed, in percent of the voice's own pace (eSpeak: 150 words per minute).
    int speech_rate = 100;
    // Swedish voice: legacy "espeak" / "espeak-f2" or an installed "espeak:<selector>", from the
    // user's own eSpeak NG. The selected voice survives application restarts.
    std::string speech_voice = "espeak";
    // Selects the source for a user-initiated update check; never enables background networking.
    UpdateChannel update_channel = UpdateChannel::Releases;
};
// The language of an edition, such as "el" for grc-lxx: "sv", "el" or "en".
std::string source_language(const std::string& source);
// A book or named section in the OSB reference authority.
struct CanonBook {
    std::string code; // name, picker identity and OSB book code, e.g. "Neh"
    // Swedish names as the reader shows the book, e.g. "Ester enligt den grekiska texten".
    std::string name, abbreviation;
    int last_chapter = 999;
    bool new_testament = false;
};
} // namespace ortho
