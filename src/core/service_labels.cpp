#include "core/model.hpp"
#include <charconv>
#include <map>
namespace ortho {
std::string service_label(const std::string& service) {
    static const std::map<std::string, std::string> names = {
        {"Epistle", "Epistel"},
        {"Gospel", "Evangelium"},
        {"Vespers", "Vesper"},
        {"Vespers Gospel", "Vesper, evangelium"},
        {"Matins", "Ortros"},
        {"Matins Gospel", "Ortros, evangelium"},
        {"Matins Epistle", "Ortros, epistel"},
        {"Great Blessing of Waters", "Stora vattenvälsignelsen"},
        {"Cross Procession", "Korsprocession"}};
    if (const auto it = names.find(service); it != names.end())
        return it->second;
    int number = 0;
    const auto parsed = std::from_chars(service.data(), service.data() + service.size(), number);
    if (parsed.ec == std::errc{}) {
        if (service.find("Hour") != std::string::npos) {
            static const std::map<int, std::string> hours = {
                {1, "Första timmen"}, {3, "Tredje timmen"}, {6, "Sjätte timmen"}, {9, "Nionde timmen"}};
            auto label = hours.contains(number) ? hours.at(number) : std::to_string(number) + ":e timmen";
            if (service.ends_with("Prophecy"))
                label += ", profetia";
            if (service.ends_with("Epistle"))
                label += ", epistel";
            if (service.ends_with("Gospel"))
                label += ", evangelium";
            return label;
        }
        if (service.ends_with("Matins Gospel"))
            return "Ortros, evangelium " + std::to_string(number);
        if (service.ends_with("Passion Gospel"))
            return "Passionsevangelium " + std::to_string(number);
    }
    return service;
}
} // namespace ortho
