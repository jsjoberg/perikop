#include "core/source_preferences.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ortho;
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected corpus and temporary profile paths");
        CorpusDb corpus(argv[1]);
        Settings settings;
        check(settings.primary_sources.front().source == "sv1917", "Swedish is the primary default");
        check(std::ranges::none_of(settings.primary_sources,
                                   [](const auto& preference) {
                                       return preference.enabled &&
                                              source_language(preference.source) == "el";
                                   }),
              "Greek is initially disabled in the main text");
        const auto john = chapter_sources(corpus, "sv1917", "John", 1);
        check(preferred_source(settings.primary_sources, john) == "sv1917", "Primary chapter is Swedish");
        check(preferred_source(settings.secondary_sources, john, "sv1917") == "en-web",
              "Secondary New Testament uses WEB, excluding the main edition");
        check(preferred_source(settings.secondary_sources, {"sv1917"}).empty(),
              "Swedish is initially disabled in the second pane");
        const auto esdras = chapter_sources(corpus, "grc-lxx", "1Esd", 1);
        check(preferred_source(settings.primary_sources, esdras) == "en-lxx2012",
              "A chapter without Swedish text falls back to enabled English");
        settings.primary_sources =
            decode_sources("grc-patriarchal=1,en-web=1,sv1917=0", settings.primary_sources);
        check(preferred_source(settings.primary_sources, john) == "grc-patriarchal",
              "An enabled Greek edition can become the main text");
        check(preferred_source({{"sv1917", false}, {"en-web", false}}, john).empty(),
              "Disabled sources are never used as a silent fallback");
        const auto decoded =
            decode_sources("en-web=1,en-web=0,broken=2,new-edition=0", Settings{}.primary_sources);
        check(std::ranges::count(decoded, "en-web", &SourcePreference::source) == 1 &&
                  decoded.front().enabled,
              "Malformed entries and duplicate editions do not corrupt order");
        check(std::ranges::find(decoded, "new-edition", &SourcePreference::source) != decoded.end(),
              "New source identifiers survive storage");
        check(decode_sources(encode_sources(decoded), Settings{}.primary_sources) == decoded,
              "Source order and enabled state round-trip");
        const std::filesystem::path path(argv[2]);
        std::filesystem::remove(path);
        {
            UserDb user(path);
            settings.split_view = true;
            settings.secondary_sources = decode_sources("sv1917=1,en-web=0", settings.secondary_sources);
            user.save(settings);
        }
        {
            UserDb user(path);
            const auto restored = user.load();
            check(restored.split_view && restored.primary_sources == settings.primary_sources &&
                      restored.secondary_sources == settings.secondary_sources,
                  "Reopened profile retains both priority lists and split view");
        }
        std::filesystem::remove(path);
        std::cout << "Source preferences: defaults, fallback, Greek opt-in and persistence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
