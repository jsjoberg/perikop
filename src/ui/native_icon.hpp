#pragma once
#include <span>
namespace ortho {
#ifdef __APPLE__
void set_native_app_icon(std::span<const unsigned char>);
#else
inline void set_native_app_icon(std::span<const unsigned char>) {}
#endif
} // namespace ortho
