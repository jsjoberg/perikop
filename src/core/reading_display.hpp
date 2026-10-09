#pragma once

#include "core/model.hpp"

namespace ortho {
// Convert framing coordinates to the book and chapter numbers shown to readers.
Passage displayed_passage(const CorpusDb& corpus, Passage passage);
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages);
} // namespace ortho
