// Fasting rules adapted from Orthocal (c) 2022 Brian Glass, MIT.
#include "core/fasting.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
namespace ortho {
Fasting resolve_fasting(const std::vector<FeastRule>& days, int pdist, int weekday, int feast_rank,
                        int peter_and_paul, int nativity, int theophany, bool slavic) {
    using D = DietaryAllowance;
    using P = FastPeriod;
    constexpr std::array<D, 12> rungs = {D::Strict,      D::WineOil, D::FishWineOil,   D::WineOil,
                                         D::FishWineOil, D::Wine,    D::WineOilCaviar, D::MeatFast,
                                         D::WineOil,     D::Strict,  D::Strict,        D::Free};
    Fasting result;
    std::map<int, bool> festal;
    bool exempt = false;
    for (const auto& day : days) {
        if (day.fast_exception == 11)
            return {P::None, D::Free};
        result.period = std::max(result.period, static_cast<P>(std::max(0, day.fast)));
        festal[std::max(0, day.fast_exception)] = day.month != 0;
        exempt = exempt || day.fast_cap_exempt == 1;
    }
    if (56 < pdist && pdist < peter_and_paul)
        result.period = P::Apostles;
    const bool weekend = weekday == 0 || weekday == 6;
    const bool wed_fri = weekday == 3 || weekday == 5;
    D floor = D::Strict, cap = D::Free;
    int cap_exempt_rank = 99;
    switch (result.period) {
    case P::None:
        break;
    case P::Day:
        if (!slavic && -55 <= pdist && pdist <= -49)
            floor = D::MeatFast;
        else {
            floor = weekend ? D::WineOil : D::Strict;
            if (!slavic) {
                cap = wed_fri ? D::WineOil : D::Free;
                cap_exempt_rank = 7;
            }
        }
        break;
    case P::Lent:
        cap = D::WineOil;
        cap_exempt_rank = 7;
        break;
    case P::Dormition:
        floor = weekend ? D::WineOil : D::Strict;
        cap = D::Strict;
        cap_exempt_rank = 7;
        break;
    case P::Apostles:
    case P::Nativity:
        if (slavic) {
            floor = weekend ? D::FishWineOil : (weekday == 2 || weekday == 4) ? D::WineOil : D::Strict;
            cap = wed_fri ? D::WineOil : weekend ? D::FishWineOil : D::Free;
            cap_exempt_rank = 4;
        } else if (result.period == P::Nativity && pdist >= nativity - 13) {
            floor = weekend ? D::WineOil : D::Strict;
            cap = D::WineOil;
            cap_exempt_rank = 4;
        } else {
            floor = wed_fri ? D::Strict : D::FishWineOil;
            cap = wed_fri ? D::WineOil : D::FishWineOil;
            cap_exempt_rank = result.period == P::Apostles ? 4 : 7;
        }
        break;
    }
    if (feast_rank >= cap_exempt_rank)
        cap = D::Free;
    result.allowance = floor;
    for (const auto& [index, fixed] : festal) {
        if (index == 0 || index == 9 || index == 10)
            continue;
        const auto claim = rungs.at(std::size_t(index));
        const auto effective_cap = exempt || !fixed ? D::Free : cap;
        const auto clamped =
            claim == D::WineOilCaviar && effective_cap >= D::WineOil ? claim : std::min(claim, effective_cap);
        result.allowance = std::max(result.allowance, clamped);
    }
    for (const auto& [index, fixed] : festal) {
        (void)fixed;
        if (index == 5 || index == 9 || index == 10)
            result.allowance = std::min(result.allowance, rungs.at(std::size_t(index)));
    }
    if (slavic && result.period == P::Nativity && nativity - 6 < pdist && pdist < nativity - 1)
        result.allowance = std::min(result.allowance, D::WineOil);
    if (weekend && (pdist == nativity - 1 || pdist == theophany - 1))
        result.allowance = std::max(result.allowance, D::WineOil);
    return result;
}
std::string fasting_period_label(FastPeriod period) {
    switch (period) {
    case FastPeriod::None:
        return "Ingen fasta";
    case FastPeriod::Day:
        return "Fastedag";
    case FastPeriod::Lent:
        return "Stora fastan";
    case FastPeriod::Apostles:
        return "Apostlafastan";
    case FastPeriod::Dormition:
        return "Gudsmoderns avsomnandefasta";
    case FastPeriod::Nativity:
        return "Julfastan";
    }
    return {};
}
std::string fasting_allowance_label(Fasting fast) {
    if (fast.period == FastPeriod::None)
        return fast.allowance == DietaryAllowance::Free ? "Fastefri dag" : "Ingen föreskriven fasta";
    switch (fast.allowance) {
    case DietaryAllowance::Strict:
        return "Strikt fasta";
    case DietaryAllowance::Wine:
        return "Vin tillåtet";
    case DietaryAllowance::WineOil:
        return "Vin och olja tillåtna";
    case DietaryAllowance::WineOilCaviar:
        return "Vin, olja och kaviar tillåtna";
    case DietaryAllowance::FishWineOil:
        return "Fisk, vin och olja tillåtna";
    case DietaryAllowance::MeatFast:
        return "Köttfasta";
    case DietaryAllowance::Free:
        return "Fastefri dag";
    }
    return {};
}
std::string fasting_abstentions(Fasting fast) {
    if (fast.period == FastPeriod::None || fast.allowance == DietaryAllowance::Free)
        return {};
    switch (fast.allowance) {
    case DietaryAllowance::Strict:
        return "Avstå från kött, fisk, mejeriprodukter, ägg, vin och olja.";
    case DietaryAllowance::Wine:
        return "Avstå från kött, fisk, mejeriprodukter, ägg och olja.";
    case DietaryAllowance::WineOil:
    case DietaryAllowance::WineOilCaviar:
        return "Avstå från kött, fisk, mejeriprodukter och ägg.";
    case DietaryAllowance::FishWineOil:
        return "Avstå från kött, mejeriprodukter och ägg.";
    case DietaryAllowance::MeatFast:
        return "Avstå från kött.";
    case DietaryAllowance::Free:
        break;
    }
    return {};
}
std::string service_label(const std::string& service) {
    static const std::map<std::string, std::string> names = {
        {"Epistle", "EPISTEL"},
        {"Gospel", "EVANGELIUM"},
        {"Vespers", "VESPER"},
        {"Vespers Gospel", "VESPER · EVANGELIUM"},
        {"Matins", "MATUTIN"},
        {"Matins Gospel", "MATUTIN · EVANGELIUM"},
        {"Matins Epistle", "MATUTIN · EPISTEL"},
        {"Great Blessing of Waters", "STORA VATTENVÄLSIGNELSEN"},
        {"Cross Procession", "KORSPROCESSION"}};
    if (const auto it = names.find(service); it != names.end())
        return it->second;
    int number = 0;
    const auto parsed = std::from_chars(service.data(), service.data() + service.size(), number);
    if (parsed.ec == std::errc{}) {
        if (service.find("Hour") != std::string::npos) {
            static const std::map<int, std::string> hours = {
                {1, "FÖRSTA TIMMEN"}, {3, "TREDJE TIMMEN"}, {6, "SJÄTTE TIMMEN"}, {9, "NIONDE TIMMEN"}};
            auto label = hours.contains(number) ? hours.at(number) : std::to_string(number) + ":E TIMMEN";
            if (service.ends_with("Prophecy"))
                label += " · PROFETIA";
            if (service.ends_with("Epistle"))
                label += " · EPISTEL";
            if (service.ends_with("Gospel"))
                label += " · EVANGELIUM";
            return label;
        }
        if (service.ends_with("Matins Gospel"))
            return "MATUTIN · EVANGELIUM " + std::to_string(number);
        if (service.ends_with("Passion Gospel"))
            return "PASSIONSEVANGELIUM " + std::to_string(number);
    }
    return service;
}
} // namespace ortho
