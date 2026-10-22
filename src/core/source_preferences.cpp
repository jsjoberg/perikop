#include "core/source_preferences.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace ortho {
std::string encode_sources(const std::vector<SourcePreference>& sources) {
    std::string value;
    for (const auto& source : sources) {
        if (!value.empty())
            value += ',';
        value += source.source + (source.enabled ? "=1" : "=0");
    }
    return value;
}
std::vector<SourcePreference> decode_sources(const std::string& value,
                                             const std::vector<SourcePreference>& defaults) {
    std::vector<SourcePreference> result;
    std::set<std::string> seen;
    std::istringstream stream(value);
    for (std::string item; std::getline(stream, item, ',');) {
        const auto equals = item.find('=');
        if (equals == std::string::npos || (item.substr(equals) != "=1" && item.substr(equals) != "=0"))
            continue;
        const auto source = item.substr(0, equals);
        if (!source.empty() &&
            source.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") == std::string::npos &&
            seen.insert(source).second)
            result.push_back({source, item.back() == '1'});
    }
    for (const auto& source : defaults)
        if (!seen.contains(source.source))
            result.push_back(source);
    return result;
}
std::vector<std::string> chapter_sources(const CorpusDb& corpus, const std::string& frame,
                                         const std::string& book, int chapter) {
    std::vector<std::string> result;
    for (const auto& source : corpus.sources())
        for (const auto& ref : corpus.coordinates(frame, book))
            if (ref.chapter == chapter && corpus.parallel_verse(frame, source.code, book, ref)) {
                result.push_back(source.code);
                break;
            }
    return result;
}
std::string preferred_source(const std::vector<SourcePreference>& preferences,
                             const std::vector<std::string>& available, const std::string& exclude) {
    for (const auto& source : preferences)
        if (source.enabled && source.source != exclude &&
            std::ranges::find(available, source.source) != available.end())
            return source.source;
    return {};
}
} // namespace ortho
