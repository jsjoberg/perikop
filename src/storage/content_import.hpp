#pragma once
#include <filesystem>
#include <functional>
#include <string>

namespace ortho {
inline constexpr const char* srb_source = "sv-srb2026";
// The two publisher files are required together. Nothing is downloaded by this API.
// Validation and extraction finish before a transaction replaces local content.
struct ContentImportResult {
    int books = 0, verses = 0;
};
using ImportProgress = std::function<void(int)>;
ContentImportResult import_reformationsbibeln(const std::filesystem::path& old_testament,
                                              const std::filesystem::path& new_testament,
                                              const std::filesystem::path& recipe,
                                              const std::filesystem::path& destination,
                                              const ImportProgress& progress = {});
} // namespace ortho
