#pragma once
#include "core/model.hpp"
#include <utility>
namespace ortho {
class CorpusDb;
// Each passage uses framing coordinates; its source is the edition actually on screen.
std::string scripture_copy(const CorpusDb&, const std::string& frame,
                           const std::vector<std::pair<std::string, Passage>>& passages);
} // namespace ortho
