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
Reading plan_reading(const CorpusDb& corpus, const PlanPart& part, const std::string& language) {
    std::vector<Passage> passages;
    bool new_testament = false;
    for (const auto& range : part) {
        const auto& canon = corpus.canon();
        const auto book = std::ranges::find(canon, range.book, &CanonBook::code);
        if (book == canon.end())
            throw std::logic_error("Okänd bok i läsplanen: " + range.book);
        std::optional<VerseRef> begin, end;
        for (const auto& ref : corpus.coordinates(osb_reference, range.book))
            if (range.first <= ref.chapter && ref.chapter <= range.last) {
                if (!begin)
                    begin = ref;
                end = ref;
            }
        if (!begin || !end)
            continue;
        passages.push_back({range.book, *begin, *end});
        new_testament = book->new_testament;
    }
    if (passages.empty())
        throw std::runtime_error("Delen har ingen text i denna utgåva.");
    Reading reading{new_testament ? ReadingKind::Gospel : ReadingKind::OldTestament,
                    passages.front(),
                    {passages.begin() + 1, passages.end()},
                    language};
    reading.reference = osb_reference;
    return reading;
}
} // namespace ortho
