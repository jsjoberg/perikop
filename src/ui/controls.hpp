#pragma once

#include "ui/theme.hpp"
#include <filesystem>
#include <functional>
#include <wx/button.h>
#include <wx/stattext.h>

namespace ortho::ui {
wxString utf8(const std::string& text);
std::string to_utf8(const wxString& text);
std::filesystem::path filesystem_path(const wxString& text);
wxButton* button(wxWindow* parent, const wxString& text, const std::function<void()>& action);
wxStaticText* label(wxWindow* parent, const wxString& text, int points = 11);
void recolor(wxWindow* window, const Palette& colors);
} // namespace ortho::ui
