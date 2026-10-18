#pragma once
#include "core/model.hpp"

namespace ortho {
std::string encode_sources(const std::vector<SourcePreference>&);
std::vector<SourcePreference> decode_sources(const std::string&,
                                             const std::vector<SourcePreference>& defaults);
// Available editions for one chapter, in corpus order, using the established verse alignments.
std::vector<std::string> chapter_sources(const CorpusDb&, const std::string& frame, const std::string& book,
                                         int chapter);
std::string preferred_source(const std::vector<SourcePreference>&, const std::vector<std::string>& available,
                             const std::string& exclude = {});
} // namespace ortho
