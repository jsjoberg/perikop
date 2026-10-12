#include "core/online_bibles.hpp"
#include "storage/database.hpp"
#include <algorithm>
namespace ortho {
namespace {
struct OnlineBook {
    const char* code;
    const char* usfm;
    const char* folkbibeln;
    int number;
};
constexpr OnlineBook books[] = {
    {"Gen", "GEN", "1mos", 1},       {"Exod", "EXO", "2mos", 2},      {"Lev", "LEV", "3mos", 3},
    {"Num", "NUM", "4mos", 4},       {"Deut", "DEU", "5mos", 5},      {"Josh", "JOS", "jos", 6},
    {"Judg", "JDG", "dom", 7},       {"Ruth", "RUT", "rut", 8},       {"1Sam", "1SA", "1sam", 9},
    {"2Sam", "2SA", "2sam", 10},     {"1Kgs", "1KI", "1kung", 11},    {"2Kgs", "2KI", "2kung", 12},
    {"1Chr", "1CH", "1kron", 13},    {"2Chr", "2CH", "2kron", 14},    {"Ezra", "EZR", "esra", 15},
    {"Neh", "NEH", "neh", 16},       {"Esth", "EST", "est", 17},      {"Job", "JOB", "job", 18},
    {"Ps", "PSA", "ps", 19},         {"Prov", "PRO", "ord", 20},      {"Eccl", "ECC", "pred", 21},
    {"Song", "SNG", "hv", 22},       {"Isa", "ISA", "jes", 23},       {"Jer", "JER", "jer", 24},
    {"Lam", "LAM", "kla", 25},       {"Ezek", "EZK", "hes", 26},      {"Dan", "DAN", "dan", 27},
    {"Hos", "HOS", "hos", 28},       {"Joel", "JOL", "joel", 29},     {"Amos", "AMO", "amos", 30},
    {"Obad", "OBA", "oba", 31},      {"Jonah", "JON", "jona", 32},    {"Micah", "MIC", "mika", 33},
    {"Nah", "NAM", "nah", 34},       {"Hab", "HAB", "hab", 35},       {"Zeph", "ZEP", "sef", 36},
    {"Hag", "HAG", "hag", 37},       {"Zech", "ZEC", "sak", 38},      {"Mal", "MAL", "mal", 39},
    {"Matt", "MAT", "matt", 40},     {"Mark", "MRK", "mark", 41},     {"Luke", "LUK", "luk", 42},
    {"John", "JHN", "joh", 43},      {"Acts", "ACT", "apg", 44},      {"Rom", "ROM", "rom", 45},
    {"1Cor", "1CO", "1kor", 46},     {"2Cor", "2CO", "2kor", 47},     {"Gal", "GAL", "gal", 48},
    {"Eph", "EPH", "ef", 49},        {"Phil", "PHP", "fil", 50},      {"Col", "COL", "kol", 51},
    {"1Thess", "1TH", "1thess", 52}, {"2Thess", "2TH", "2thess", 53}, {"1Tim", "1TI", "1tim", 54},
    {"2Tim", "2TI", "2tim", 55},     {"Titus", "TIT", "tit", 56},     {"Philemon", "PHM", "filem", 57},
    {"Heb", "HEB", "heb", 58},       {"James", "JAS", "jak", 59},     {"1Peter", "1PE", "1pet", 60},
    {"2Peter", "2PE", "2pet", 61},   {"1John", "1JN", "1joh", 62},    {"2John", "2JN", "2joh", 63},
    {"3John", "3JN", "3joh", 64},    {"Jude", "JUD", "jud", 65},      {"Rev", "REV", "upp", 66},
    {"Tob", "TOB", "", 0},           {"Jdt", "JDT", "", 0},           {"Wis", "WIS", "", 0},
    {"Sir", "SIR", "", 0},           {"Baruch", "BAR", "", 0},        {"1Macc", "1MA", "", 0},
    {"2Macc", "2MA", "", 0},         {"PrMan", "MAN", "", 0},
};
std::string html_escape(const std::string& value) {
    std::string escaped;
    for (char c : value) {
        switch (c) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += c;
        }
    }
    return escaped;
}
} // namespace
std::vector<OnlineBibleLink> online_bible_links(const CorpusDb& corpus, const std::string& frame,
                                                const std::string& book, VerseRef verse) {
    std::vector<OnlineBibleLink> links{
        {"SFB15", {}, {}}, {"SKB", {}, {}}, {"SRB", {}, {}}, {"B2026", {}, {}}};
    // The word carries LXX numbers even when the left pane displays Swedish text.
    // Use the existing alignment, including book moves and split/merged verses.
    const auto targets = corpus.counterparts(frame, "sv1917", book, verse);
    if (targets.empty())
        return links;
    const auto& [target_book, target] = targets.front();
    if (target.chapter < 1 || target.verse < 1 || !corpus.verse("sv1917", target_book, target))
        return links;
    const auto entry = std::ranges::find(books, target_book, &OnlineBook::code);
    if (entry == std::end(books))
        return links;
    const auto chapter = std::to_string(target.chapter), number = std::to_string(target.verse);
    const auto reference = corpus.book_abbreviation(target_book) + " " + chapter + ":" + number;
    for (auto& link : links)
        link.reference = reference;
    if (entry->number > 0) {
        links[0].url = "https://www.folkbibeln.it/?tr=SFB15&book=" + std::string(entry->folkbibeln) +
                       "&chapter=" + chapter + "&verse=" + number;
        links[1].url = "https://www.karnbibeln.se/las/?b=" + std::to_string(entry->number) + "#" + chapter +
                       "_" + number;
        // showref ignores GET search parameters; its generated share URL also redirects.
        // Submit the same POST reference search that the site's own form uses.
        links[2].url = "https://bibelonline.se/showref.php";
        auto abbreviation = corpus.book_abbreviation(target_book);
        // Bibelonline's accepted abbreviations differ from the corpus for these books.
        if (target_book == "Song")
            abbreviation = "Hv";
        else if (target_book == "Amos")
            abbreviation = "Amos";
        else if (target_book == "Obad")
            abbreviation = "Oba";
        else if (target_book == "1Thess")
            abbreviation = "1 Thess";
        else if (target_book == "2Thess")
            abbreviation = "2 Thess";
        links[2].post_reference = abbreviation + ". " + chapter + ":" + number;
    }
    links[3].url =
        "https://www.bibeln.se/bibel/B2026/" + std::string(entry->usfm) + "." + chapter + "." + number;
    return links;
}
std::string online_bible_form(const OnlineBibleLink& link) {
    if (link.post_reference.empty() || link.url.empty())
        return {};
    return "<!doctype html><html lang=\"sv\"><meta charset=\"utf-8\">"
           "<meta name=\"referrer\" content=\"no-referrer\"><title>" +
           html_escape(link.label) + "</title><p>Öppnar " + html_escape(link.reference) + " i " +
           html_escape(link.label) + "…</p><form id=\"reference-form\" method=\"post\" action=\"" +
           html_escape(link.url) + "\"><input type=\"hidden\" name=\"search\" value=\"" +
           html_escape(link.post_reference) +
           "\"><button type=\"submit\">Öppna versen</button></form>"
           "<script>document.getElementById('reference-form').submit();</script></html>";
}
} // namespace ortho
