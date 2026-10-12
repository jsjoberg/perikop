#pragma once
#include "core/model.hpp"
namespace ortho {
class CorpusDb;
struct OnlineBibleLink {
    std::string label, url, reference;
    // A reference search submitted by POST when the site does not support verse URLs.
    std::string post_reference = {};
};
// Input uses the reader's framing numbers. Empty URLs have no verified verse link.
std::vector<OnlineBibleLink> online_bible_links(const CorpusDb&, const std::string& frame,
                                                const std::string& book, VerseRef);
// A local browser handoff page for POST-only reference searches; empty for ordinary URLs.
std::string online_bible_form(const OnlineBibleLink&);
} // namespace ortho
