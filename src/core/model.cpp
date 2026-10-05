#include "core/model.hpp"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ortho {
CivilDate local_civil_date() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return std::chrono::year{local.tm_year + 1900}/(local.tm_mon + 1)/local.tm_mday;
}
CivilDate shift_date(CivilDate date, int days) {
    if (!date.ok()) throw std::invalid_argument("Invalid civil date");
    return CivilDate{std::chrono::sys_days{date} + std::chrono::days{days}};
}
std::string date_iso(CivilDate date) {
    std::ostringstream out;
    out << int(date.year()) << '-' << std::setfill('0') << std::setw(2)
        << unsigned(date.month()) << '-' << std::setw(2) << unsigned(date.day());
    return out.str();
}
std::expected<CivilDate, std::string> parse_date(const std::string& value) {
    int year, month, day; char tail;
    if (std::sscanf(value.c_str(), "%d-%d-%d%c", &year, &month, &day, &tail) != 3)
        return std::unexpected("Date must be YYYY-MM-DD");
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31)
        return std::unexpected("Invalid civil date");
    CivilDate date{std::chrono::year{year}, std::chrono::month{unsigned(month)}, std::chrono::day{unsigned(day)}};
    if (!date.ok()) return std::unexpected("Invalid civil date");
    return date;
}
std::string date_swedish(CivilDate date) {
    static const char* weekdays[] = {"Söndag", "Måndag", "Tisdag", "Onsdag", "Torsdag", "Fredag", "Lördag"};
    static const char* months[] = {"januari", "februari", "mars", "april", "maj", "juni", "juli", "augusti", "september", "oktober", "november", "december"};
    return std::string(weekdays[std::chrono::weekday{std::chrono::sys_days{date}}.c_encoding()]) + " " +
        std::to_string(unsigned(date.day())) + " " + months[unsigned(date.month())-1] + " " + std::to_string(int(date.year()));
}
std::expected<Passage, std::string> normalize_passage(Passage passage) {
    if (passage.book.empty() || passage.first.chapter < 1 || passage.first.verse < 1 ||
        passage.last.chapter < 1 || passage.last.verse < 1) return std::unexpected("Invalid passage coordinates");
    if (passage.last < passage.first) std::swap(passage.first, passage.last);
    return passage;
}
void SelectedDay::select(CivilDate date) {
    if (!date.ok() || int(date.year()) < 1 || int(date.year()) > 9999) throw std::invalid_argument("Invalid selected date");
    date_ = date;
}
DayReadings FixtureLectionary::readings_for(CivilDate date, CalendarStyle style) const {
    DayReadings result{{date, style, {}, {}, "Provdata · ingen fastställd kyrkokalender"}, {}};
    const auto iso = date_iso(date);
    if (iso == "2026-10-05") {
        result.readings = {{ReadingKind::MorningPsalm, {"Ps", {23,1}, {23,6}}, "Psalm 23"},
            {ReadingKind::Epistle, {"Phil", {2,1}, {2,11}}, "Filipperbrevet 2:1–11"},
            {ReadingKind::Gospel, {"Luke", {6,27}, {6,36}}, "Lukasevangeliet 6:27–36"}};
    } else if (iso == "2026-10-06") {
        // A distinct old-calendar fixture, not a calendar calculation.
        result.readings = {{ReadingKind::MorningPsalm, {"Ps", {24,1}, {24,10}}, "Psalm 24"},
            {ReadingKind::Gospel, {"Luke", {6,37}, {6,42}}, "Lukasevangeliet 6:37–42"}};
        if (style == CalendarStyle::Old) result.readings[0] =
            {ReadingKind::MorningPsalm, {"Ps", {23,1}, {23,6}}, "Psalm 23 · gammal kalender, provdata"};
    } else if (iso == "2026-10-07") {
        result.readings = {{ReadingKind::Epistle, {"Phil", {2,12}, {2,18}}, "Filipperbrevet 2:12–18"},
            {ReadingKind::Gospel, {"Luke", {6,43}, {6,49}}, "Lukasevangeliet 6:43–49"}};
    }
    return result;
}
std::string source_for_language(const std::string& language, const std::string& book) {
    if (language == "sv") return "sv1917";
    if (language == "en") return "en-kjv";
    if (language == "el") return book == "Ps" ? "grc-ot-fixture" : "grc-nt-fixture";
    return {};
}
}
