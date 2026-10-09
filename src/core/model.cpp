#include "core/model.hpp"
#include <algorithm>
#include <charconv>
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
    return std::chrono::year{local.tm_year + 1900} / (local.tm_mon + 1) / local.tm_mday;
}
CivilDate shift_date(CivilDate date, int days) {
    if (!date.ok())
        throw std::invalid_argument("Invalid civil date");
    return CivilDate{std::chrono::sys_days{date} + std::chrono::days{days}};
}
std::string date_iso(CivilDate date) {
    std::ostringstream out;
    out << int(date.year()) << '-' << std::setfill('0') << std::setw(2) << unsigned(date.month()) << '-'
        << std::setw(2) << unsigned(date.day());
    return out.str();
}
std::expected<CivilDate, std::string> parse_date(const std::string& value) {
    int year = 0, month = 0, day = 0;
    const char* position = value.data();
    const char* const end = value.data() + value.size();
    const auto field = [&](int& number, bool last) {
        const auto [next, error] = std::from_chars(position, end, number);
        position = next;
        if (error != std::errc{} || (last ? position != end : position == end || *position != '-'))
            return false;
        position += last ? 0 : 1;
        return true;
    };
    if (!field(year, false) || !field(month, false) || !field(day, true))
        return std::unexpected("Date must be YYYY-MM-DD");
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31)
        return std::unexpected("Invalid civil date");
    CivilDate date{std::chrono::year{year}, std::chrono::month{unsigned(month)},
                   std::chrono::day{unsigned(day)}};
    if (!date.ok())
        return std::unexpected("Invalid civil date");
    return date;
}
std::string date_swedish(CivilDate date) {
    static const char* weekdays[] = {"Söndag", "Måndag", "Tisdag", "Onsdag", "Torsdag", "Fredag", "Lördag"};
    static const char* months[] = {"januari", "februari", "mars",      "april",   "maj",      "juni",
                                   "juli",    "augusti",  "september", "oktober", "november", "december"};
    return std::string(weekdays[std::chrono::weekday{std::chrono::sys_days{date}}.c_encoding()]) + " " +
           std::to_string(unsigned(date.day())) + " " + months[unsigned(date.month()) - 1] + " " +
           std::to_string(int(date.year()));
}
std::expected<Passage, std::string> normalize_passage(Passage passage) {
    if (passage.book.empty() || passage.first.chapter < 1 || passage.first.verse < 1 ||
        passage.last.chapter < 1 || passage.last.verse < 1)
        return std::unexpected("Invalid passage coordinates");
    if (passage.last < passage.first)
        std::swap(passage.first, passage.last);
    return passage;
}
void SelectedDay::select(CivilDate date) {
    if (!date.ok() || int(date.year()) < 1 || int(date.year()) > 9999)
        throw std::invalid_argument("Invalid selected date");
    date_ = date;
}
std::string source_language(const std::string& source) {
    return source.starts_with("grc") ? "el" : source.starts_with("en") ? "en" : "sv";
}
} // namespace ortho
