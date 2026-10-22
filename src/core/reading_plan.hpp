#pragma once
#include "core/model.hpp"
namespace ortho {
// Whole chapters of one book, numbered as the reader shows them: by the
// Septuagint for the Old Testament. The book is a CorpusDb::canon() code.
struct PlanRange {
    std::string book;
    int first, last;
};
using PlanPart = std::vector<PlanRange>;
struct ReadingPlan {
    std::string id, title, description;
    std::vector<PlanPart> parts;
};
// The New Testament, the whole Bible, and the Septuagint's other books.
const std::vector<ReadingPlan>& reading_plans();
// Versioned progress key of a part, counted from 1: "plan:nt:v3:7".
std::string plan_key(const ReadingPlan&, std::size_t part);
// A part as the plan states it, with ranges or complete short books, or with whole book names.
std::string plan_label(const CorpusDb&, const PlanPart&, bool short_names = true);
// The part as a reading in the left pane's language, with one section per range.
Reading plan_reading(const CorpusDb&, const PlanPart&, const std::string& language);
// Frozen pre-OSB coordinates, only for converting stored progress.
Reading legacy_plan_reading(const CorpusDb&, const PlanPart&, const std::string& language);
} // namespace ortho
