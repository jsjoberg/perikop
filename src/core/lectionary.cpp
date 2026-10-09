// Calendar rules adapted from Orthocal (c) 2022 Brian Glass, MIT.
// Greek tradition only. Scripture wording is supplied by the separate corpus.
#include "core/model.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
namespace ortho {
namespace {
using namespace std::chrono;
int mod(int n, int d) {
    return (n % d + d) % d;
}
int julian_jdn(int y, int m, int d) {
    const int a = (14 - m) / 12;
    y += 4800 - a;
    m += 12 * a - 3;
    return d + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
}
CivilDate from_jdn(int jdn) {
    return CivilDate{sys_days{days{jdn - 2440588}}};
}
int jdn(CivilDate d) {
    return int(sys_days{d}.time_since_epoch().count()) + 2440588;
}
int floor_div(int n, int d) {
    return (n - mod(n, d)) / d;
}
bool revised_leap(int y) {
    return mod(y, 4) == 0 && (mod(y, 100) != 0 || mod(y, 900) == 200 || mod(y, 900) == 600);
}
int revised_jdn(int y, int m, int d) {
    // Proleptic Revised Julian calendar; same epoch as Gregorian, distinct leap rule.
    const int n = y - 1;
    const int leaps = floor_div(n, 4) - floor_div(n, 100) + floor_div(n + 700, 900) + floor_div(n + 300, 900);
    constexpr std::array<int, 12> preceding = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return 1721426 + 365 * n + leaps + preceding.at(m - 1) + (m > 2 && revised_leap(y)) + d - 1;
}
CivilDate revised_label(int value) {
    int y = int(from_jdn(value).year());
    while (value < revised_jdn(y, 1, 1))
        --y;
    while (value >= revised_jdn(y + 1, 1, 1))
        ++y;
    int m = 1;
    while (m < 12 && value >= revised_jdn(y, m + 1, 1))
        ++m;
    return year{y} / month{unsigned(m)} / day{unsigned(value - revised_jdn(y, m, 1) + 1)};
}
CivilDate julian_label(int value) {
    const int c = value + 32082, d = (4 * c + 3) / 1461, e = c - 1461 * d / 4, m = (5 * e + 2) / 153;
    return year{d - 4800 + m / 10} / month{unsigned(m + 3 - 12 * (m / 10))} /
           day{unsigned(e - (153 * m + 2) / 5 + 1)};
}
struct Year {
    int number, pascha, next, theophany, elevation, nativity, annunciation, forefathers, first_luke,
        lukan_jump;
    int sat_before_cross, sun_before_cross, sat_after_cross, sun_after_cross;
    int sat_before_nativity, sun_before_nativity, sat_after_nativity, sun_after_nativity;
    int sat_before_theophany, sun_before_theophany, sat_after_theophany, sun_after_theophany;
    CalendarStyle style;
    std::map<int, int> floats, luke, interpolation;
    int date(int m, int d, int y = 0) const {
        if (!y)
            y = number;
        return (style == CalendarStyle::Old ? julian_jdn(y, m, d) : revised_jdn(y, m, d)) - pascha;
    }
    static std::array<int, 4> weekends(int p) {
        const int w = mod(p, 7);
        return {p - w - 1, p - 7 + mod(7 - w, 7), p + 7 - mod(w + 1, 7), p + 7 - w};
    }
    int extra() const {
        int value = (next - pascha - 70 - sun_after_theophany) / 7;
        if (mod(theophany + 8, 7) == 0)
            --value;
        return value;
    }
    Year(int y, CalendarStyle calendar)
        : number(y), pascha(jdn(orthodox_pascha(y))), next(jdn(orthodox_pascha(y + 1))), style(calendar) {
        theophany = date(1, 6, y + 1);
        elevation = date(9, 14);
        nativity = date(12, 25);
        annunciation = date(3, 25);
        std::tie(sat_before_cross, sun_before_cross, sat_after_cross, sun_after_cross) =
            as_tuple(weekends(elevation));
        std::tie(sat_before_nativity, sun_before_nativity, sat_after_nativity, sun_after_nativity) =
            as_tuple(weekends(nativity));
        std::tie(sat_before_theophany, sun_before_theophany, sat_after_theophany, sun_after_theophany) =
            as_tuple(weekends(theophany));
        forefathers = nativity - 14 + mod(7 - mod(nativity, 7), 7);
        first_luke = sun_after_cross + 7;
        lukan_jump = 168 - sun_after_cross;
        make_floats();
        make_luke();
        make_interpolation();
    }
    static std::tuple<int, int, int, int> as_tuple(const std::array<int, 4>& a) {
        return std::tuple{a[0], a[1], a[2], a[3]};
    }
    bool daily(int p) const {
        const std::set<int> suppressed = {sun_before_theophany, sun_after_theophany, theophany - 5,
                                          theophany - 1,        theophany,           forefathers,
                                          sun_before_nativity,  nativity - 1,        nativity,
                                          nativity + 1,         sun_after_nativity};
        if (suppressed.contains(p))
            return false;
        if (p == sat_after_theophany && sat_after_theophany == theophany + 1)
            return false;
        return !(p == annunciation && mod(annunciation, 7) == 6);
    }
    void make_floats() {
        const int july = date(7, 16), fathers = july + (mod(july, 7) < 4 ? -mod(july, 7) : 7 - mod(july, 7));
        const int october = date(10, 11), fathers7 = october + mod(7 - mod(october, 7), 7),
                  november = date(11, 1);
        floats = {{fathers, 1001},
                  {fathers7, 1002},
                  {date(10, 26) - mod(date(10, 26), 7) - 1, 1003},
                  {november + mod(6 - mod(november, 7), 7), 1039},
                  {sun_before_cross, 1007},
                  {sat_after_cross, 1008},
                  {sun_after_cross, 1009},
                  {forefathers, 1010},
                  {sat_after_theophany, 1029},
                  {sun_after_theophany, 1030}};
        if (mod(theophany + 8, 7) != 0 && mod(theophany + 8, 7) != 6)
            floats[theophany + 8] = 1038;
        if (sat_before_cross == date(9, 8))
            floats[elevation - 1] = 1005;
        else
            floats[sat_before_cross] = 1006;
        const int eve = nativity - 1;
        if (eve == sat_before_nativity) {
            floats[nativity - 2] = 1013;
            floats[sun_before_nativity] = 1012;
            floats[eve] = 1015;
        } else if (eve == sun_before_nativity) {
            floats[nativity - 3] = 1013;
            floats[sat_before_nativity] = 1011;
            floats[eve] = 1016;
        } else {
            floats[eve] = 1014;
            floats[sat_before_nativity] = 1011;
            floats[sun_before_nativity] = 1012;
        }
        switch (mod(nativity, 7)) {
        case 0:
            floats[sat_after_nativity] = 1017;
            floats[nativity + 1] = 1020;
            floats[sun_before_theophany] = 1024;
            floats[theophany - 1] = 1026;
            break;
        case 1:
            floats[sat_after_nativity] = 1017;
            floats[sun_after_nativity] = 1021;
            floats[theophany - 5] = 1023;
            floats[theophany - 1] = 1026;
            break;
        case 2:
            floats[sat_after_nativity] = 1019;
            floats[sun_after_nativity] = 1021;
            floats[sat_before_theophany] = 1027;
            floats[theophany - 5] = 1023;
            floats[theophany - 2] = 1025;
            break;
        case 3:
            floats[sat_after_nativity] = 1019;
            floats[sun_after_nativity] = 1021;
            floats[sat_before_theophany] = 1022;
            floats[sun_before_theophany] = 1028;
            floats[theophany - 3] = 1025;
            break;
        case 4:
        case 5:
            floats[sat_after_nativity] = 1019;
            floats[sun_after_nativity] = 1021;
            floats[sat_before_theophany] = 1022;
            floats[sun_before_theophany] = 1024;
            floats[theophany - 1] = 1026;
            break;
        case 6:
            floats[nativity + 6] = 1018;
            floats[sun_after_nativity] = 1021;
            floats[sat_before_theophany] = 1022;
            floats[sun_before_theophany] = 1024;
            floats[theophany - 1] = 1026;
            break;
        }
        switch (mod(annunciation, 7)) {
        case 6:
            floats[annunciation - 1] = 1032;
            floats[annunciation] = 1033;
            break;
        case 0:
            floats[annunciation] = 1034;
            break;
        case 1:
            floats[annunciation] = 1035;
            break;
        default:
            floats[annunciation - 1] = 1036;
            floats[annunciation] = 1037;
            break;
        }
    }
    void make_luke() {
        const std::array<std::array<int, 5>, 6> windows = {{{10, 11, 10, 17, 4},
                                                            {10, 30, 11, 5, 5},
                                                            {11, 24, 11, 30, 13},
                                                            {12, 1, 12, 3, 14},
                                                            {12, 4, 12, 10, 10},
                                                            {12, 11, 12, 17, 11}}};
        const std::set<int> reserved = {4, 5, 13, 14, 10, 11},
                            overrides = {date(10, 18), date(11, 16), date(11, 30)};
        int sequence = 1;
        for (int p = first_luke; p <= forefathers; p += 7) {
            int assigned = 0;
            for (const auto& w : windows)
                if (date(w[0], w[1]) <= p && p <= date(w[2], w[3])) {
                    assigned = w[4];
                    break;
                }
            if (!assigned) {
                while (reserved.contains(sequence))
                    ++sequence;
                assigned = sequence++;
            }
            if (!overrides.contains(p))
                luke[p] = assigned;
        }
    }
    void make_interpolation() {
        std::set<int> used;
        for (const auto& [p, n] : luke) {
            (void)p;
            used.insert(n);
        }
        std::vector<std::pair<bool, int>> pool;
        for (auto candidate :
             std::vector<std::pair<bool, int>>{{false, 12}, {false, 15}, {false, 14}, {true, 16}, {true, 15}})
            if (candidate.first || !used.contains(candidate.second))
                pool.push_back(candidate);
        pool.resize(std::min(pool.size(), std::size_t(std::max(0, extra() - 2))));
        std::sort(pool.begin(), pool.end());
        int p = sun_after_theophany + 7;
        if (mod(theophany + 8, 7) == 0) {
            interpolation[p] = 1030;
            p += 7;
        }
        for (auto [m, n] : pool) {
            interpolation[p] = m ? 49 + 7 * n : 168 + 7 * n;
            p += 7;
        }
    }
    // -1 suppresses an ordinary reading, -999 means no override.
    int sunday_gospel(int p) const {
        if (p == -77 && Year(number - 1, style).extra() >= 4)
            return 49 + 7 * 17;
        if (p >= first_luke && p <= forefathers) {
            auto it = luke.find(p);
            return it == luke.end() ? -1 : 168 + 7 * it->second;
        }
        if (auto it = interpolation.find(p); it != interpolation.end())
            return it->second;
        return -999;
    }
};
} // namespace
CivilDate orthodox_pascha(int y) {
    if (y < 0 || y > 10000)
        throw std::invalid_argument("Pascha year outside supported civil dates");
    const int a = y % 4, b = y % 7, c = y % 19, d = (19 * c + 15) % 30, e = (2 * a + 4 * b - d + 34) % 7;
    const int m = (d + e + 114) / 31, day = (d + e + 114) % 31 + 1;
    return from_jdn(julian_jdn(y, m, day));
}
CivilDate fixed_calendar_date(CivilDate civil, CalendarStyle style) {
    return style == CalendarStyle::Old ? julian_label(jdn(civil)) : revised_label(jdn(civil));
}
AntiochianLectionary::AntiochianLectionary(const CorpusDb& corpus)
    : rules_(corpus.reading_rules()), feasts_(corpus.feast_rules()), ordos_(corpus.ordo_rules()) {}
DayReadings AntiochianLectionary::readings_for(CivilDate civil, CalendarStyle style) const {
    if (!civil.ok() || int(civil.year()) < 1 || int(civil.year()) > 9999)
        throw std::invalid_argument("Invalid lectionary civil date");
    const auto fixed = fixed_calendar_date(civil, style);
    int py = int(fixed.year());
    int p = jdn(civil) - jdn(orthodox_pascha(py));
    if (p < -77) {
        --py;
        p = jdn(civil) - jdn(orthodox_pascha(py));
    }
    const Year year(py, style);
    const int weekday = mod(p, 7), month = int(unsigned(fixed.month())), day = int(unsigned(fixed.day()));
    const int floating = year.floats.contains(p) ? year.floats.at(p) : -999;
    int ep = year.daily(p) ? p : -999, gospel = ep;
    if (ep != -999) {
        const int override = weekday == 0 ? year.sunday_gospel(p) : -999;
        if (override == -1) {
            ep = gospel = -999;
        } else {
            if (override != -999) {
                gospel = override;
                if (!(p >= year.first_luke && p <= year.forefathers))
                    ep = override;
            } else if (p > year.sat_before_theophany)
                gospel = jdn(civil) - year.next;
            else if (p > year.sun_after_cross)
                gospel = p + year.lukan_jump;
            if (ep == 49 + 29 * 7)
                ep = year.forefathers;
            else if (ep >= 49 + 32 * 7)
                ep = jdn(civil) - year.next;
        }
    }
    // Published annual assignments override a computed pointer only for their year.
    bool annual = false;
    if (style == CalendarStyle::New)
        for (const auto& o : ordos_)
            if (o.year == int(civil.year()) && o.month == int(unsigned(civil.month())) &&
                o.day == int(unsigned(civil.day()))) {
                if (o.service == "Gospel")
                    gospel = o.pdist;
                if (o.service == "Epistle")
                    ep = o.pdist;
                annual = true;
            }
    std::map<std::tuple<int, int, int>, FeastRule> merged;
    for (const auto& f : feasts_)
        if ((f.month == month && f.day == day) || (f.month == 0 && (f.pdist == p || f.pdist == floating))) {
            auto key = std::tuple{f.pdist, f.month, f.day};
            auto it = merged.find(key);
            if (it == merged.end())
                merged.emplace(key, f);
            else if (f.tradition == "greek") {
                if (f.rank != -100)
                    it->second.rank = f.rank;
                if (!f.title.empty())
                    it->second.title = f.title;
                if (!f.feast.empty())
                    it->second.feast = f.feast;
            }
        }
    int fixed_rank = 0;
    std::string title;
    for (const auto& [key, f] : merged) {
        (void)key;
        if (f.month)
            fixed_rank = std::max(fixed_rank, f.rank);
        const auto& name = f.feast.empty() ? f.title : f.feast;
        if (!name.empty()) {
            if (!title.empty())
                title += " · ";
            title += swedish_title(name).value_or(name);
        }
    }
    std::vector<const ReadingRule*> selected;
    // Resolve tradition overrides at identical recurring slots.
    using Slot = std::tuple<int, int, int, std::string, int, std::string>;
    std::map<Slot, const ReadingRule*> candidates;
    for (const auto& r : rules_) {
        const bool fixed_match = r.month == month && r.day == day;
        const bool float_match = r.month == 0 && r.pdist == floating;
        const bool ordinary =
            r.month == 0 &&
            ((r.service == "Epistle" && r.pdist == ep) || (r.service == "Gospel" && r.pdist == gospel) ||
             (r.service != "Epistle" && r.service != "Gospel" && r.pdist == p));
        if (!(fixed_match || float_match || ordinary) || r.description == "Departed")
            continue;
        Slot slot{r.pdist, r.month, r.day, r.service, r.ordering, r.description};
        if (!candidates.contains(slot) || r.tradition == "greek")
            candidates[slot] = &r;
    }
    auto pick = [&](const std::string& service) {
        const ReadingRule* winner = nullptr;
        int best = -999999;
        for (const auto& [slot, r] : candidates) {
            (void)slot;
            if (r->service != service)
                continue;
            const bool proper = r->month != 0, fl = r->pdist >= 1000;
            // Greek Sunday Epistles may be replaced by a saint while the
            // resurrectional Gospel continues. Great feasts replace both.
            const std::set<std::pair<int, int>> sunday_epistle_feasts = {
                {1, 11}, {1, 18}, {1, 25}, {7, 5}, {7, 26}, {10, 18}, {11, 1}, {11, 8}, {12, 6}, {12, 27}};
            const bool paschal_sunday = weekday == 0 && p >= -70 && p <= 56;
            const bool prefer =
                (fixed_rank >= 7 && !(p >= -6 && p <= 6)) ||
                (!paschal_sunday &&
                 ((weekday != 0 && fixed_rank >= 3) ||
                  (weekday == 0 && service == "Epistle" && sunday_epistle_feasts.contains({month, day}))));
            const bool after_feast = floating == 1030 || floating == 1021;
            const int score = (fl       ? (after_feast && service == "Epistle" ? 15000 : 30000)
                               : proper ? (prefer ? 20000 : 0)
                                        : 10000) -
                              r->ordering + (r->tradition == "greek" ? 100 : 0);
            if (score > best) {
                best = score;
                winner = r;
            }
        }
        return winner;
    };
    if (const auto* r = pick("Epistle"))
        selected.push_back(r);
    if (const auto* r = pick("Gospel"))
        selected.push_back(r);
    // Weekday Lent has prophecy readings instead of a daily Apostle/Gospel.
    if (selected.empty())
        for (const auto& [slot, r] : candidates) {
            (void)slot;
            if (r->month == 0 && r->pdist == p && (r->service == "6th Hour" || r->service == "Vespers"))
                selected.push_back(r);
        }
    // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order): sorted by ordering, not address.
    std::sort(selected.begin(), selected.end(), [](auto a, auto b) {
        return a->ordering < b->ordering;
    });
    DayReadings result{{civil, style, date_iso(fixed),
                        "Pascha " + date_iso(orthodox_pascha(py)) + " · dag " + std::to_string(p),
                        style == CalendarStyle::New ? "Antiochia · Nordamerika"
                                                    : "Grekisk läsordning · juliansk fast kalender"},
                       {}};
    if (!title.empty())
        result.day.annotation += " · " + title;
    if (annual)
        result.day.annotation += " · publicerad årsanvisning";
    for (const auto* r : selected)
        result.readings.push_back(r->reading);
    return result;
}
} // namespace ortho
