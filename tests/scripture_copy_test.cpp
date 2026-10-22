#include "core/scripture_copy.hpp"
#include "storage/database.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char** argv) {
    using namespace ortho;
    try {
        check(argc == 2, "Expected corpus path");
        CorpusDb corpus(argv[1]);
        const auto quote = [&](const std::string& source, Passage passage) {
            return scripture_copy(corpus, osb_reference, {{source, passage}});
        };
        const auto psalm1 = corpus.verse("sv1917", "Ps", {23, 1})->text;
        const auto psalm2 = corpus.verse("sv1917", "Ps", {23, 2})->text;
        const auto psalm4 = corpus.verse("sv1917", "Ps", {23, 4})->text;
        check(quote("sv1917", {"Ps", {22, 1}, {22, 2}}) ==
                  psalm1 + " " + psalm2 + "\n\n— Psaltaren 23:1–2 (Svenska 1917 med apokryfer)",
              "Swedish Psalm copy cites its own numbering, with the exact unhyphenated text");
        check(scripture_copy(corpus, osb_reference,
                             {{"sv1917", {"Ps", {22, 1}, {22, 2}}}, {"sv1917", {"Ps", {22, 4}, {22, 4}}}}) ==
                  psalm1 + " " + psalm2 + "\n\n" + psalm4 +
                      "\n\n— Psaltaren 23:1–2; 23:4 (Svenska 1917 med apokryfer)",
              "Disjoint ranges retain their gaps and share an exact citation");
        const auto merged = corpus.verse("sv1917", "2Cor", {13, 12})->text;
        check(quote("sv1917", {"2Cor", {13, 12}, {13, 13}}) ==
                  merged + "\n\n— Andra Korintierbrevet 13:12 (Svenska 1917 med apokryfer)",
              "Merged source verses are copied once");
        check(quote("sv1917", {"2Cor", {13, 13}, {13, 13}}).starts_with(merged),
              "A range beginning inside a merged verse includes its source text");
        check(quote("sv1917", {"2Chr", {37, 1}, {37, 1}}).ends_with("\n\n— Manasses bön 1:1 (Svenska 1921)"),
              "Moved apocrypha cite the translated book and the 1921 edition");
        const auto greek = corpus.verse("grc-patriarchal", "John", {1, 1})->text;
        check(quote("grc-patriarchal", {"John", {1, 1}, {1, 1}}) ==
                  greek + "\n\n— Johannesevangeliet 1:1 (Patriarkal grekiska 1904)",
              "Greek Unicode and edition attribution survive copy");
        const auto english = corpus.verse("en-web", "John", {1, 1})->text;
        const auto mixed = scripture_copy(
            corpus, osb_reference,
            {{"grc-patriarchal", {"John", {1, 1}, {1, 1}}}, {"en-web", {"John", {1, 1}, {1, 1}}}});
        check(mixed.starts_with(greek) && mixed.find(english) != std::string::npos &&
                  mixed.ends_with("(World English Bible Classic)"),
              "Different shown editions receive separate citations");
        check(quote("grc-lxx", {"John", {1, 1}, {1, 1}}).empty() &&
                  scripture_copy(corpus, osb_reference, {}).empty(),
              "Missing text and empty selections never copy placeholders or invented references");
        check(scripture_copy(corpus, "sv1917", {{"sv1917", {"Ps", {23, 1}, {23, 1}}}}).starts_with(psalm1),
              "An edition can also serve as the frame");
        std::cout << "Scripture copy: ranges, numbering, merged verses, editions and Unicode passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
