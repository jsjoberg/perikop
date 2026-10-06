#pragma once

#include "core/model.hpp"
#include <wx/window.h>

namespace ortho {
std::optional<CivilDate> pick_civil_date(wxWindow* parent, CivilDate current, Theme theme);
}
