#include "core/model.hpp"
#include <algorithm>
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
bool new_testament_book(const std::string& book) {
    static const std::vector<std::string> books={"Matt","Mark","Luke","John","Acts","Rom","1Cor","2Cor","Gal","Eph","Phil","Col","1Thess","2Thess","1Tim","2Tim","Titus","Philemon","Heb","James","1Peter","2Peter","1John","2John","3John","Jude","Rev"};
    for(const auto& code:books)if(code==book)return true;
    return false;
}
std::string source_for_language(const std::string& language, const std::string& book) {
    if (language == "sv") return "sv1917";
    if (language == "en") {
        // KJV remains the familiar main edition; WEB supplies deuterocanonical books.
        const std::vector<std::string> deuterocanon={"Tob","Jdt","EsthGr","Wis","Sir","Baruch","EpJer","PrAzar","Sus","Bel","PrMan","Ps151","1Macc","2Macc","3Macc","4Macc","1Esd","2Esd","DanGr"};
        return std::find(deuterocanon.begin(),deuterocanon.end(),book)!=deuterocanon.end()?"en-web":"en-kjv";
    }
    if (language == "el") return new_testament_book(book) ? "grc-patriarchal" : "grc-lxx";
    return {};
}
}
