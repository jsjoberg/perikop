#pragma once

#include "core/reading_plan.hpp"
#include <wx/string.h>

namespace ortho::ui {
inline constexpr std::size_t reading_preview_limit = 180;
// One line, cut at a word boundary. Ellipsis also marks a reading that continues.
wxString text_preview(const std::string&, bool continues = false);
// Swedish text where available, otherwise English or Greek with a language label.
wxString verse_tooltip(const CorpusDb&, const std::string& frame, const std::string& book, VerseRef,
                       bool preview, bool continues = false);
// The beginning of an already localized reading, in its framing coordinates.
wxString reading_tooltip(const CorpusDb&, const Reading&);
wxString plan_parts_tooltip(const CorpusDb&, const PlanPart&);
} // namespace ortho::ui
