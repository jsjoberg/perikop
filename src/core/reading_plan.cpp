#include "core/reading_plan.hpp"
#include "core/reading_display.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <stdexcept>
namespace ortho {
const std::vector<ReadingPlan>& reading_plans() {
    // Descriptions state what a plan contains and nothing about pace.
    // The Masoretic-canon plan keeps the Hebrew Bible's book order and splits
    // it into parts of about the same length of Septuagint text, at chapter
    // breaks and mostly at book ends. Chapters use Septuagint numbers, so
    // Esther and Daniel 3 include their Greek additions; the other books
    // outside the Hebrew Bible have their own plan.
    static const std::vector<ReadingPlan> plans = {
        {"nt",
         "Nya testamentet",
         "Nya testamentet i 30 delar.",
         {{{"Matt", 1, 9}},
          {{"Matt", 10, 18}},
          {{"Matt", 19, 28}},
          {{"Mark", 1, 9}},
          {{"Mark", 10, 16}, {"Luke", 1, 2}},
          {{"Luke", 3, 11}},
          {{"Luke", 12, 20}},
          {{"Luke", 21, 24}, {"John", 1, 5}},
          {{"John", 6, 14}},
          {{"John", 15, 21}, {"Acts", 1, 2}},
          {{"Acts", 3, 11}},
          {{"Acts", 12, 20}},
          {{"Acts", 21, 28}, {"Rom", 1, 1}},
          {{"Rom", 2, 10}},
          {{"Rom", 11, 16}, {"1Cor", 1, 3}},
          {{"1Cor", 4, 12}},
          {{"1Cor", 13, 16}, {"2Cor", 1, 5}},
          {{"2Cor", 6, 13}, {"Gal", 1, 1}},
          {{"Gal", 2, 6}, {"Eph", 1, 4}},
          {{"Eph", 5, 6}, {"Phil", 1, 4}, {"Col", 1, 3}},
          {{"Col", 4, 4}, {"1Thess", 1, 5}, {"2Thess", 1, 3}},
          {{"1Tim", 1, 6}, {"2Tim", 1, 3}},
          {{"2Tim", 4, 4}, {"Titus", 1, 3}, {"Philemon", 1, 1}, {"Heb", 1, 4}},
          {{"Heb", 5, 13}},
          {{"James", 1, 5}, {"1Peter", 1, 4}},
          {{"1Peter", 5, 5}, {"2Peter", 1, 3}, {"1John", 1, 5}},
          {{"2John", 1, 1}, {"3John", 1, 1}, {"Jude", 1, 1}, {"Rev", 1, 6}},
          {{"Rev", 7, 12}},
          {{"Rev", 13, 17}},
          {{"Rev", 18, 22}}}},
        {"ot",
         "Masoretisk kanon",
         "Gamla testamentets böcker i den hebreiska bibeln, i 30 delar. Kapitlen följer Septuagintas "
         "numrering, och Ester och Daniel har sina grekiska tillägg.",
         {{{"Gen", 1, 27}},
          {{"Gen", 28, 50}},
          {{"Exod", 1, 24}},
          {{"Exod", 25, 40}, {"Lev", 1, 7}},
          {{"Lev", 8, 27}},
          {{"Num", 1, 21}},
          {{"Num", 22, 36}, {"Deut", 1, 8}},
          {{"Deut", 9, 34}},
          {{"Josh", 1, 24}},
          {{"Judg", 1, 21}, {"Ruth", 1, 4}},
          {{"1Sam", 1, 31}},
          {{"2Sam", 1, 24}},
          {{"1Kgs", 1, 22}},
          {{"2Kgs", 1, 25}},
          {{"1Chr", 1, 29}},
          {{"2Chr", 1, 25}},
          {{"2Chr", 26, 36}, {"Ezra", 1, 10}},
          {{"Neh", 1, 13}, {"Esth", 1, 10}},
          {{"Job", 1, 42}},
          {{"Ps", 1, 75}},
          {{"Ps", 76, 150}},
          {{"Prov", 1, 31}, {"Eccl", 1, 12}},
          {{"Song", 1, 8}, {"Isa", 1, 31}},
          {{"Isa", 32, 66}},
          {{"Jer", 1, 27}},
          {{"Jer", 28, 52}},
          {{"Lam", 1, 5}, {"Ezek", 1, 23}},
          {{"Ezek", 24, 48}},
          {{"Dan", 1, 12}, {"Hos", 1, 14}, {"Joel", 1, 4}},
          {{"Amos", 1, 9},
           {"Obad", 1, 1},
           {"Jonah", 1, 4},
           {"Micah", 1, 7},
           {"Nah", 1, 3},
           {"Hab", 1, 3},
           {"Zeph", 1, 3},
           {"Hag", 1, 2},
           {"Zech", 1, 14},
           {"Mal", 1, 3}}}},
        {"lxx",
         "Fler böcker ur Septuaginta",
         "De böcker utanför den hebreiska bibeln som Perikop har på svenska, i 14 delar. Första "
         "Esdrasboken, Tredje Mackabeerboken och Psalm 151 finns inte med, eftersom Perikop saknar svensk "
         "text för dem.",
         {{{"PrMan", 1, 1}, {"Tob", 1, 7}},
          {{"Tob", 8, 14}, {"Jdt", 1, 3}},
          {{"Jdt", 4, 12}},
          {{"Jdt", 13, 16}, {"Wis", 1, 6}},
          {{"Wis", 7, 15}},
          {{"Wis", 16, 19}, {"Sir", 1, 6}},
          {{"Sir", 7, 16}},
          {{"Sir", 17, 26}},
          {{"Sir", 27, 36}},
          {{"Sir", 37, 46}},
          {{"Sir", 47, 51}, {"Baruch", 1, 5}},
          {{"EpJer", 1, 1}, {"Sus", 1, 1}, {"Bel", 1, 1}, {"1Macc", 1, 5}},
          {{"1Macc", 6, 16}},
          {{"2Macc", 1, 15}}}},
    };
    return plans;
}
std::string plan_key(const ReadingPlan& plan, std::size_t part) {
    return "plan:" + plan.id + ":" + std::to_string(part + 1);
}
std::string plan_label(const PlanPart& part) {
    // Books of one chapter are named without a chapter number, as in "Filem".
    static const std::vector<std::string> single = {"Obad",  "Philemon", "2John", "3John", "Jude",
                                                    "PrMan", "EpJer",    "Sus",   "Bel"};
    std::string label;
    for (const auto& range : part) {
        if (!label.empty())
            label += " + ";
        label += book_abbreviation(range.book);
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
    std::string reference;
    bool new_testament = false;
    for (const auto& range : part) {
        const auto& canon = osb_canon();
        const auto book = std::find_if(canon.begin(), canon.end(), [&](const CanonBook& candidate) {
            return candidate.code == range.book;
        });
        if (book == canon.end())
            throw std::logic_error("Okänd bok i läsplanen: " + range.book);
        const auto frame = frame_source(language, book->frame_book);
        const int first = range.first + book->offset(), last = range.last + book->offset();
        std::optional<VerseRef> begin, end;
        for (const auto& ref : corpus.coordinates(frame, book->frame_book))
            if (first <= ref.chapter && ref.chapter <= last) {
                if (!begin)
                    begin = ref;
                end = ref;
            }
        if (!begin)
            continue;
        passages.push_back({book->frame_book, *begin, *end});
        reference = frame;
        new_testament = book->new_testament;
    }
    if (passages.empty())
        throw std::runtime_error("Delen har ingen text i denna utgåva.");
    Reading reading{new_testament ? ReadingKind::Gospel : ReadingKind::OldTestament,
                    passages.front(),
                    passage_label(corpus, passages),
                    {passages.begin() + 1, passages.end()},
                    language};
    reading.reference = reference;
    return reading;
}
} // namespace ortho
