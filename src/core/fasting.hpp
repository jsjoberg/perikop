#pragma once
#include "core/model.hpp"
namespace ortho {
// Orthocal's season floors and caps, after sparse day overrides are merged.
Fasting resolve_fasting(const std::vector<FeastRule>& days, int pdist, int weekday, int feast_rank,
                        int peter_and_paul, int nativity, int theophany, bool slavic);
} // namespace ortho
