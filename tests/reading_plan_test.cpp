#include "core/reading_plan.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>

using namespace ortho;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void layout_test(const CorpusDb& corpus) {
    const auto& plans = reading_plans();
    check(plans.size() == 3 && plans[0].parts.size() == 60 && plans[1].parts.size() == 182 &&
              plans[2].parts.size() == 36,
          "NT has 60 parts; other plans choose comparable reading lengths");
    const std::set<std::string> separate{"Matt", "Mark", "Luke", "John", "Acts", "Rev"};
    const std::set<std::string> long_letters{"Rom", "1Cor", "2Cor", "Heb"};
    std::map<std::string, int> appearances;
    std::vector<double> means;
    for (const auto& plan : plans) {
        std::vector<int> lengths;
        for (const auto& part : plan.parts) {
            for (const auto& range : part) {
                ++appearances[range.book];
                if (separate.contains(range.book))
                    check(part.size() == 1, "Gospels, Acts, and Revelation never share a part");
                if (part.size() > 1) {
                    const auto book = std::ranges::find(corpus.canon(), range.book, &CanonBook::code);
                    check(book != corpus.canon().end(), "Every planned book belongs to the canon");
                    int last = 0;
                    for (const auto& verse : corpus.coordinates(osb_reference, book->code))
                        if (verse.chapter <= book->last_chapter)
                            last = std::max(last, verse.chapter);
                    check(range.first == 1 && range.last == last,
                          "A mixed part contains only complete books, never the start of another book");
                }
            }
            int words = 0;
            std::optional<std::pair<std::string, std::vector<std::pair<std::string, VerseRef>>>> previous;
            const auto reading = plan_reading(corpus, part, "sv");
            for (const auto& passage : reading.segments())
                for (const auto& ref : corpus.coordinates(osb_reference, passage.book)) {
                    if (!corpus.contains(osb_reference, passage, ref))
                        continue;
                    auto source = std::string("sv1917");
                    auto targets = corpus.counterparts(osb_reference, source, passage.book, ref);
                    if (targets.empty()) {
                        source = corpus.new_testament_book(passage.book) ? "en-web" : "en-lxx2012";
                        targets = corpus.counterparts(osb_reference, source, passage.book, ref);
                    }
                    const auto unit = std::pair{source, targets};
                    if (previous && *previous == unit)
                        continue;
                    previous = unit;
                    for (const auto& [book, target] : targets)
                        if (const auto verse = corpus.verse(source, book, target)) {
                            std::istringstream text(verse->text);
                            for (std::string word; text >> word;)
                                ++words;
                        }
                }
            check(words > 0 && words < 5'200, "Every part has text and a bounded reading length");
            check(words >= 1'200, "OSB parts have comparable reading lengths");
            lengths.push_back(words);
        }
        const auto mean = double(std::accumulate(lengths.begin(), lengths.end(), 0)) / lengths.size();
        means.push_back(mean);
        std::cout << plan.id << ": " << lengths.size() << " parts, " << mean << " mean words\n";
    }
    check(std::abs(means[1] / means[0] - 1) < 0.15 && std::abs(means[2] / means[0] - 1) < 0.15,
          "The three plans have similar average reading lengths");
    for (const auto& range : plans[0].parts)
        for (const auto& book : range)
            if (!separate.contains(book.book) && !long_letters.contains(book.book))
                check(appearances[book.book] == 1, "Short NT letters remain whole");
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 2, "Expected the corpus");
        CorpusDb corpus(argv[1]);
        layout_test(corpus);
        std::cout << "OSB plan layout passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
