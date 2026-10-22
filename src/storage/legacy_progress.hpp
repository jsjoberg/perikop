#pragma once
#include "core/reading_plan.hpp"
// Pre-OSB coordinates, frozen for converting progress saved by 0.1.x. Only the plan migrations use
// them; they go together once 0.2.0 has shipped (see docs/releasing.md).
namespace ortho {
// The edition that framed a book before OSB: the Septuagint for the Old Testament.
std::string legacy_frame_source(const CorpusDb&, const std::string& language, const std::string& book);
// A plan part in those coordinates, with Nehemiah inside Greek Ezra.
Reading legacy_plan_reading(const CorpusDb&, const PlanPart&, const std::string& language);
// The progress key of a passage stated in those coordinates.
std::string legacy_reading_key(const Reading&);
} // namespace ortho
