#include "core/reading_display.hpp"
#include "storage/database.hpp"
#include <map>
namespace ortho {
std::string book_abbreviation(const std::string& book) {
    static const std::map<std::string, std::string> names = {
        {"Gen", "1 Mos"},    {"Exod", "2 Mos"},   {"Lev", "3 Mos"},     {"Num", "4 Mos"},
        {"Deut", "5 Mos"},   {"Josh", "Jos"},     {"Judg", "Dom"},      {"Ruth", "Rut"},
        {"1Sam", "1 Sam"},   {"2Sam", "2 Sam"},   {"1Kgs", "1 Kung"},   {"2Kgs", "2 Kung"},
        {"1Chr", "1 Krön"},  {"2Chr", "2 Krön"},  {"Ezra", "Esra"},     {"Neh", "Neh"},
        {"Esth", "Est"},     {"Job", "Job"},      {"Ps", "Ps"},         {"Prov", "Ords"},
        {"Eccl", "Pred"},    {"Song", "Höga v"},  {"Isa", "Jes"},       {"Jer", "Jer"},
        {"Lam", "Klag"},     {"Ezek", "Hes"},     {"Dan", "Dan"},       {"Hos", "Hos"},
        {"Joel", "Joel"},    {"Amos", "Am"},      {"Obad", "Ob"},       {"Jonah", "Jona"},
        {"Micah", "Mika"},   {"Nah", "Nah"},      {"Hab", "Hab"},       {"Zeph", "Sef"},
        {"Hag", "Hagg"},     {"Zech", "Sak"},     {"Mal", "Mal"},       {"Tob", "Tob"},
        {"Jdt", "Judit"},    {"EsthGr", "T Est"}, {"Wis", "Vish"},      {"Sir", "Syr"},
        {"Baruch", "Bar"},   {"EpJer", "Jer br"}, {"PrAzar", "Asarj"},  {"Sus", "Sus"},
        {"Bel", "Bel"},      {"DanGr", "Dan gr"}, {"1Macc", "1 Mack"},  {"2Macc", "2 Mack"},
        {"3Macc", "3 Mack"}, {"4Macc", "4 Mack"}, {"1Esd", "1 Esd"},    {"2Esd", "2 Esd"},
        {"PrMan", "Man"},    {"Ps151", "Ps 151"}, {"Matt", "Matt"},     {"Mark", "Mark"},
        {"Luke", "Luk"},     {"John", "Joh"},     {"Acts", "Apg"},      {"Rom", "Rom"},
        {"1Cor", "1 Kor"},   {"2Cor", "2 Kor"},   {"Gal", "Gal"},       {"Eph", "Ef"},
        {"Phil", "Fil"},     {"Col", "Kol"},      {"1Thess", "1 Tess"}, {"2Thess", "2 Tess"},
        {"1Tim", "1 Tim"},   {"2Tim", "2 Tim"},   {"Titus", "Tit"},     {"Philemon", "Filem"},
        {"Heb", "Hebr"},     {"James", "Jak"},    {"1Peter", "1 Petr"}, {"2Peter", "2 Petr"},
        {"1John", "1 Joh"},  {"2John", "2 Joh"},  {"3John", "3 Joh"},   {"Jude", "Jud"},
        {"Rev", "Upp"}};
    const auto it = names.find(book);
    return it == names.end() ? book : it->second;
}

Passage displayed_passage(Passage passage) {
    // A framing passage in the book and chapter numbers the reader shows.
    if (const auto* canon = canon_book(passage.book, passage.first.chapter)) {
        passage.book = canon->code;
        passage.first.chapter -= canon->offset();
        passage.last.chapter -= canon->offset();
    }
    return passage;
}
std::string passage_label(const CorpusDb& corpus, const std::vector<Passage>& passages) {
    std::string label, book;
    for (const auto& framing : passages) {
        const auto p = displayed_passage(framing);
        if (!label.empty())
            label += "; ";
        if (p.book != book) {
            label += corpus.book_name(p.book) + " ";
            book = p.book;
        }
        label += std::to_string(p.first.chapter) + ":" + std::to_string(p.first.verse) + p.first.suffix;
        if (p.last != p.first)
            label += "–" + (p.last.chapter != p.first.chapter ? std::to_string(p.last.chapter) + ":" : "") +
                     std::to_string(p.last.verse) + p.last.suffix;
    }
    return label;
}
} // namespace ortho
