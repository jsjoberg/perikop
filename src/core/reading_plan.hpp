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
// The progress key of a part, counted from 1: "plan:nt:7".
std::string plan_key(const ReadingPlan&, std::size_t part);
// A part as the plan states it, such as "1 Mos 34–50 + 2 Mos 1–22".
std::string plan_label(const CorpusDb&, const PlanPart&);
// The part as a reading in the left pane's language, with one section per range.
Reading plan_reading(const CorpusDb&, const PlanPart&, const std::string& language);
} // namespace ortho
