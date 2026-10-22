#include "core/reading_plan.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <stdexcept>
namespace ortho {
const std::vector<ReadingPlan>& reading_plans() {
    // Fixed ranges keep progress stable across editions and application updates.
    static const std::vector<ReadingPlan> plans{
#include "core/reading_plan_parts.inc"
    };
    return plans;
}
std::string plan_key(const ReadingPlan& plan, std::size_t part) {
    return "plan:" + plan.id + ":v3:" + std::to_string(part + 1);
}
std::string plan_label(const CorpusDb& corpus, const PlanPart& part, bool short_names) {
    // Books of one chapter are named without a chapter number, as in "Filem".
    static const std::vector<std::string> single = {"Obad",  "Philemon", "2John", "3John", "Jude",
                                                    "PrMan", "EpJer",    "Sus",   "Bel"};
    std::string label;
    for (const auto& range : part) {
        if (!label.empty())
            label += " + ";
        label += short_names ? corpus.short_title(range.book) : corpus.title(range.book);
        if (std::find(single.begin(), single.end(), range.book) != single.end())
            continue;
        label += " " + std::to_string(range.first);
        if (range.last != range.first)
            label += "–" + std::to_string(range.last);
    }
    return label;
}
namespace {
Reading make_plan_reading(const CorpusDb& corpus, const PlanPart& part, const std::string& language,
                          bool legacy) {
    std::vector<Passage> passages;
    std::string reference;
    bool new_testament = false;
    for (const auto& range : part) {
        const auto& canon = legacy ? corpus.legacy_canon() : corpus.canon();
        const auto book = std::find_if(canon.begin(), canon.end(), [&](const CanonBook& candidate) {
            return candidate.code == range.book;
        });
        if (book == canon.end())
            throw std::logic_error("Okänd bok i läsplanen: " + range.book);
        const std::string frame =
            legacy ? corpus.legacy_frame_source(language, book->frame_book) : osb_reference;
        const int first = range.first + book->offset(), last = range.last + book->offset();
        std::optional<VerseRef> begin, end;
        for (const auto& ref : corpus.coordinates(frame, book->frame_book))
            if (first <= ref.chapter && ref.chapter <= last) {
                if (!begin)
                    begin = ref;
                end = ref;
            }
        if (!begin || !end)
            continue;
        passages.push_back({book->frame_book, *begin, *end});
        reference = frame;
        new_testament = book->new_testament;
    }
    if (passages.empty())
        throw std::runtime_error("Delen har ingen text i denna utgåva.");
    Reading reading{new_testament ? ReadingKind::Gospel : ReadingKind::OldTestament,
                    passages.front(),
                    {passages.begin() + 1, passages.end()},
                    language};
    reading.reference = reference;
    return reading;
}
} // namespace
Reading plan_reading(const CorpusDb& corpus, const PlanPart& part, const std::string& language) {
    return make_plan_reading(corpus, part, language, false);
}
Reading legacy_plan_reading(const CorpusDb& corpus, const PlanPart& part, const std::string& language) {
    return make_plan_reading(corpus, part, language, true);
}
} // namespace ortho
