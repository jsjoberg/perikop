#pragma once

#include "core/model.hpp"

namespace ortho {
// Convert framing coordinates to the book and chapter numbers shown to readers.
Passage displayed_passage(Passage passage);
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages);
// Short Swedish name of a book, such as "1 Mos", for grids and plan parts.
std::string book_abbreviation(const std::string& book);
} // namespace ortho
