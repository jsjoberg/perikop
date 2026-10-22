#pragma once
#include "core/model.hpp"

namespace ortho {
// Independent of the selected day or plan: reopening the same passage resumes it.
std::string reading_key(const CorpusDb&, const Reading&);
// Old key for a passage already stated in its pre-OSB frame; migration only.
std::string legacy_reading_key(const Reading&);
// Validate and count verses in displayed order, including disjoint sections.
std::optional<ReadingPosition> reading_position(const CorpusDb&, const Reading&, std::size_t section,
                                                VerseRef);
} // namespace ortho
