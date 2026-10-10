// Calendar rules adapted from Orthocal (c) 2022 Brian Glass, MIT: its GreekYear for
// the Greek and Antiochian reading orders, its SlavicYear for the Slavic one.
// Scripture wording is supplied by the separate corpus.
#include "core/fasting.hpp"
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
    bool slavic;
    std::map<int, int> floats, luke, interpolation;
    // Slavic: Sunday Gospels left unread in autumn, read again after Theophany.
    std::vector<int> reserves;
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
    // Slavic: Sundays from the one after Theophany to the one before Zacchaeus.
    int slavic_extra() const {
        return floor_div(next - pascha - 84 - sun_after_theophany, 7);
    }
    Year(int y, CalendarStyle calendar, bool slavic_year)
        : number(y), pascha(jdn(orthodox_pascha(y))), next(jdn(orthodox_pascha(y + 1))), style(calendar),
          slavic(slavic_year) {
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
        if (slavic)
            make_reserves();
        else {
            make_luke();
            make_interpolation();
        }
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
    bool no_paremias(int p) const {
        constexpr std::array<std::pair<int, int>, 8> feasts = {
            {{2, 24}, {2, 27}, {3, 9}, {3, 31}, {4, 7}, {4, 23}, {4, 25}, {4, 30}}};
        for (const auto& [m, d] : feasts)
            if (const int position = date(m, d);
                position == p && -44 < position && position < -7 && mod(position, 7) > 1)
                return true;
        return false;
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
        if (slavic) {
            // The Synaxis of the Unmercenaries on the Sunday after November 1,
            // and the New Martyrs of Russia on the Sunday nearest January 25.
            const int unmercenaries = date(11, 1), martyrs = date(1, 25, number + 1);
            floats[unmercenaries + 7 - mod(unmercenaries, 7)] = 1004;
            floats[martyrs - mod(martyrs, 7) + (mod(martyrs, 7) < 4 ? 0 : 7)] = 1031;
        }
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
    void make_reserves() {
        const int first = 49 + 7 * 18, thirteenth = first + 7 * 13, extra = slavic_extra();
        if (!extra)
            return;
        // Sunday Gospels displaced by the feasts from Forefathers to Theophany,
        // then those the Lukan jump skipped.
        for (int p = forefathers + lukan_jump + 7; p <= thirteenth; p += 7)
            reserves.push_back(p);
        if (const int remainder = extra - int(reserves.size()))
            for (int p = first - remainder * 7; p < first - 6; p += 7)
                reserves.push_back(p);
    }
    // Greek: -1 suppresses an ordinary reading, -999 means no override.
    int sunday_gospel(int p) const {
        if (p == -77 && Year(number - 1, style, false).extra() >= 4)
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
// How a row's tradition tag applies to a reading order: -1 excludes it, and a
// higher rank replaces a lower one in the same slot.
int tradition_rank(const std::string& tag, Tradition tradition) {
    if (tag == "common")
        return 0;
    if (tradition == Tradition::Slavic)
        return tag == "slavic" ? 1 : -1;
    if (tag == "greek")
        return 1;
    return tradition == Tradition::Antiochian && tag == "antiochian" ? 2 : -1;
}
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
Lectionary::Lectionary(const CorpusDb& corpus)
    : rules_(corpus.reading_rules()), feasts_(corpus.feast_rules()), ordos_(corpus.ordo_rules()),
      commemorations_(corpus.commemoration_rules()) {}
DayReadings Lectionary::readings_for(CivilDate civil, CalendarStyle style, Tradition tradition) const {
    return calculate(civil, style, tradition, false);
}
DayReadings Lectionary::service_readings_for(CivilDate civil, CalendarStyle style,
                                             Tradition tradition) const {
    return calculate(civil, style, tradition, true);
}
DayReadings Lectionary::calculate(CivilDate civil, CalendarStyle style, Tradition tradition,
                                  bool full) const {
    if (!civil.ok() || int(civil.year()) < 1 || int(civil.year()) > 9999)
        throw std::invalid_argument("Invalid lectionary civil date");
    const bool slavic = tradition == Tradition::Slavic;
    const auto fixed = fixed_calendar_date(civil, style);
    int py = int(fixed.year());
    int p = jdn(civil) - jdn(orthodox_pascha(py));
    if (p < -77) {
        --py;
        p = jdn(civil) - jdn(orthodox_pascha(py));
    }
    const Year year(py, style, slavic);
    const int weekday = mod(p, 7), month = int(unsigned(fixed.month())), day = int(unsigned(fixed.day()));
    const int floating = year.floats.contains(p) ? year.floats.at(p) : -999;
    int ep = year.daily(p) ? p : -999, gospel = ep;
    if (ep != -999) {
        const int override = weekday == 0 && !slavic ? year.sunday_gospel(p) : -999;
        if (override == -1) {
            ep = gospel = -999;
        } else {
            if (override != -999) {
                gospel = override;
                if (!(p >= year.first_luke && p <= year.forefathers))
                    ep = override;
            } else if (slavic && p == year.first_luke + 70)
                // The eleventh Sunday of Luke reads the Forefathers Gospel.
                gospel = year.forefathers + year.lukan_jump;
            else if (const int i = (p - year.sun_after_theophany) / 7;
                     slavic && weekday == 0 && p > year.sun_after_theophany && year.slavic_extra() > 1 &&
                     i >= 1 && i <= int(year.reserves.size()))
                gospel = year.reserves[std::size_t(i - 1)];
            else if (p > year.sat_before_theophany)
                gospel = jdn(civil) - year.next;
            else if (p > year.sun_after_cross)
                gospel = p + year.lukan_jump;
            if (ep == 49 + 29 * 7)
                ep = year.forefathers;
            else if (ep >= 49 + 32 * 7)
                ep = jdn(civil) - year.next;
        }
    }
    // A jurisdiction's published assignments override a computed pointer only for their year.
    const char* jurisdiction = tradition == Tradition::Antiochian ? "antiochian"
                               : tradition == Tradition::Greek    ? "greek"
                                                                  : nullptr;
    bool annual = false;
    if (style == CalendarStyle::New && jurisdiction)
        for (const auto& o : ordos_)
            if (o.jurisdiction == jurisdiction && o.year == int(civil.year()) &&
                o.month == int(unsigned(civil.month())) && o.day == int(unsigned(civil.day()))) {
                if (o.service == "Gospel")
                    gospel = o.pdist;
                if (o.service == "Epistle")
                    ep = o.pdist;
                annual = true;
            }
    // A tradition's row overrides the fields it sets in the common row for the same day.
    std::map<std::tuple<int, int, int>, std::pair<int, FeastRule>> merged;
    std::set<int> commemoration_days;
    for (const auto& f : feasts_) {
        const bool matches =
            (f.month == month && f.day == day) || (f.month == 0 && (f.pdist == p || f.pdist == floating));
        if (matches)
            commemoration_days.insert(f.id);
        const int rank = tradition_rank(f.tradition, tradition);
        if (rank < 0 || !matches)
            continue;
        auto key = std::tuple{f.pdist, f.month, f.day};
        auto it = merged.find(key);
        if (it == merged.end())
            merged.emplace(key, std::pair{rank, f});
        else if (rank > it->second.first) {
            it->second.first = rank;
            auto& base = it->second.second;
            if (f.rank != -100)
                base.rank = f.rank;
            if (!f.title.empty())
                base.title = f.title;
            if (!f.feast.empty())
                base.feast = f.feast;
            if (f.fast >= 0)
                base.fast = f.fast;
            if (f.fast_exception >= 0)
                base.fast_exception = f.fast_exception;
            if (f.fast_cap_exempt >= 0)
                base.fast_cap_exempt = f.fast_cap_exempt;
        }
    }
    int fixed_rank = 0, moveable_rank = 0, feast_level = 0;
    std::string title;
    std::vector<FeastRule> day_rules;
    std::set<int> winning_days;
    for (const auto& [key, ranked] : merged) {
        (void)key;
        const auto& f = ranked.second;
        day_rules.push_back(f);
        winning_days.insert(f.id);
        (f.month ? fixed_rank : moveable_rank) = std::max(f.month ? fixed_rank : moveable_rank, f.rank);
        feast_level = std::max(feast_level, f.rank);
        const auto& name = f.feast.empty() ? f.title : f.feast;
        if (!name.empty()) {
            if (!title.empty())
                title += " · ";
            title += swedish_title(name).value_or(name);
        }
    }
    std::vector<const ReadingRule*> selected;
    bool unpaired = false;
    // Slavic memorial Saturdays of Lent keep their readings for the departed
    // unless a feast cancels the memorial.
    const bool no_memorial =
        (p == -36 || p == -29 || p == -22) && month == 3 && (day == 9 || day == 24 || day == 25 || day == 26);
    // Resolve tradition overrides at identical recurring slots.
    using Slot = std::tuple<int, int, int, std::string, int, std::string>;
    std::map<Slot, std::pair<int, const ReadingRule*>> candidates;
    const auto next_fixed = fixed_calendar_date(from_jdn(jdn(civil) + 1), style);
    const bool fixed_matins = weekday != 0 || (!(p > -8 && p < 50) && feast_level >= 7);
    const int eothinon = weekday == 0 && !(p > -8 && p < 50) && feast_level < 7
                             ? 701 + mod(floor_div(p - 49, 7) - 1, 11)
                             : -999;
    for (const auto& r : rules_) {
        const int rank = tradition_rank(r.tradition, tradition);
        bool fixed_match = r.month == month && r.day == day;
        if (full && fixed_match) {
            if ((!fixed_matins && r.service == "Matins Gospel") ||
                (year.no_paremias(p) && r.service == "Vespers") ||
                (month == 3 && day == 26 && (weekday == 1 || weekday == 2 || weekday == 4) &&
                 r.description == "Theotokos"))
                fixed_match = false;
        }
        const bool float_match = r.month == 0 && r.pdist == floating;
        const bool ordinary =
            r.month == 0 &&
            ((r.service == "Epistle" && r.pdist == ep) || (r.service == "Gospel" && r.pdist == gospel) ||
             (r.service != "Epistle" && r.service != "Gospel" && r.pdist == p));
        const bool matins_cycle = full && r.month == 0 && r.pdist == eothinon;
        const bool moved = full && year.no_paremias(p + 1) && r.month == int(unsigned(next_fixed.month())) &&
                           r.day == int(unsigned(next_fixed.day())) && r.service == "Vespers";
        if (rank < 0 || !(fixed_match || float_match || ordinary || matins_cycle || moved) ||
            (r.description == "Departed" && (no_memorial || (!full && !slavic))) ||
            (!full && !r.reading.can_open()))
            continue;
        // The first common row holds its slot; a tradition's row replaces it.
        Slot slot{r.pdist, r.month, r.day, r.service, r.ordering, r.description};
        if (auto it = candidates.find(slot);
            it == candidates.end() || rank > it->second.first || (rank > 0 && rank == it->second.first))
            candidates[slot] = {rank, &r};
    }
    auto pick = [&](const std::string& service) {
        const ReadingRule* winner = nullptr;
        int best = -999999;
        for (const auto& [slot, ranked] : candidates) {
            (void)slot;
            const auto [rank, r] = ranked;
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
                              r->ordering + (rank > 0 ? 100 : 0);
            if (score > best) {
                best = score;
                winner = r;
            }
        }
        return winner;
    };
    if (full) {
        for (const auto& [slot, ranked] : candidates) {
            (void)slot;
            selected.push_back(ranked.second);
        }
    } else if (slavic) {
        // Orthocal's choice of one Epistle and Gospel, measured against oca.org: a
        // floating feast's pair first, then a proper of rank 3 or more on a weekday,
        // else the ordinary daily pair before a saint's. Fixed propers need rank 2,
        // and none are read in Clean Week or on Holy Monday to Wednesday. Unlike
        // Orthocal, a higher-ranked moveable day keeps its pair, as Ascension does
        // over Constantine and Helen in oca.org's 2023 and 2026 lectionaries.
        const bool strict = (p >= -48 && p <= -44) || (p >= -6 && p <= -4);
        std::vector<const ReadingRule*> floats, propers, ordinary;
        for (const auto& [slot, ranked] : candidates) {
            (void)slot;
            const auto* r = ranked.second;
            if (r->service != "Epistle" && r->service != "Gospel")
                continue;
            if (r->pdist >= 1000)
                floats.push_back(r);
            else if (!r->month)
                ordinary.push_back(r);
            else if (feast_level >= 2 && !strict)
                propers.push_back(r);
        }
        const auto pair = [](std::vector<const ReadingRule*> group) {
            std::ranges::sort(group, {}, [](auto r) {
                return std::pair{r->ordering, r->id};
            });
            const auto epistle = std::ranges::find(group, std::string("Epistle"), &ReadingRule::service);
            if (epistle == group.end())
                return std::vector<const ReadingRule*>{};
            auto gospel = std::find_if(epistle, group.end(), [](auto r) {
                return r->service == "Gospel";
            });
            if (gospel == group.end())
                gospel = std::ranges::find(group, std::string("Gospel"), &ReadingRule::service);
            if (gospel == group.end())
                return std::vector<const ReadingRule*>{};
            return std::vector{*epistle, *gospel};
        };
        const bool prefer_propers = fixed_rank >= 3 && fixed_rank >= moveable_rank && weekday != 0;
        for (const auto* group :
             {&floats, prefer_propers ? &propers : &ordinary, prefer_propers ? &ordinary : &propers}) {
            selected = pair(*group);
            if (!selected.empty())
                break;
        }
        // Without a pair anywhere, Orthocal keeps whichever readings it found,
        // such as the Gospels of Holy Week beside its prophecies.
        if (selected.empty()) {
            unpaired = true;
            selected = floats;
            selected.insert(selected.end(), propers.begin(), propers.end());
            selected.insert(selected.end(), ordinary.begin(), ordinary.end());
            if (const auto paired = pair(selected); !paired.empty()) {
                selected = paired;
                unpaired = false;
            }
        }
    } else {
        if (const auto* r = pick("Epistle"))
            selected.push_back(r);
        if (const auto* r = pick("Gospel"))
            selected.push_back(r);
    }
    // Weekday Lent has prophecy readings instead of a daily Apostle/Gospel.
    if (!full && (selected.empty() || unpaired))
        for (const auto& [slot, ranked] : candidates) {
            (void)slot;
            const auto* r = ranked.second;
            if (r->month == 0 && r->pdist == p && (r->service == "6th Hour" || r->service == "Vespers"))
                selected.push_back(r);
        }
    const bool matins_first = full && -42 < p && p < -7 && feast_level < 7;
    const auto reading_order = [matins_first](const auto* r) {
        return std::tuple{!(matins_first && r->service == "Matins Gospel"), r->ordering, r->id};
    };
    // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order): sorted by service and ordering, not
    // address.
    std::sort(selected.begin(), selected.end(), [&](auto a, auto b) {
        return reading_order(a) < reading_order(b);
    });
    DayReadings result{{civil,
                        style,
                        date_iso(fixed),
                        "Pascha " + date_iso(orthodox_pascha(py)) + " · dag " + std::to_string(p),
                        title,
                        annual,
                        {},
                        {}},
                       {}};
    for (const auto* r : selected)
        result.readings.push_back(r->reading);
    if (full) {
        result.day.fasting = resolve_fasting(day_rules, p, weekday, feast_level, year.date(6, 29),
                                             year.nativity, year.theophany, slavic);
        std::set<int> civil_days;
        if (style == CalendarStyle::Old)
            for (const auto& f : feasts_)
                if (f.month == int(unsigned(civil.month())) && f.day == int(unsigned(civil.day())))
                    civil_days.insert(f.id);
        std::vector<std::string> native, additive;
        for (const auto& c : commemorations_) {
            if (tradition_rank(c.tradition, tradition) < 0)
                continue;
            const bool civil_anchored = style == CalendarStyle::Old && c.new_style;
            if (!(civil_anchored ? civil_days : commemoration_days).contains(c.day_id))
                continue;
            if (!civil_anchored && c.ordering < 0 && winning_days.contains(c.day_id))
                continue;
            (c.day_native && !civil_anchored && c.ordering >= 0 ? native : additive)
                .push_back(swedish_title(c.title).value_or(c.title));
        }
        native.insert(native.end(), additive.begin(), additive.end());
        std::set<std::string> shown;
        for (auto& name : native)
            if (shown.insert(name).second)
                result.day.commemorations.push_back(std::move(name));
    }
    return result;
}
DayReadings Lectionary::readings_with_variants(CivilDate civil, CalendarStyle style,
                                               Tradition tradition) const {
    if (tradition == Tradition::Slavic)
        return service_readings_for(civil, style, tradition);
    auto result = service_readings_for(civil, style, Tradition::Greek);
    const auto antiochian = service_readings_for(civil, style, Tradition::Antiochian);
    const auto same_passage = [](const Passage& a, const Passage& b) {
        return a.book == b.book && a.first == b.first && a.last == b.last;
    };
    const auto same_reading = [&](const Reading& a, const Reading& b) {
        return a.kind == b.kind && a.service == b.service && a.reference == b.reference &&
               (a.can_open() || a.citation == b.citation) && same_passage(a.passage, b.passage) &&
               std::ranges::equal(a.additional, b.additional, same_passage);
    };
    // Match shared readings first so that repeated services retain the correct alternatives.
    std::vector<bool> matched(result.readings.size()), shared(antiochian.readings.size());
    for (std::size_t j = 0; j < antiochian.readings.size(); ++j)
        for (std::size_t i = 0; i < result.readings.size(); ++i)
            if (!matched[i] && same_reading(result.readings[i], antiochian.readings[j])) {
                matched[i] = shared[j] = true;
                break;
            }
    const std::string jurisdiction = "Antiokiska ärkestiftet i Nordamerika";
    const auto explanation = [&](const Reading* primary, const Reading& alternative) {
        std::string text;
        if (!primary)
            text = "Den antiokiska ordningen anger en ytterligare läsning.";
        else if (primary->can_open() && alternative.can_open() &&
                 primary->passage.book == alternative.passage.book &&
                 primary->passage.first == alternative.passage.first &&
                 primary->reference == alternative.reference)
            text = "Den antiokiska ordningen har en annan versavgränsning.";
        else
            text = "De två kyrkornas läsordningar anger olika bibelställen denna dag.";
        if (result.day.title != antiochian.day.title && !antiochian.day.title.empty())
            text += " Antiokisk dagsrubrik: " + antiochian.day.title + ".";
        return text;
    };
    for (std::size_t j = 0; j < antiochian.readings.size(); ++j) {
        if (shared[j])
            continue;
        const auto& alternative = antiochian.readings[j];
        std::optional<std::size_t> index;
        for (std::size_t i = 0; i < result.readings.size(); ++i)
            if (!matched[i] && result.readings[i].service == alternative.service) {
                index = i;
                matched[i] = true;
                break;
            }
        result.variants.push_back({index, alternative, jurisdiction,
                                   explanation(index ? &result.readings[*index] : nullptr, alternative)});
    }
    for (std::size_t i = 0; i < result.readings.size(); ++i)
        if (!matched[i])
            result.variants.push_back(
                {i, std::nullopt, jurisdiction, "Den antiokiska ordningen anger ingen motsvarande läsning."});
    return result;
}
} // namespace ortho
