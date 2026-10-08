#pragma once

#include "core/model.hpp"
#include <functional>
#include <wx/window.h>

namespace ortho {
wxWindow* make_date_page(wxWindow* parent, CivilDate current, Theme theme,
                         const std::function<void(CivilDate)>& selected);
}
