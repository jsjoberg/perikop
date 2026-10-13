#pragma once

#include "core/model.hpp"

namespace ortho {
// Convert framing coordinates to the book and chapter numbers shown to readers.
Passage displayed_passage(const CorpusDb& corpus, Passage passage);
// Short labels use book abbreviations, such as "1 Kor" for Första Korintierbrevet.
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages,
                          bool short_names = false);
} // namespace ortho
