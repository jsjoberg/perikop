#pragma once

#include "storage/database.hpp"
#include <wx/window.h>

namespace ortho {
std::optional<Reading> pick_bible_reading(wxWindow* parent, const CorpusDb& corpus, const Settings& settings);
}
