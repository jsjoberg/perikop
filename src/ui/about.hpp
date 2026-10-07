#pragma once
#include "core/model.hpp"
#include <filesystem>
#include <wx/window.h>
namespace ortho {
// Program information and the sources Perikop builds on, with links to them.
void show_about(wxWindow* parent, Theme, const std::filesystem::path& resources);
} // namespace ortho
