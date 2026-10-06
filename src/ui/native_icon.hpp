#pragma once
#include <filesystem>
namespace ortho {
#ifdef __APPLE__
void set_native_app_icon(const std::filesystem::path&);
#else
inline void set_native_app_icon(const std::filesystem::path&) {}
#endif
} // namespace ortho
