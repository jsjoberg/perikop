#include "core/model.hpp"
#include "core/online_bibles.hpp"
#include "core/reading_plan.hpp"
#include "core/source_preferences.hpp"
#include "speech/reading_speech.hpp"
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace {
int checks = 0;
void check(bool value, const std::string& message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
ortho::CivilDate date(const char* value) {
    return ortho::parse_date(value).value();
}
} // namespace
int main(int argc, char** argv) {
    try {
        using namespace ortho;
        if (argc != 5)
            throw std::runtime_error("Usage: ortho-tests CORPUS USER CHART OCA");
        check(shift_date(date("2024-02-28"), 1) == date("2024-02-29"), "leap day");
        check(shift_date(date("2026-12-31"), 1) == date("2027-01-01"), "year navigation");
        check(shift_date(date("2026-03-01"), -1) == date("2026-02-28"), "backwards navigation");
        check(!parse_date("2026-02-29"), "invalid leap date");
        check(!parse_date("2026-257-01") && !parse_date("2026-01-257"), "overflow date parts");
        check(!parse_date("2026-10-05abc"), "trailing date text");
        check(!parse_date("99999999999-01-01"), "year outside int");
        check(date_swedish(date("2026-10-05")) == "Måndag 5 oktober 2026", "Swedish civil date");
        SelectedDay selected(date("2026-10-05"));
        // Simulate a clock change and wake: no update method consults this clock.
        auto simulated_now = date("2026-10-06");
        check(selected.date() != simulated_now && selected.date() == date("2026-10-05"),
              "midnight changes selected date");
        selected.move(1);
        check(selected.date() == simulated_now, "explicit date move");
        selected.select(date("2026-10-07"));
        check(selected.date() == date("2026-10-07"), "explicit selection");
        SelectedDay first_day(date("2019-01-01"));
        bool rejected = false;
        try {
            first_day.move(-1);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && first_day.date() == date("2019-01-01"), "calendar rejects dates before 2019");
        check(!selectable_date(date("2018-12-31")) && selectable_date(date("2019-01-01")),
              "calendar selection bounds");
        CorpusDb corpus(argv[1]);
        {
            const auto john = online_bible_links(corpus, "sv1917", "John", {3, 16});
            check(john.size() == 4 &&
                      john[0].url.find("tr=SFB15&book=joh&chapter=3&verse=16") != std::string::npos &&
                      john[1].url.ends_with("?b=43#3_16") && john[3].url.ends_with("/B2026/JHN.3.16"),
                  "online Bible sources open the selected verse in the requested editions");
            check(john[2].post_reference == "Joh. 3:16" &&
                      john[2].url == "https://bibelonline.se/showref.php",
                  "Reformationsbibeln submits the selected verse to Bibelonline's reference search");
            const auto form = online_bible_form(john[2]);
            check(form.find("method=\"post\"") != std::string::npos &&
                      form.find("action=\"https://bibelonline.se/showref.php\"") != std::string::npos &&
                      form.find("name=\"search\" value=\"Joh. 3:16\"") != std::string::npos &&
                      form.find(".submit();") != std::string::npos && online_bible_form(john[1]).empty(),
                  "POST handoff preserves the exact verse and ordinary links stay direct");
            check(online_bible_form({"<label>", "https://example.test/?a=1&b=2", "test", "\"<&"})
                          .find("value=\"&quot;&lt;&amp;\"") != std::string::npos,
                  "POST form attributes escape reference text");
            const auto psalm = online_bible_links(corpus, "grc-lxx", "Ps", {22, 1});
            check(psalm[0].url.ends_with("book=ps&chapter=23&verse=1") && psalm[3].url.ends_with("/PSA.23.1"),
                  "online Psalm links use the Swedish counterpart, not the reader's LXX numbers");
            check(psalm[1].url.ends_with("?b=19#23_1") && psalm[2].post_reference == "Ps. 23:1",
                  "Kärnbibeln anchors and Reformationsbibeln searches use the mapped Psalm verse");
            check(online_bible_links(corpus, "sv1917", "Song", {1, 1})[2].post_reference == "Hv. 1:1" &&
                      online_bible_links(corpus, "sv1917", "1Thess", {1, 1})[2].post_reference ==
                          "1 Thess. 1:1",
                  "Bibelonline reference searches use its accepted book abbreviations");
            const auto nehemiah = online_bible_links(corpus, "grc-lxx", "Ezra", {11, 1});
            check(nehemiah[0].url.ends_with("book=neh&chapter=1&verse=1"),
                  "online links map framing books and chapter offsets to Swedish books");
            const auto tobit = online_bible_links(corpus, "sv1917", "Tob", {1, 1});
            check(tobit[0].url.empty() && tobit[1].url.empty() && tobit[2].url.empty() &&
                      tobit[3].url.ends_with("/TOB.1.1"),
                  "deuterocanonical sources only enable supported online editions");
            for (const auto& [book, verse] :
                 std::array{std::pair{"Ps", VerseRef{151, 1}}, std::pair{"Unknown", VerseRef{1, 1}},
                            std::pair{"John", VerseRef{3, 999}}}) {
                const auto links = online_bible_links(corpus, "grc-lxx", book, verse);
                check(std::ranges::all_of(links,
                                          [](const auto& link) {
                                              return link.url.empty();
                                          }),
                      "unmapped or missing verses never produce misleading online URLs");
            }
        }
        const auto paragraphs = corpus.paragraph_starts("sv1917", "John");
        check(std::binary_search(paragraphs.begin(), paragraphs.end(), VerseRef{20, 20}),
              "WEB editorial resurrection paragraph");
        check(!std::binary_search(paragraphs.begin(), paragraphs.end(), VerseRef{20, 21}),
              "verse must not imply paragraph break");
        check(!std::binary_search(paragraphs.begin(), paragraphs.end(), VerseRef{20, 14}),
              "internal prose break must not migrate to next verse");
        const auto greek_paragraphs = corpus.paragraph_starts("grc-lxx", "Gen");
        check(std::binary_search(greek_paragraphs.begin(), greek_paragraphs.end(), VerseRef{1, 6}),
              "original USFM Greek paragraph boundary");
        Lectionary lectionary(corpus);
        check(lectionary.readings_with_variants(date("2026-10-14"), CalendarStyle::New, Tradition::Greek)
                      .day.cycle_title == "Onsdag i 20:e veckan efter pingst",
              "Reading cycle day and week heading is retained");
        check(lectionary.readings_with_variants(date("2026-03-25"), CalendarStyle::New, Tradition::Greek)
                      .day.cycle_title == "Onsdag i stora fastans 5:e vecka",
              "Reading cycle heading excludes the coinciding fixed feast");
        for (const auto& rule : corpus.feast_rules())
            if (!rule.cycle_title.empty() && !swedish_title(rule.cycle_title))
                throw std::runtime_error("Missing Swedish cycle title: " + rule.cycle_title);
        {
            // The start page shows each reading's occasion; "Composite" is hidden there.
            sqlite3* corpus_db = nullptr;
            check(sqlite3_open_v2(argv[1], &corpus_db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
                  "open corpus for occasions");
            sqlite3_stmt* occasions = nullptr;
            check(sqlite3_prepare_v2(corpus_db,
                                     "SELECT DISTINCT description FROM reading_rule "
                                     "WHERE description<>'' AND description<>'Composite'",
                                     -1, &occasions, nullptr) == SQLITE_OK,
                  "prepare occasion query");
            std::string missing;
            while (sqlite3_step(occasions) == SQLITE_ROW) {
                const std::string occasion = reinterpret_cast<const char*>(sqlite3_column_text(occasions, 0));
                if (!swedish_title(occasion))
                    missing += "\n  " + occasion;
            }
            sqlite3_finalize(occasions);
            sqlite3_close(corpus_db);
            check(missing.empty(), "Missing Swedish occasion titles:" + missing);
        }
        check(swedish_title("Tuesday of the 19th week after Pentecost") ==
                  "Tisdag i 19:e veckan efter pingst",
              "Swedish weekday title");
        check(swedish_title("21st Sunday after Pentecost") == "21:a söndagen efter pingst" &&
                  swedish_title("11th Sunday after Pentecost") == "11:e söndagen efter pingst",
              "Swedish ordinals");
        check(swedish_title("Sunday before Nativity – Eve of Nativity") ==
                  "Söndagen före Kristi födelse; Julafton",
              "Coinciding Swedish titles");
        const auto today =
            lectionary.readings_for(date("2026-10-05"), CalendarStyle::New, Tradition::Antiochian);
        check(today.readings.size() == 2, "Antiochian daily pair");
        check(today.readings[0].passage.book == "Phil" && today.readings[0].passage.first == VerseRef{1, 1} &&
                  today.readings[0].passage.last == VerseRef{1, 7},
              "official Oct 5 epistle");
        check(today.readings[1].passage.book == "Luke" &&
                  today.readings[1].passage.first == VerseRef{6, 24} &&
                  today.readings[1].passage.last == VerseRef{6, 30},
              "official Oct 5 gospel");
        auto old = lectionary.readings_for(date("2026-10-06"), CalendarStyle::Old, Tradition::Antiochian);
        auto modern = lectionary.readings_for(date("2026-10-06"), CalendarStyle::New, Tradition::Antiochian);
        check(old.day.civil_date == modern.day.civil_date, "calendar shifts civil date");
        check(old.day.fixed_cycle == "2026-09-23" && modern.day.fixed_cycle == "2026-10-06",
              "independent fixed cycle");
        check(old.day.fixed_cycle && old.day.paschal_cycle, "computed calendar cycles");
        check(!lectionary.readings_for(date("2030-01-01"), CalendarStyle::New, Tradition::Antiochian)
                   .readings.empty(),
              "future recurring readings");
        check(orthodox_pascha(2026) == date("2026-04-12") && orthodox_pascha(2027) == date("2027-05-02"),
              "official Pascha dates");
        check(fixed_calendar_date(date("2100-03-14"), CalendarStyle::Old) == std::chrono::year{2100} / 2 / 29,
              "Julian century leap label");
        // Independent Serbian Church witnesses: published facts, not runtime lookups.
        check(fixed_calendar_date(date("2026-01-07"), CalendarStyle::Old) == date("2025-12-25"),
              "Serbian Nativity fixed date");
        check(fixed_calendar_date(date("2026-01-19"), CalendarStyle::Old) == date("2026-01-06"),
              "Serbian Theophany fixed date");
        check(fixed_calendar_date(date("2026-09-27"), CalendarStyle::Old) == date("2026-09-14"),
              "Serbian Elevation fixed date");
        const auto serbian_sunday =
            lectionary.readings_for(date("2026-01-11"), CalendarStyle::Old, Tradition::Antiochian);
        check(serbian_sunday.readings[1].passage.book == "Matt" &&
                  serbian_sunday.readings[1].passage.first == VerseRef{2, 13} &&
                  serbian_sunday.readings[1].passage.last == VerseRef{2, 23},
              "Serbian Sunday after Nativity Gospel");
        const auto serbian_pascha =
            lectionary.readings_for(date("2026-04-12"), CalendarStyle::Old, Tradition::Antiochian);
        check(serbian_pascha.readings[1].passage.book == "John" &&
                  serbian_pascha.readings[1].passage.first == VerseRef{1, 1} &&
                  serbian_pascha.readings[1].passage.last == VerseRef{1, 17},
              "Serbian published Pascha Gospel");
        check(fixed_calendar_date(date("2101-01-08"), CalendarStyle::Old) == date("2100-12-25"),
              "Julian conversion must not assume thirteen days forever");
        check(fixed_calendar_date(date("2800-02-29"), CalendarStyle::New) == date("2800-03-01"),
              "New calendar must use Revised Julian leap rules");
        check(fixed_calendar_date(date("2900-02-28"), CalendarStyle::New) == std::chrono::year{2900} / 2 / 29,
              "Revised Julian century leap label");
        check(fixed_calendar_date(date("2900-03-01"), CalendarStyle::New) == date("2900-03-01"),
              "Revised Julian leap difference returns to zero");
        for (int year : {1, 2036, 2100, 2400, 2800, 5000, 9999}) {
            const auto start = std::chrono::year{year} / 1 / 1;
            const int length = std::chrono::year{year}.is_leap() ? 366 : 365;
            for (int i = 0; i < length; ++i)
                for (auto style : {CalendarStyle::New, CalendarStyle::Old})
                    for (auto tradition : {Tradition::Antiochian, Tradition::Greek, Tradition::Slavic}) {
                        const auto civil = shift_date(start, i);
                        const auto computed = lectionary.readings_for(civil, style, tradition);
                        check(computed.day.civil_date == civil && !computed.readings.empty(),
                              "calculated calendar horizon contains a gap");
                    }
        }
        // The Julian computus repeats over 532 years; civil dates do not.
        for (int year = 2000; year < 2532; ++year) {
            const auto a = fixed_calendar_date(orthodox_pascha(year), CalendarStyle::Old);
            const auto b = fixed_calendar_date(orthodox_pascha(year + 532), CalendarStyle::Old);
            check(a.month() == b.month() && a.day() == b.day(), "Julian 532-year Paschal cycle");
            check(std::chrono::weekday{std::chrono::sys_days{orthodox_pascha(year)}} == std::chrono::Sunday,
                  "computed Pascha is not Sunday");
        }
        // Independently checked citations from each official annual Sunday chart.
        const auto published = [&](Reading reading) {
            reading.base_language = "en";
            return corpus.localize(std::move(reading));
        };
        const auto chart_dir = std::filesystem::path(argv[3]).parent_path();
        for (int chart_year = first_calendar_year; chart_year <= 2026; ++chart_year) {
            std::ifstream chart(chart_dir / ("antiochian-" + std::to_string(chart_year) + ".tsv"));
            check(bool(chart), "official chart fixture missing");
            std::string line;
            int sundays = 0;
            const CivilDate january = std::chrono::year{chart_year} / std::chrono::January / 1;
            const int weekday = int(std::chrono::weekday{std::chrono::sys_days{january}}.c_encoding());
            auto expected_date = shift_date(january, (7 - weekday) % 7);
            while (std::getline(chart, line)) {
                std::istringstream row(line);
                std::string iso, expected;
                std::getline(row, iso, '\t');
                const auto civil = parse_date(iso).value();
                check(civil == expected_date, "Sunday chart must cover every date in order");
                expected_date = shift_date(expected_date, 7);
                const auto result = lectionary.readings_for(civil, CalendarStyle::New, Tradition::Antiochian);
                auto services =
                    lectionary.service_readings_for(civil, CalendarStyle::New, Tradition::Antiochian);
                // Composite Vespers name the source of their verses, not a chart.
                std::erase_if(services.readings, [](const auto& r) {
                    return r.assignment_source_url.empty() || r.kind == ReadingKind::Vespers;
                });
                check(result.day.annual && services.day.annual, "published chart coverage");
                check(result.readings.size() == 2 && services.readings.size() == 2,
                      "official Sunday Liturgy pair");
                for (std::size_t i = 0; i < result.readings.size(); ++i) {
                    check(bool(std::getline(row, expected, '\t')), "extra computed Sunday reading");
                    for (const Reading* reading :
                         std::initializer_list<const Reading*>{&result.readings[i], &services.readings[i]}) {
                        check(reading->assignment_source_url ==
                                  "https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/"
                                  "Liturgical%20Chart%20for%20" +
                                      std::to_string(chart_year) + "%20English.pdf",
                              "official chart source on each assigned reading");
                        std::string actual = std::to_string(int(reading->kind)) + "=";
                        for (const auto& p : published(*reading).segments()) {
                            if (actual.back() != '=')
                                actual += '|';
                            actual += p.book + "_" + std::to_string(p.first.chapter * 1000 + p.first.verse) +
                                      "_" + std::to_string(p.last.chapter * 1000 + p.last.verse);
                        }
                        if (actual != expected)
                            throw std::runtime_error("Official chart mismatch " + iso + ": " + actual +
                                                     " != " + expected);
                        ++checks;
                    }
                }
                check(!std::getline(row, expected, '\t'), "missing Sunday reading");
                ++sundays;
            }
            check((sundays == 52 || sundays == 53) && int(expected_date.year()) == chart_year + 1,
                  "incomplete Sunday audit");
        }
        check(lectionary.readings_with_variants(date("2018-05-06"), CalendarStyle::New, Tradition::Greek)
                  .variants.empty(),
              "no Antiochian alternatives before 2019");
        check(!lectionary.readings_for(date("2027-06-27"), CalendarStyle::New, Tradition::Antiochian)
                   .day.annual,
              "unpublished future Sunday uses calculated readings");
        check(!lectionary.readings_for(date("2026-06-14"), CalendarStyle::Old, Tradition::Antiochian)
                   .day.annual,
              "New Calendar chart does not override Old Calendar");
        // The Antiochian variants and the jurisdictions' annual assignments apply only to their reading
        // order.
        const auto first_reading = [&](const char* iso, Tradition tradition, std::size_t index = 0) {
            const auto readings = lectionary.readings_for(date(iso), CalendarStyle::New, tradition).readings;
            check(index < readings.size(), "missing reading");
            return readings[index].passage;
        };
        check(first_reading("2026-05-10", Tradition::Antiochian).last == VerseRef{11, 30} &&
                  first_reading("2026-05-10", Tradition::Greek).last == VerseRef{11, 26},
              "Antiochian Samaritan Sunday Epistle only in its own order");
        check(first_reading("2026-06-14", Tradition::Antiochian).book == "Acts" &&
                  first_reading("2026-06-14", Tradition::Greek).book == "Rom" &&
                  first_reading("2026-06-14", Tradition::Slavic).book == "Rom",
              "All Saints of Antioch only in the Antiochian order");
        check(first_reading("2026-01-24", Tradition::Antiochian, 1).first !=
                  first_reading("2026-01-24", Tradition::Greek, 1).first,
              "each jurisdiction's own annual assignment");
        check(lectionary.readings_for(date("2027-01-26"), CalendarStyle::New, Tradition::Greek).day.annual &&
                  !lectionary.readings_for(date("2027-01-26"), CalendarStyle::New, Tradition::Slavic)
                       .day.annual,
              "Greek 2027 annual assignment");
        const auto family =
            lectionary.readings_with_variants(date("2026-05-10"), CalendarStyle::New, Tradition::Greek);
        check(family.variants.size() == 1 && family.variants[0].primary_index && family.variants[0].reading &&
                  family.readings[*family.variants[0].primary_index].passage.last == VerseRef{11, 26} &&
                  family.variants[0].reading->passage.last == VerseRef{11, 30} &&
                  family.variants[0].explanation.find("versavgränsning") != std::string::npos,
              "Greek family includes the Antiochian range difference without duplicating the Gospel");
        const auto local_feast =
            lectionary.readings_with_variants(date("2026-06-14"), CalendarStyle::New, Tradition::Greek);
        check(local_feast.variants.size() == 1 && local_feast.variants[0].primary_index &&
                  local_feast.variants[0].reading->passage.book == "Acts" &&
                  local_feast.variants[0].explanation.find("Alla Antiokias helgon") != std::string::npos,
              "Greek family explains the Antiochian local feast");
        for (auto style : {CalendarStyle::New, CalendarStyle::Old}) {
            check(lectionary.readings_with_variants(date("2026-04-12"), style, Tradition::Greek)
                      .variants.empty(),
                  "shared Pascha readings appear once in either calendar");
            check(lectionary.readings_with_variants(date("2026-06-14"), style, Tradition::Slavic)
                      .variants.empty(),
                  "Slavic family does not invent unsupported jurisdiction variants");
        }
        const auto annual_variants =
            lectionary.readings_with_variants(date("2026-01-24"), CalendarStyle::New, Tradition::Greek);
        check(std::ranges::any_of(annual_variants.variants,
                                  [](const auto& variant) {
                                      return variant.reading && variant.reading->kind == ReadingKind::Gospel;
                                  }),
              "Greek family includes differing annual Gospel assignments");
        for (const auto* iso : {"2026-01-11", "2026-04-12"}) {
            const auto greek = lectionary.readings_for(date(iso), CalendarStyle::Old, Tradition::Antiochian);
            const auto slavic = lectionary.readings_for(date(iso), CalendarStyle::Old, Tradition::Slavic);
            check(slavic.readings.size() == 2 &&
                      slavic.readings[1].passage.first == greek.readings[1].passage.first,
                  "Serbian published Gospel in the Slavic order");
        }
        const auto friday =
            lectionary.service_readings_for(date("2026-10-09"), CalendarStyle::New, Tradition::Slavic);
        check(friday.readings.size() == 8 &&
                  std::ranges::count(friday.readings, ReadingKind::Vespers, &Reading::kind) == 3 &&
                  std::ranges::count(friday.readings, ReadingKind::Matins, &Reading::kind) == 1 &&
                  std::ranges::count(friday.readings, ReadingKind::Epistle, &Reading::kind) == 2 &&
                  std::ranges::count(friday.readings, ReadingKind::Gospel, &Reading::kind) == 2,
              "October 9 contains Vespers, Matins, daily readings, and the saint's readings");
        for (auto order : {Tradition::Greek, Tradition::Slavic}) {
            const auto shown =
                lectionary.readings_with_variants(date("2026-10-09"), CalendarStyle::New, order);
            const auto vespers = std::ranges::find(shown.readings, ReadingKind::Vespers, &Reading::kind);
            check(
                shown.readings.size() == 6 && vespers != shown.readings.end() &&
                    vespers->segments().size() == 3 && vespers->additional[1].first == VerseRef{4, 7} &&
                    std::ranges::count(shown.readings, ReadingKind::Epistle, &Reading::kind) == 2 &&
                    std::ranges::count(shown.readings, ReadingKind::Gospel, &Reading::kind) == 2,
                "October 9 shows its Vespers prophecies as one reading and keeps the saint's readings apart");
        }
        const auto hours =
            lectionary.service_readings_for(date("2026-01-05"), CalendarStyle::New, Tradition::Slavic);
        check(std::ranges::count(hours.readings, ReadingKind::Hours, &Reading::kind) == 12 &&
                  std::ranges::count(hours.readings, ReadingKind::OtherService, &Reading::kind) == 5,
              "Theophany eve includes Royal Hours and the Great Blessing of Waters");
        const auto saturday =
            lectionary.service_readings_for(date("2026-04-11"), CalendarStyle::New, Tradition::Slavic);
        check(std::ranges::count(saturday.readings, std::string("Vespers"), &Reading::service) == 15,
              "Holy Saturday includes fifteen Vespers readings");
        const auto moved_eve =
            lectionary.service_readings_for(date("2026-03-30"), CalendarStyle::New, Tradition::Slavic);
        const auto moved_feast =
            lectionary.service_readings_for(date("2026-03-31"), CalendarStyle::New, Tradition::Slavic);
        check(std::ranges::count(moved_eve.readings, std::string("Vespers"), &Reading::service) == 5 &&
                  std::ranges::none_of(moved_eve.readings,
                                       [](const Reading& r) {
                                           return !r.can_open();
                                       }) &&
                  std::ranges::count(moved_feast.readings, std::string("Vespers"), &Reading::service) == 2,
              "Lenten feast Vespers move to the eve, with composites at their sourced verses");
        {
            // Composites open at their cited verses and keep the short published citation.
            const auto innocent =
                lectionary.service_readings_for(date("2026-10-06"), CalendarStyle::New, Tradition::Slavic);
            std::vector<const Reading*> vespers;
            for (const auto& r : innocent.readings)
                if (r.service == "Vespers")
                    vespers.push_back(&r);
            const std::string paremii =
                "https://azbyka.ru/otechnik/Pravoslavnoe_Bogosluzhenie/parimii-na-vse-dni-goda/";
            check(vespers.size() == 3 && std::ranges::all_of(vespers, &Reading::can_open) &&
                      vespers[0]->citation == "Prov 10, 3, 8" &&
                      vespers[0]->assignment_source_url == paremii + "458#458_316" &&
                      vespers[0]->passage.book == "Prov" && vespers[0]->passage.first == VerseRef{10, 7} &&
                      vespers[1]->citation == "Wis 4, 5" &&
                      vespers[1]->assignment_source_url == paremii + "454#454_306" &&
                      vespers[1]->segments().size() == 4 && vespers[1]->passage.first == VerseRef{4, 7} &&
                      vespers[2]->assignment_source_url == paremii + "458#458_317",
                  "Composite Vespers open at their sourced verses");
            for (const auto& rule : corpus.reading_rules())
                check(!rule.reading.citation.starts_with("Composite") &&
                          !rule.reading.citation.starts_with("​"),
                      "Composite citations use the short published form");
        }

        // oca.org's printed daily pairs for 2026. The Slavic order's Epistle and Gospel must be
        // one of them, within a verse for translation boundaries, except on these dates.
        const std::map<std::string, std::string> oca_exceptions = {
            {"2026-02-24", "Orthocal reads no fixed propers in Clean Week"},
            {"2026-02-27", "Orthocal reads no fixed propers in Clean Week"},
            {"2026-10-31", "oca.org's saint is not in Orthocal's tables"},
            {"2026-11-08", "Orthocal reads the Unmercenaries on the Sunday after November 1"}};
        std::ifstream oca(argv[4]);
        check(bool(oca), "oca.org fixture missing");
        int compared = 0, leading = 0;
        std::set<std::string> excepted;
        std::string line;
        while (std::getline(oca, line)) {
            if (line.empty() || line.front() == '#')
                continue;
            std::istringstream row(line);
            std::string iso, field;
            std::getline(row, iso, '\t');
            using Start = std::tuple<std::string, int, int>;
            const auto start = [](const std::string& token) -> std::optional<Start> {
                const auto at = token.rfind('_');
                if (token == "-" || at == std::string::npos)
                    return std::nullopt;
                const int value = std::stoi(token.substr(at + 1));
                return Start{token.substr(0, at), value / 1000, value % 1000};
            };
            std::vector<std::pair<std::optional<Start>, std::optional<Start>>> pairs;
            while (std::getline(row, field, '\t')) {
                const auto space = field.find(' ');
                pairs.emplace_back(start(field.substr(0, space)), start(field.substr(space + 1)));
            }
            std::optional<Start> epistle, gospel;
            for (const auto& listed :
                 lectionary.readings_for(date(iso.c_str()), CalendarStyle::New, Tradition::Slavic).readings) {
                const auto reading = published(listed);
                const Start here{reading.passage.book, reading.passage.first.chapter,
                                 reading.passage.first.verse};
                if (reading.kind == ReadingKind::Epistle)
                    epistle = here;
                if (reading.kind == ReadingKind::Gospel)
                    gospel = here;
            }
            const auto near = [](const std::optional<Start>& a, const std::optional<Start>& b) {
                return a && b && std::get<0>(*a) == std::get<0>(*b) && std::get<1>(*a) == std::get<1>(*b) &&
                       std::abs(std::get<2>(*a) - std::get<2>(*b)) <= 1;
            };
            const bool complete = std::ranges::any_of(pairs, [](const auto& pair) {
                return pair.first && pair.second;
            });
            std::optional<std::size_t> found;
            for (std::size_t i = 0; i < pairs.size() && !found; ++i)
                if (near(epistle, pairs[i].first) && near(gospel, pairs[i].second))
                    found = i;
            // Holy Week and Lenten weekdays: oca.org prints no complete pair, or this order reads prophecies.
            if (!complete || (!epistle && !gospel) || found) {
                if (complete && !found)
                    excepted.insert(iso);
                if (found) {
                    ++compared;
                    leading += *found == 0;
                }
                continue;
            }
            excepted.insert(iso);
        }
        for (const auto& iso : excepted)
            check(oca_exceptions.contains(iso), "Slavic order differs from oca.org on " + iso);
        check(excepted.size() == oca_exceptions.size(), "an oca.org exception no longer applies");
        check(compared >= 330 && leading >= 293,
              "Slavic order agreement with oca.org regressed: " + std::to_string(leading) + " of " +
                  std::to_string(compared));
        const auto thomas =
            lectionary.readings_for(date("2026-10-06"), CalendarStyle::New, Tradition::Antiochian);
        check(thomas.readings.size() == 2 && thomas.readings[0].passage.book == "1Cor" &&
                  thomas.readings[1].passage.book == "John",
              "Antiochian Apostle Thomas propers");
        check(orthodox_pascha(2028) == date("2028-04-16") && orthodox_pascha(2029) == date("2029-04-08") &&
                  orthodox_pascha(2030) == date("2030-04-28"),
              "official future Pascha dates");
        const auto pentecost =
            lectionary.readings_for(date("2026-05-31"), CalendarStyle::New, Tradition::Antiochian)
                .readings[1];
        const auto in_pentecost = [&](VerseRef ref) {
            return std::ranges::any_of(pentecost.segments(), [&](const Passage& part) {
                return part.contains(ref);
            });
        };
        check(in_pentecost({7, 37}) && !in_pentecost({8, 1}) && in_pentecost({8, 12}),
              "omitted Pentecost verses must stay omitted");
        for (int year = 2027; year <= 2035; ++year) {
            const auto pascha =
                lectionary.readings_for(orthodox_pascha(year), CalendarStyle::New, Tradition::Antiochian);
            check(pascha.readings.size() == 2 && pascha.readings[0].passage.book == "Acts" &&
                      pascha.readings[0].passage.first == VerseRef{1, 1} &&
                      pascha.readings[1].passage.book == "John",
                  "future computed Pascha");
        }
        const Passage passage{"Luke", {6, 27}, {6, 36}};
        check(passage.contains({6, 31}) && !passage.contains({6, 37}), "passage membership");
        check(corpus.source_for_language("el", "Ps") == "grc-lxx", "OT source selection");
        check(corpus.source_for_language("el", "Luke") == "grc-patriarchal", "NT source selection");
        check(corpus.source_for_language("xx", "Luke").empty(), "unknown source selection");
        check(corpus.read_only(), "corpus not read-only");
        check(corpus.sources().size() == 5, "source catalog");
        check(corpus.book_name("Ps") == "Psaltaren" && corpus.book_name("Ps", "en") == "Psalms" &&
                  corpus.book_name("Ps", "el") == "ΨΑΛΜΟΙ",
              "Book names must retain each edition's language");
        check(corpus.book_name("missing-book") == "missing-book" &&
                  corpus.book_name("Ps", "unknown-language") == corpus.book_name("Ps"),
              "Book-name fallbacks must retain their previous behavior");
        for (const auto& [book, ch, first, last] : std::vector<std::tuple<std::string, int, int, int>>{
                 {"Ps", 23, 1, 6}, {"Ps", 24, 1, 10}, {"Luke", 6, 1, 49}, {"Phil", 2, 1, 30}}) {
            for (int v = first; v <= last; ++v)
                for (const std::string language : {"sv", "el", "en"}) {
                    auto text = corpus.parallel_verse("sv1917", corpus.source_for_language(language, book),
                                                      book, {ch, v});
                    check(text && !text->text.empty(), "required aligned fixture verse missing");
                }
        }
        check(!corpus.verse("sv1917", "Luke", {99, 1}), "missing text is not an error");
        check(!corpus.verse("unknown", "Luke", {6, 1}), "unknown source is not an error");
        auto map = corpus.alignment("sv1917", "grc-lxx", {"Ps", {23, 1}, {23, 6}});
        check(map && map->kind == AlignmentKind::Renumbered && map->to.book == "Ps", "LXX mapping");
        auto greek = corpus.parallel_verse("sv1917", "grc-lxx", "Ps", {23, 1});
        check(greek && greek->ref.chapter == 22 && greek->text.find("Κύριος") != std::string::npos,
              "Greek Psalm alignment");
        auto psalm = corpus.map_passage("sv1917", "grc-lxx", {"Ps", {51, 1}, {51, 21}});
        check(psalm.size() == 1 && psalm[0].first == VerseRef{50, 1} && psalm[0].last == VerseRef{50, 21},
              "Hebrew and Greek Psalm titles share verse numbers");
        // LXX2012 prints Psalm titles apart; the build keeps them with verse 1.
        auto title = corpus.parallel_verse("grc-lxx", "en-lxx2012", "Ps", {3, 1});
        check(title && title->ref == VerseRef{3, 1} && title->text.find("Abessalom") != std::string::npos &&
                  title->text.find("multiplied") != std::string::npos,
              "LXX2012 includes the title in verse 1");
        auto title_shift = corpus.parallel_verse("grc-lxx", "en-lxx2012", "Ps", {3, 3});
        check(title_shift && title_shift->ref == VerseRef{3, 2}, "source-specific title numbering");
        check(corpus.parallel_verse("grc-lxx", "en-lxx2012", "Ps", {3, 2}).error() ==
                  "Ingår i föregående vers",
              "merged verse is shown once");
        check(corpus.parallel_verse("grc-lxx", "sv1917", "1Esd", {1, 1}).error() == "Saknas i denna utgåva" &&
                  corpus.parallel_verse("grc-lxx", "sv1917", "Ps", {151, 1}).error() ==
                      "Saknas i denna utgåva",
              "texts the Swedish edition lacks");
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Ps", {100, 1})->ref == VerseRef{99, 1},
              "complete Psalter alignment");
        // The Septuagint orders Jeremiah differently and lacks about one eighth of the Hebrew text.
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Jer", {31, 31})->ref == VerseRef{38, 31},
              "new covenant in LXX Jeremiah 38");
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Jer", {46, 2})->ref == VerseRef{26, 2},
              "oracle against Egypt in LXX Jeremiah 26");
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Jer", {25, 15})->ref == VerseRef{32, 15},
              "cup of wrath in LXX Jeremiah 32");
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Jer", {33, 14}).error() == "Saknas i Septuaginta",
              "verse without LXX counterpart");
        check(corpus.parallel_verse("grc-lxx", "sv1917", "Jer", {23, 40, "a"})->ref == VerseRef{23, 7},
              "transposed LXX verse");
        // LXX2012 translates the Greek but follows English chapter divisions in places.
        check(corpus.parallel_verse("en-lxx2012", "grc-lxx", "Mal", {4, 5})->ref == VerseRef{3, 23},
              "LXX2012 Malachi 4 is Greek Malachi 3:19-24");
        check(corpus.parallel_verse("grc-lxx", "en-lxx2012", "Josh", {9, 2, "a"})->ref == VerseRef{8, 30},
              "LXX2012 prints the altar on Mount Ebal in its Hebrew place");
        check(corpus.parallel_verse("sv1917", "en-lxx2012", "Jer", {31, 31})->ref == VerseRef{38, 31},
              "Swedish reaches LXX2012 through the Greek");
        auto nehemiah = corpus.parallel_verse("sv1917", "grc-lxx", "Neh", {1, 1});
        check(nehemiah && nehemiah->ref == VerseRef{11, 1}, "Nehemiah in Greek 2 Esdras 11");
        check(corpus.parallel_verse("sv1917", "grc-lxx", "Baruch", {6, 2})->ref == VerseRef{1, 3},
              "Letter of Jeremiah after its Greek title");
        auto joel = corpus.map_passage("en-lxx2012", "grc-lxx", {"Joel", {2, 28}, {2, 32}});
        check(joel.size() == 1 && joel[0].first == VerseRef{3, 1} && joel[0].last == VerseRef{3, 5},
              "LXX2012 Joel 2:28-32 is Greek 3:1-5");
        const auto rules = corpus.reading_rules();
        // The build restates Orthocal's KJV-numbered references in the Greek editions' numbering.
        check(std::ranges::all_of(rules,
                                  [&](const auto& r) {
                                      return r.reading.reference == "grc-lxx" ||
                                             (r.reading.reference == "grc-patriarchal" &&
                                              corpus.new_testament_book(r.reading.passage.book));
                                  }),
              "lectionary references use Greek numbering");
        const auto joel_reading = std::ranges::find_if(rules, [](const auto& r) {
            return r.reading.citation.starts_with("Joel 2.23-32");
        });
        check(joel_reading != rules.end() && joel_reading->reading.passage.first == VerseRef{2, 23} &&
                  joel_reading->reading.passage.last == VerseRef{3, 5},
              "KJV Joel 2:23-32 is LXX Joel 2:23-3:5");
        const auto doxology = std::ranges::find_if(rules, [](const auto& r) {
            return r.reading.citation == "Romans 14.19-23, 16.25-27";
        });
        check(doxology != rules.end() && doxology->reading.additional.size() == 1 &&
                  doxology->reading.additional[0].first == VerseRef{14, 24},
              "the KJV doxology is Patriarchal Romans 14:24-26");
        // Holy Saturday's 15th Vespers reading: Daniel 3 with the Song of the Three.
        const auto song = std::find_if(rules.begin(), rules.end(), [](const auto& r) {
            return r.pdist == -1 && r.service == "Vespers" && r.description == "15th reading";
        });
        check(song != rules.end() && song->reading.reference == "grc-lxx",
              "Song of the Three uses Septuagint numbering");
        // Readings open in Septuagint numbering; the Swedish pane finds its own verses.
        const auto daniel = corpus.localize(song->reading).segments();
        check(daniel.size() == 1 && daniel[0].book == "Dan" && daniel[0].first == VerseRef{3, 1},
              "Holy Saturday Daniel reading in LXX numbering");
        const auto swedish = corpus.map_passage(osb_reference, "sv1917", daniel[0]);
        check(swedish.size() == 2 && swedish[0].book == "Dan" && swedish[0].last == VerseRef{3, 23} &&
                  swedish[1].book == "PrAzar",
              "Holy Saturday Daniel reading in Swedish");
        const auto baruch = std::find_if(rules.begin(), rules.end(), [](const auto& r) {
            return r.reading.passage.book == "Baruch";
        });
        check(baruch != rules.end() && corpus.localize(baruch->reading).passage.first == VerseRef{3, 36},
              "Baruch reading in LXX numbering");
        check(corpus.map_passage(osb_reference, "sv1917", corpus.localize(baruch->reading).passage)
                      .front()
                      .first == VerseRef{3, 36},
              "Baruch 3:35 is Swedish 3:36");
        check(corpus.canon_book("Neh", 1) && corpus.canon_book("Neh", 13) && !corpus.canon_book("Neh", 14) &&
                  !corpus.canon_book("Ezra", 11),
              "OSB Nehemiah is a separate reference book");
        check(corpus.canon_book("Esth", 1) && corpus.canon_book("Esth", 1)->code == "Esth",
              "OSB Esther owns its references independently of source book codes");
        check(corpus.title("Gen") == "Första Moseboken", "Swedish book names remain available");
        // Book order comes from the OSB reference metadata, book data from books.tsv.
        check(corpus.canon().size() == 78 && corpus.canon().front().code == "Gen" &&
                  corpus.canon().back().code == "Rev" && corpus.canon().back().new_testament &&
                  !corpus.canon().front().new_testament,
              "OSB canon order");
        check(corpus.book_abbreviation("Gen") == "1 Mos" && corpus.book_abbreviation("unknown") == "unknown",
              "book abbreviations");
        check(corpus.new_testament_book("Rev") && !corpus.new_testament_book("Mal") &&
                  corpus.deuterocanonical_book("Tob") && !corpus.deuterocanonical_book("Gen") &&
                  corpus.stanza_book("Ps") && !corpus.stanza_book("Gen"),
              "book classifications");
        auto refs = corpus.coordinates("sv1917", "Luke");
        check(refs.front().chapter == 1 && refs.back().chapter == 24, "adjacent chapter context");
        {
            // Every part opens and can be read aloud in Swedish, and no chapter is in two parts.
            const auto& plans = reading_plans();
            check(plans.size() == 3 && plans[0].parts.size() == 60 && plans[1].parts.size() == 182 &&
                      plans[2].parts.size() == 36,
                  "three reading plans");
            std::map<std::string, std::set<int>> covered;
            for (const auto& plan : plans)
                for (const auto& part : plan.parts) {
                    const auto reading = plan_reading(corpus, part, "sv");
                    check(reading.segments().size() == part.size(),
                          "plan range without text: " + plan_label(corpus, part));
                    const auto first = reading.passage;
                    check(!chapter_sources(corpus, osb_reference, first.book, first.first.chapter).empty(),
                          "plan part has no available source: " + plan_label(corpus, part));
                    for (const auto& range : part)
                        for (int chapter = range.first; chapter <= range.last; ++chapter)
                            check(covered[range.book].insert(chapter).second,
                                  "chapter in two plan parts: " + range.book);
                }
            check(plan_label(corpus, plans[0].parts[50]) == "2 Tim 1–4 + Tit 1–3 + Filem", "plan part label");
            check(plan_key(plans[1], 6) == "plan:ot:v4:7", "versioned plan part key");
            // Together the plans cover the Septuagint canon and the New Testament, except the books
            // that have no Swedish text yet.
            std::vector<std::string> missing;
            for (const auto& book : corpus.canon()) {
                std::set<int> chapters;
                for (const auto& ref : corpus.coordinates(osb_reference, book.code))
                    if (ref.chapter <= book.last_chapter)
                        chapters.insert(ref.chapter);
                for (int chapter : chapters)
                    if (!covered[book.code].contains(chapter))
                        missing.push_back(book.code + " " + std::to_string(chapter));
            }
            std::string listed;
            for (const auto& item : missing)
                listed += item + ", ";
            check(missing.empty(), "plans cover the complete OSB inventory: " + listed);
        }
        check(corpus.verse("sv1917", "Gen", {1, 1}) && corpus.verse("sv1917", "Rev", {22, 21}),
              "Swedish full corpus endpoints");
        check(corpus.verse("sv1917", "Wis", {1, 1}) && corpus.verse("sv1917", "Tob", {14, 15}),
              "Swedish apocrypha");
        check(corpus.verse("grc-lxx", "4Macc", {18, 24}) && corpus.verse("grc-patriarchal", "Rev", {22, 21}),
              "Greek full corpus endpoints");
        check(corpus.verse("en-lxx2012", "Gen", {1, 1}) && corpus.verse("en-lxx2012", "4Macc", {18, 24}) &&
                  corpus.verse("en-web", "Matt", {1, 1}) && corpus.verse("en-web", "Rev", {22, 21}),
              "English full corpus endpoints");
        check(corpus.verse("grc-lxx", "Dan", {1, 1}) && corpus.verse("grc-lxx", "Dan", {12, 13}),
              "Greek Daniel retained from DAG source");
        check(corpus.verse("grc-lxx", "Gen", {31, 50, "a"}) &&
                  corpus.verse("grc-lxx", "EsthGr", {4, 17, "z"}),
              "lettered LXX coordinates retained");
        const auto esther = corpus.coordinates("grc-lxx", "EsthGr");
        const auto letter = std::find(esther.begin(), esther.end(), VerseRef{4, 17, "a"});
        check(letter != esther.end() && letter != esther.begin() && *(letter - 1) == VerseRef{4, 17},
              "lettered portions retain their position in continuous Scripture");
        check(!corpus.verse("en-web", "Gen", {1, 1}) && !corpus.verse("en-kjv", "Gen", {1, 1}),
              "build witnesses do not ship");
        for (const std::string book : {"Wis", "Tob", "3Macc", "4Macc", "1Esd", "PrMan", "EpJer", "PrAzar"})
            check(corpus.verse("en-lxx2012", book, {1, 1}).has_value(), "English deuterocanonical content");
        check(corpus.verse("en-lxx2012", "Ps", {151, 1}).has_value(), "English Psalm 151");
        check(corpus.verse("en-lxx2012", "Ps", {23, 4})->text.find("\u2303") == std::string::npos,
              "LXX2012 plural marks removed");
        auto joined = corpus.verse("en-lxx2012", "1Kgs", {4, 26});
        check(joined && joined->ref == VerseRef{4, 24} && joined->last == VerseRef{4, 28},
              "joined publisher verse retains its complete range");
        check(corpus.source_for_language("en", "Wis") == "en-lxx2012" &&
                  corpus.source_for_language("en", "John") == "en-web",
              "English edition selection");
        check(corpus.verse("grc-patriarchal", "Luke", {2, 23})->text.find("strong=") == std::string::npos,
              "nested USFM attributes leaked into display");
        sqlite3* readonly = nullptr;
        check(sqlite3_open_v2(argv[1], &readonly, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
              "read-only test connection");
        const int write_result = sqlite3_exec(readonly, "DELETE FROM verse", nullptr, nullptr, nullptr);
        sqlite3_close(readonly);
        check(write_result == SQLITE_READONLY, "read-only database accepted writes");
        {
            // The 1917 New Testament renumbers a few passages against the Greek and English texts.
            const auto greek = [&](const std::string& book, VerseRef ref) {
                return corpus.counterparts("sv1917", "grc-patriarchal", book, ref);
            };
            using Refs = std::vector<std::pair<std::string, VerseRef>>;
            check(greek("John", {11, 34}) == Refs{{"John", {11, 34}}, {"John", {11, 35}}},
                  "'Och Jesus grät' ends Swedish John 11:34");
            check(greek("John", {11, 35}) == Refs{{"John", {11, 36}}}, "Swedish John 11:35 is Greek 11:36");
            check(greek("John", {1, 39}) == Refs{{"John", {1, 40}}},
                  "the Patriarchal text divides John 1:38-39");
            check(greek("Rom", {16, 25}) == Refs{{"Rom", {14, 24}}},
                  "the Byzantine doxology follows Romans 14:23");
            check(greek("Matt", {23, 13}) == Refs{{"Matt", {23, 14}}},
                  "the Byzantine text swaps Matthew 23:13-14");
            check(greek("Luke", {10, 6}) == Refs{{"Luke", {10, 6}}}, "unchanged verses keep their numbers");
            check(corpus.counterparts("sv1917", "en-web", "John", {11, 35}) == Refs{{"John", {11, 36}}},
                  "the WEB shares the John 11 division with the Greek");
            check(corpus.counterparts("sv1917", "en-web", "Matt", {23, 13}) == Refs{{"Matt", {23, 14}}},
                  "the WEB follows the Byzantine Matthew 23:13-14");
            check(corpus.counterparts("grc-patriarchal", "en-web", "John", {1, 40}) ==
                      Refs{{"John", {1, 39}}},
                  "the WEB keeps the common John 1:38-39 division");
            check(corpus.counterparts("grc-patriarchal", "en-web", "Acts", {8, 37}).empty(),
                  "the WEB omits Acts 8:37");
            const auto philippians = corpus.map_passage("en-web", "sv1917", {"Phil", {1, 12}, {1, 20}});
            check(philippians.size() == 1 && philippians.front().first == VerseRef{1, 12} &&
                      philippians.front().last == VerseRef{1, 20},
                  "a reordered verse pair stays one reading");
        }
        {
            // Word study: 1917 forms resolve to Dalin entries and Nyström's biblical articles.
            const auto& study = corpus;
            const auto entries = study.swedish("svarade");
            check(entries.size() == 1 && entries.front().headword == "svara",
                  "regular verb form resolves to its Dalin entry");
            check(!study.swedish("fingo").empty() && study.swedish("fingo").front().headword == "få",
                  "irregular form resolves through the table");
            check(study.swedish("hjärta").front().headword == "hjerta",
                  "reformed spelling resolves to Dalin's spelling");
            const auto heart = study.saldo("hjärta");
            check(heart.size() == 2 && heart.front().headword == "hjärta" && heart.front().gram == "nn" &&
                      heart.front().match == "exact" &&
                      heart.front().associations.find("organ") != std::string::npos &&
                      heart.back().associations == "känna",
                  "SALDO preserves separate senses and semantic associations without phrase matches");
            const auto many = study.saldo("många");
            check(many.size() == 1 && many.front().headword == "mången" && many.front().gram == "pn" &&
                      many.front().match == "inflected",
                  "SALDO uses the published pronoun form instead of its synthetic placeholder");
            const auto answered = study.saldo("svarade");
            check(answered.size() == 2 && answered.front().headword == "svara" &&
                      answered.front().match == "inflected",
                  "SALDO matches a published verb inflection");
            const auto archaic = study.saldo("fingo");
            check(!archaic.empty() && archaic.front().headword == "få" && archaic.front().gram == "vb" &&
                      archaic.front().match == "curated",
                  "reviewed archaic forms retain their matching provenance");
            check(study.saldo("xhjärta").empty(), "modern lookup never guesses a suffix");
            const auto jesu = study.biblical("jesu");
            check(jesu.size() == 2 && jesu.front().headword == "Jesus Kristus" &&
                      jesu.back().headword == "Jesus Justus",
                  "Jesu resolves to the biblical entries without hiding the namesake");
            const auto kristi = study.biblical("kristi");
            check(kristi.size() == 1 && kristi.front().headword == "Kristus" &&
                      kristi.front().definition.find("Messias") != std::string::npos,
                  "Kristi resolves to the historical explanation of the title Kristus");
            const auto jerusalems = study.biblical("jerusalems");
            check(jerusalems.size() == 1 && jerusalems.front().headword == "Jerusalem" &&
                      jerusalems.front().definition.size() > 500 &&
                      jerusalems.front().url == "https://runeberg.org/biblobok/ordbok_j.html#Jerusalem",
                  "place genitive retains the full article and its source");
            const auto biblical_headword = [&](const std::string& form) {
                const auto found = study.biblical(form);
                return found.empty() ? std::string{} : found.front().headword;
            };
            check(biblical_headword("pauli") == "Paulus", "Latin genitive of Paulus resolves");
            check(biblical_headword("galileen") == "Galileen", "biblical region has its own article");
            check(biblical_headword("nasaret") == "Nazaret",
                  "1917 place spelling resolves to the old spelling");
            const auto rebecka = study.biblical("rebecka");
            check(!rebecka.empty() && rebecka.front().definition.starts_with("Betuels dotter"),
                  "an article without an HTML anchor remains available");
            check(biblical_headword("nebukadnessars") == "Nebukadnezar",
                  "genitive also resolves through a name spelling alias");
            const auto mordokai = study.biblical("mordokai");
            check(mordokai.size() == 2 && mordokai.back().headword == "Mordekai" &&
                      mordokai.back().definition.find("Ester") != std::string::npos,
                  "a cross-reference opens its referenced biography offline");
            check(study.biblical("xjesu").empty(), "name matching does not use substrings");
        }
        auto lexicon = corpus.pronunciations("sv");
        auto utterance = make_utterance("Melkisedek, inte XMelkisedek eller Melkisedeks.", "sv", lexicon);
        check(utterance.display_text == "Melkisedek, inte XMelkisedek eller Melkisedeks.",
              "display text was mutated");
        check(utterance.speech_text == "Melki-sedek, inte XMelkisedek eller Melkisedeks.",
              "token boundaries");
        check(make_utterance("MELKISEDEK!", "sv", lexicon).speech_text == "Melki-sedek!",
              "case insensitive pronunciation");
        lexicon.push_back({"sv", "helige Ande", "heliga ande", "", 200});
        lexicon.push_back({"sv", "Åke", "Oke", "", 100});
        check(make_utterance("helige Ande; helige, Ande; ÅKE.", "sv", lexicon).speech_text ==
                  "heliga ande; helige, Ande; Oke.",
              "phrase and Swedish token matching");
        check(make_utterance("Melkisedek", "en", lexicon).speech_text == "Melkisedek", "language isolation");
        check(make_utterance("Manasse och Manasses söner", "sv", lexicon).speech_text ==
                  "⟦man'asE-⟧ och ⟦man'asE-s⟧ söner",
              "bundled name phonemes and genitive");
        const std::string joses = "Joses, Joses' moder; Joses’ moder. Josef och Josefs.";
        const auto joses_utterance = make_utterance(joses, "sv", lexicon);
        check(joses_utterance.display_text == joses,
              "Joses and possessive punctuation remain in display text");
        check(joses_utterance.speech_text ==
                  "⟦j'u:sE-s⟧, ⟦j'u:sE-s⟧' moder; ⟦j'u:sE-s⟧’ moder. Josef och Josefs.",
              "Joses and its possessive keep the Swedish j instead of Spanish Jose's pronunciation");
        check(make_utterance("Han bar sitt kors, och hans ben", "sv", lexicon).speech_text ==
                  "⟦h'an⟧ bar sitt ⟦k'Os.⟧, och ⟦h'ans⟧ ⟦b'e:n⟧",
              "bundled homographs take the sense of this Bible, not a name's or another word's");
        check(make_utterance("en ko, sin kos och Kos.", "sv", lexicon).speech_text ==
                  "en ⟦k'u:⟧, sin kos och Kos.",
              "word-only corrections do not turn the place Kos or sin kos into a cow's genitive");
        check(make_utterance("Jo, jo! Jos.", "sv", lexicon).speech_text == "⟦j'u:⟧, ⟦j'u:⟧! Jos.",
              "jo uses the bundled word correction without generating a genitive");
        check(make_utterance("Vi vi; v, l. Vis.", "sv", lexicon).speech_text == "⟦v'i:⟧ ⟦v'i:⟧; v, l. Vis.",
              "vi is a word correction; standalone letters have no pronunciation workarounds");
        const std::string abbreviations = "O.s.v.; o. s. v.; osv.; t. ex.; bl.a.; d.v.s.; dvs.; m.m.; "
                                          "m.fl.; fr.o.m.; t.o.m.; s.k.; f.Kr.; e.Kr.";
        const auto expanded = make_utterance(abbreviations, "sv", lexicon);
        check(expanded.display_text == abbreviations &&
                  expanded.speech_text ==
                      "och så vidare; och så vidare; och så vidare; till exempel; bland annat; "
                      "det vill säga; det vill säga; med mera; med flera; från och med; till och med; "
                      "så kallad; före Kristus; efter Kristus.",
              "Swedish abbreviations are spoken in full without changing the displayed text");
        check(make_utterance("åosv.; o, s, v.; osv; o s v", "sv", lexicon).speech_text ==
                  "åosv.; o, s, v.; osv; o s v",
              "abbreviation matching requires whole words and the abbreviation's dots");
        check(make_utterance("osv.", "en", lexicon).speech_text == "osv.",
              "Swedish abbreviation expansion does not affect another language");
        const std::vector<Pronunciation> punctuation_aliases{{"sv", "osv.", "och så vidare", "", 50},
                                                             {"sv", "o.s.v.", "och så vidare", "", 50},
                                                             {"sv", "m.fl.", "med flera", "", 50},
                                                             {"sv", "t.ex.", "till exempel", "", 50},
                                                             {"sv", "bl.a.", "bland annat", "", 50}};
        for (const auto& [before, after] : std::vector<std::pair<std::string, std::string>>{
                 {"osv. Nästa mening.", "och så vidare. Nästa mening."},
                 {"o. s. v. Ännu en mening.", "och så vidare. Ännu en mening."},
                 {"m.fl. Övriga väntar.", "med flera. Övriga väntar."},
                 {"osv. och annat", "och så vidare och annat"},
                 {"osv., men", "och så vidare, men"},
                 {"osv.. Nästa mening.", "och så vidare. Nästa mening."},
                 {"osv.» Nästa mening.", "och så vidare.» Nästa mening."},
                 {"osv.»", "och så vidare.»"},
                 {"t.ex. Stockholm och bl.a. Åke.", "till exempel Stockholm och bland annat Åke."}}) {
            const auto spoken = make_utterance(before, "sv", punctuation_aliases);
            check(spoken.display_text == before && spoken.speech_text == after,
                  "abbreviation punctuation is preserved at sentence boundaries: " + before);
        }
        for (const auto& ref : std::vector<VerseRef>{{4, 10}, {4, 11}, {4, 17}, {7, 12}}) {
            const auto text = corpus.verse("sv1917", "Ezra", ref)->text;
            const auto speech = make_utterance(text, "sv", lexicon);
            check(speech.display_text == text &&
                      speech.speech_text.find("och så vidare") != std::string::npos &&
                      speech.speech_text.find("o. s. v.") == std::string::npos,
                  "every abbreviated o. s. v. in Ezra is expanded for speech");
        }
        check(corpus.verse("sv1917", "Jer", {8, 15})->text.starts_with("Vi bida efter frid"),
              "Jeremiah's missing i is corrected from the printed text");
        check(corpus.verse("sv1917", "Sir", {28, 9})->text.find("leva i frid") != std::string::npos,
              "Sirach's OCR l is corrected to the printed i");
        struct TextCorrection {
            std::string book;
            VerseRef ref;
            std::string before, after;
        };
        for (const auto& correction : std::vector<TextCorrection>{
                 {"2Kgs", {18, 21}, "förtröstar val nu", "förtröstar väl nu"},
                 {"Job", {9, 35}, "med min själv", "med mig själv"},
                 {"Ps", {22, 30}, "sin själv vid liv", "sin själ vid liv"},
                 {"Prov", {20, 7}, "går det val efter honom", "går det väl efter honom"},
                 {"Prov", {23, 7}, "kan han val säga", "kan han väl säga"},
                 {"Jer", {8, 12}, "på skam, övade styggelse", "på skam, ty de övade styggelse"},
                 {"Jer", {16, 13}, "ett land son varken", "ett land som varken"},
                 {"Jer", {17, 21}, "Tagen eder val till vara", "Tagen eder väl till vara"},
                 {"Jer", {20, 13}, "den fattiges själv", "den fattiges själ"},
                 {"Jer", {29, 7}, "så går det ock eder val", "så går det ock eder väl"},
                 {"Ezek", {3, 17}, "å mina vagnar", "å mina vägnar"},
                 {"Luke", {12, 48}, "vad som val slag värt", "vad som var slag värt"},
                 {"Tob", {11, 15}, "min son TobiasI»", "min son Tobias!»"},
                 {"Sir", {4, 8}, "saktmod vänligr svar", "saktmod vänligt svar"},
                 {"Sir", {13, 10}, "icke rnå bliva glömd", "icke må bliva glömd"},
                 {"Sir", {17, 23}, "vedergälla dern", "vedergälla dem"},
                 {"Sir", {34, 9}, "En väiuppfostrad man", "En väluppfostrad man"},
                 {"Sir", {34, 19}, "skygd rnot den", "skygd mot den"},
                 {"Sir", {50, 26}, "de sorn äro", "de som äro"},
                 {"1Macc", {7, 38}, "låt dern icke", "låt dem icke"},
                 {"1Macc", {15, 41}, "fotfolk, sorn skulle", "fotfolk, som skulle"},
                 {"2Macc", {3, 25}, "sina f ramhovar", "sina framhovar"},
                 {"PrAzar", {1, 19}, "handla rned oss", "handla med oss"},
                 {"Ps", {44, 11}, "för ovånnen", "för ovännen"},
                 {"Ps", {66, 14}, "mina låppar", "mina läppar"},
                 {"Ps", {102, 27}, "skall förvanda dem", "skall förvandla dem"},
                 {"Prov", {12, 22}, "lögnaktiga låppar", "lögnaktiga läppar"},
                 {"Wis", {18, 16}, "din oryggliga befällning", "din oryggliga befallning"},
                 {"Sir", {7, 8}, "bliva osträffad", "bliva ostraffad"},
                 {"Sir", {11, 5}, "rnen den som", "men den som"},
                 {"Sir", {13, 23}, "»Vern är denne?", "»Vem är denne?"},
                 {"Sir", {31, 16}, "väcka motvilja.\"", "väcka motvilja."},
                 {"Sir", {31, 25}, "de\"som vinet", "de som vinet"},
                 {"Sir", {37, 3}, "har da vällt fram", "har du vällt fram"},
                 {"Sir", {44, 9}, "någon aminnelse", "någon åminnelse"},
                 {"Baruch", {4, 18}, "som nar låtit", "som har låtit"},
                 {"1Macc", {1, 12}, "fann behåg", "fann behag"},
                 {"1Macc", {10, 14}, "Betsura stannåde", "Betsura stannade"},
                 {"1Macc", {14, 42}, "insätta uppsyningsrnan", "insätta uppsyningsmän"},
                 {"2Macc", {5, 22}, "kvar uppsyningsrnan", "kvar uppsyningsmän"},
                 {"2Macc", {11, 10}, "framåt, ordnåde", "framåt, ordnade"},
                 {"Bel", {1, 34}, "i lejonssgropen", "i lejongropen"},
                 {"Sus", {1, 20}, "»Se,trädgårdsporten", "»Se, trädgårdsporten"},
                 {"Sus", {1, 24}, "ropa med högröst", "ropa med hög röst"},
                 {"Hag", {1, 1}, "profeten Haggai Serubbabel", "profeten Haggai till Serubbabel"},
                 {"Hag", {1, 6}, "han far den allenast", "han får den allenast"},
                 {"Hag", {1, 14}, "HERREN Sebaots; sin Guds", "HERREN Sebaots, sin Guds"},
                 {"Hag", {2, 2}, "profeten Haggai han sade", "profeten Haggai; han sade"},
                 {"Jdt", {5, 22}, "allt folket som stod", "allt folket, som stod"},
                 {"Jdt", {7, 3}, "till Gyamon,", "till Cyamon,"},
                 {"Jdt", {7, 30}, "mina broder!", "mina bröder!"},
                 {"Jdt", {8, 5}, "hade gjorts sig", "hade gjort sig"},
                 {"Jdt", {10, 3}, "Manasses’livstid", "Manasses’ livstid"},
                 {"Jdt", {14, 15}, "sängpallen;och", "sängpallen; och"},
                 {"Wis", {2, 5}, "lik ens skugga", "lik en skugga"},
                 {"Wis", {2, 9}, "vår festgädje", "vår festglädje"},
                 {"Wis", {2, 17}, "bliver för honom,", "bliver för honom."},
                 {"Wis", {2, 24}, "höra honom till, få erfara", "höra honom till, de få erfara"},
                 {"Wis", {4, 16}, "skall, fästän död,", "skall, fastän död,"},
                 {"Wis", {6, 1}, "I som domen jordens", "I som dömen jordens"},
                 {"Wis", {7, 23}, "av fullkomlig finhet,", "av fullkomlig finhet."},
                 {"Wis", {8, 3}, "gör hon kunniga genom", "gör hon kunnig genom"},
                 {"Wis", {8, 11}, "skarpsinnig i i domslut", "skarpsinnig i domslut"},
                 {"Wis", {11, 7}, "barnen kulle dödas", "barnen skulle dödas"},
                 {"Wis", {12, 18}, "närhelst du vill.", "närhelst du vill,"},
                 {"Wis", {14, 22}, "mitt under i sitt elände", "mitt under sitt elände"},
                 {"Wis", {14, 19}, "likhet; och så begynte mängden,", "likhet;"},
                 {"Wis", {16, 1}, "dessa i människor", "dessa människor"},
                 {"Wis", {16, 9}, "som frälsar ifrån allt ont. Ty dessa", "Ty dessa"},
                 {"Wis", {17, 5}, "st järnlågorna", "stjärnlågorna"},
                 {"Wis", {17, 15}, "och Oförtänkt förskräckelse", "och oförtänkt förskräckelse"},
                 {"Wis", {18, 9}, "endräk-tigt", "endräktigt"},
                 {"Wis", {19, 3}, "jämrade Sigvid", "jämrade sig vid"},
                 {"Wis", {19, 7}, "överskygga. lägret", "överskygga lägret"},
                 {"Wis", {19, 15}, "ytterligare hemsökelser medan", "ytterligare hemsökelse: medan"},
                 {"Wis", {19, 17}, "rättfärd iges dörr", "rättfärdiges dörr"},
                 {"Tob", {1, 3}, "utvandra till i Nineve", "utvandra till Nineve"},
                 {"Tob", {1, 4}, "för evig tid,", "för evig tid."},
                 {"Tob", {4, 6}, "om du göre sanningen", "om du gör sanningen"},
                 {"Tob", {4, 14}, "över nätten för", "över natten för"},
                 {"Tob", {5, 1}, "svarade och sade honom", "svarade och sade till honom"},
                 {"Tob", {5, 11}, "sade till i honom", "sade till honom"},
                 {"Tob", {6, 6}, "tillsagt honom,;", "tillsagt honom;"},
                 {"Tob", {6, 18}, "eder -barmhärtighet", "eder barmhärtighet"},
                 {"Tob", {8, 6}, "Du sade:’Det", "Du sade: ’Det"},
                 {"Tob", {8, 19}, "åt dem ett i bröllop", "åt dem ett bröllop"},
                 {"Tob", {11, 18}, "också tillstädes; och Tobias’ bröllop firades med gamman i sju dagar.", "också tillstädes;"},
                 {"Tob", {12, 5}, "Sedan kallades han till sig", "Sedan kallade han till sig"},
                 {"Tob", {12, 6}, "Ja, våren icke sena", "Ja, varen icke sena"},
                 {"Tob", {13, 1}, "en bön, enl fröjdesång", "en bön, en fröjdesång"},
                 {"Tob", {13, 17}, "från Sufir, och alla dess gränder skola säga: »Halleluja», och lova Gud: »Lovad vare Gud evinnerligen, han som har upphöjt dig!»", "från Sufir,"},
                 {"Tob", {14, 10}, "ochhuru illa han", "och huru illa han"},
                 {"Tob", {14, 12}, "begrov i han henne", "begrov han henne"},
                 {"Tob", {14, 13}, "sin fader Tobits,", "sin fader Tobits."},
                 {"Sir", {2, 9}, "vanten eder lycka", "vänten eder lycka"},
                 {"Sir", {2, 10}, "åkallade honörn", "åkallade honom"},
                 {"Sir", {3, 14}, "bliva förgäten i stället", "bliva förgäten; i stället"},
                 {"Sir", {7, 20}, "helt åt din tjänst,", "helt åt din tjänst."},
                 {"Sir", {7, 23}, "allt iirån ungdomen", "allt ifrån ungdomen"},
                 {"Sir", {8, 8}, "huru du’; skall", "huru du skall"},
                 {"Sir", {8, 14}, "giva honom rätt,", "giva honom rätt."},
                 {"Sir", {10, 9}, "den som stoft och aska", "den som är stoft och aska"},
                 {"Sir", {11, 4}, "din äras dag Ty", "din äras dag. Ty"},
                 {"Sir", {11, 11}, "och avlas,", "och ävlas,"},
                 {"Sir", {11, 31}, "och lik spejaren spanar han efter ditt fall. Försåtligt vänder han gott tills ont, och på det allra bästa sätter han en fläck.", "Försåtligt vänder han gott till ont, och på det allra bästa sätter han en fläck."},
                 {"Sir", {13, 4}, "vara honörn till nytta", "vara honom till nytta"},
                 {"Sir", {13, 21}, "av sina vann er,", "av sina vänner,"},
                 {"Sir", {14, 18}, "lövverket som grönskan", "lövverket som grönskar"},
                 {"Sir", {14, 23}, "vid hennes dörrar:", "vid hennes dörrar;"},
                 {"Sir", {15, 6}, "en fröjdekrahs", "en fröjdekrans"},
                 {"Sir", {16, 8}, "som Löt bodde", "som Lot bodde"},
                 {"Sir", {17, 13}, "o,ch hans rösts", "och hans rösts"},
                 {"Sir", {17, 25}, "med att.synda;", "med att synda;"},
                 {"Sir", {19, 5}, "Den som glades över", "Den som glädes över"},
                 {"Sir", {20, 31}, "döljer sin vishet.", "döljer sin vishet,"},
                 {"Sir", {23, 3}, "mina felsteg bliva: så många", "mina felsteg bliva så många"},
                 {"Sir", {24, 19}, "och matten eder", "och mätten eder"},
                 {"Sir", {25, 4}, "icke gråhårsman att", "icke gråhårsmän att"},
                 {"Sir", {25, 8}, "får levas samman", "får leva samman"},
                 {"Sir", {26, 10}, "den till självsvåld,", "den till självsvåld."},
                 {"Sir", {28, 11}, "blodsutgjutelse åstad.", "blodsutgjutelse åstad,"},
                 {"Sir", {28, 20}, "koppar-fjättrar.", "kopparfjättrar."},
                 {"Sir", {28, 23}, "Hon slappes lös", "Hon släppes lös"},
                 {"Sir", {29, 20}, "din nästa, allt efter", "din nästa, alltefter"},
                 {"Sir", {29, 25}, "bjuda på mat. och dryck", "bjuda på mat och dryck"},
                 {"Sir", {31, 17}, "må väcka anstöt,", "må väcka anstöt."},
                 {"Sir", {31, 28}, "en glädje for sinnet", "en glädje för sinnet"},
                 {"Sir", {32, 9}, "prata icke vitt och brett,", "prata icke vitt och brett."},
                 {"Sir", {32, 17}, "finner han ut rättsgrunder,", "finner han ut rättsgrunder."},
                 {"Sir", {32, 20}, "du ej må snäva på", "du ej må snava på"},
                 {"Sir", {33, 7}, "dagen företrade framför", "dagen företräde framför"},
                 {"Sir", {33, 15}, "de té sig sammalunda", "de te sig sammalunda"},
                 {"Sir", {34, 16}, "Herren förfaras alls icke", "Herren förfäras alls icke"},
                 {"Sir", {36, 8}, "och Utgjut din vrede.", "och utgjut din vrede."},
                 {"Sir", {37, 1}, "Också jag ar vän", "Också jag är vän"},
                 {"Sir", {37, 17}, "sinnets om-skiftningar", "sinnets omskiftningar"},
                 {"Sir", {37, 27}, "vad du tål, ocn lägg", "vad du tål, och lägg"},
                 {"Sir", {39, 23}, "hans vrede, såsom, när", "hans vrede, såsom när"},
                 {"Sir", {42, 10}, "skall bliva Vanärad", "skall bliva vanärad"},
                 {"Sir", {43, 11}, "den. övermåttan skön", "den. Övermåttan skön"},
                 {"Sir", {45, 5}, "lät honörn träda", "lät honom träda"},
                 {"Sir", {46, 20}, "hade avsomnät,", "hade avsomnat,"},
                 {"Sir", {50, 10}, "med rildig frukt", "med riklig frukt"},
                 {"Sir", {51, 17}, "förkovrades Jag däri", "förkovrades jag däri"},
                 {"Sir", {51, 26}, "Bojen eder hals", "Böjen eder hals"},
                 {"Baruch", {2, 5}, "måste’ligga under’ och fingo icke’ligga över’", "måste ’ligga under’ och fingo icke ’ligga över’"},
                 {"Baruch", {2, 21}, "Bojen edra skuldror", "Böjen edra skuldror"},
                 {"Baruch", {4, 12}, "över mig, i en värnlös", "över mig, en värnlös"},
                 {"Baruch", {4, 18}, "Nej han som", "Nej, han som"},
                 {"Baruch", {4, 27}, "Men våren vid", "Men varen vid"},
                 {"Baruch", {5, 8}, "på Guds befallning,", "på Guds befallning."},
                 {"Baruch", {6, 5}, "Tanken i stället", "Tänken i stället"},
                 {"Baruch", {6, 11}, "purpmr-dräkter.", "purpurdräkter."},
                 {"Baruch", {6, 14}, "kan nian förstå", "kan man förstå"},
                 {"1Macc", {1, 10}, "till Rom Han blev konung i det grekiska konungadömets ett hundra trettio sjunde år.", "till Rom. Han blev konung i det grekiska konungadömets ett hundra trettiosjunde år."},
                 {"1Macc", {1, 39}, "dess heder till förakt,", "dess heder till förakt."},
                 {"1Macc", {2, 8}, "en Vanärad man", "en vanärad man"},
                 {"1Macc", {2, 13}, "Varförr skulle", "Varför skulle"},
                 {"1Macc", {2, 64}, "barn, våren frimodiga", "barn, varen frimodiga"},
                 {"1Macc", {2, 70}, "Han dog år 146; Och", "Han dog år 146. Och"},
                 {"1Macc", {3, 38}, "utsåg Ptolomeus,", "utsåg Ptolemeus,"},
                 {"1Macc", {3, 42}, "och hans broder sågo", "och hans bröder sågo"},
                 {"1Macc", {4, 8}, "»Frukten Icke för deras stora antal, och radens icke för deras anlopp.", "»Frukten icke för deras stora antal, och rädens icke för deras anlopp."},
                 {"1Macc", {5, 25}, "Galaaditis.’ De sade:", "Galaaditis. De sade:"},
                 {"1Macc", {5, 38}, "hos dem. en mycket", "hos dem, en mycket"},
                 {"1Macc", {6, 11}, "vid mig själv:’I vilket", "vid mig själv: ’I vilket"},
                 {"1Macc", {6, 14}, "satte honom Över", "satte honom över"},
                 {"1Macc", {6, 20}, "år 150; Och", "år 150. Och"},
                 {"1Macc", {6, 56}, "Persien och Me-dien,", "Persien och Medien,"},
                 {"1Macc", {7, 32}, "de Övriga flydde", "de övriga flydde"},
                 {"1Macc", {7, 50}, "hade nu rö en", "hade nu ro en"},
                 {"1Macc", {8, 7}, "avträdaland åt dem,’", "avträda land åt dem,"},
                 {"1Macc", {8, 30}, "sålunda till-lägga", "sålunda tillägga"},
                 {"1Macc", {8, 31}, "följande innehåll:’Varför", "följande innehåll: ’Varför"},
                 {"1Macc", {9, 10}, "fläck,på", "fläck, på"},
                 {"1Macc", {9, 33}, "Men då Jonatan, och", "Men då Jonatan och"},
                 {"1Macc", {9, 37}, "att Jambris barn. just", "att Jambris barn just"},
                 {"1Macc", {9, 58}, "sitta i rö och säkerhet", "sitta i ro och säkerhet"},
                 {"1Macc", {10, 49}, "Demetrius’ här flydde. och Alexander", "Demetrius’ här flydde, och Alexander"},
                 {"1Macc", {10, 83}, "ryttarna sking- rade sig", "ryttarna skingrade sig"},
                 {"1Macc", {11, 3}, "alltefter; som", "alltefter som"},
                 {"1Macc", {11, 18}, "nedgjor-des", "nedgjordes"},
                 {"1Macc", {11, 33}, "följande förmanér.", "följande förmåner."},
                 {"1Macc", {11, 37}, "härför'»", "härför.'»"},
                 {"1Macc", {11, 63}, "hans förehavande.’ Han", "hans förehavande. Han"},
                 {"1Macc", {11, 67}, "bröto nuu bittida", "bröto nu bittida"},
                 {"1Macc", {12, 3}, "åstad till Rom När", "åstad till Rom. När"},
                 {"1Macc", {12, 22}, "och säga oni det", "och säga om det"},
                 {"1Macc", {13, 5}, "att jags någonsin", "att jag någonsin"},
                 {"1Macc", {14, 22}, "upptecknat följande:’Numenius", "upptecknat följande: ’Numenius"},
                 {"1Macc", {14, 33}, "judar såsom be- sättning.", "judar såsom besättning."},
                 {"1Macc", {15, 23}, "Halikarnas-sus", "Halikarnassus"},
                 {"1Macc", {16, 1}, "vad Cende-beus förövade.", "vad Cendebeus förövade."},
                 {"1Macc", {16, 4}, "mot Gendebeus och", "mot Cendebeus och"},
                 {"1Macc", {16, 9}, "som Gendebeus hade befäst.", "som Cendebeus hade befäst."},
                 {"2Macc", {2, 28}, "verklige författareris sak", "verklige författarens sak"},
                 {"2Macc", {4, 1}, "omtalade Sisom hade", "omtalade Simon, som hade"},
                 {"2Macc", {5, 5}, "ett falskt ryktes hade", "ett falskt rykte hade"},
                 {"2Macc", {5, 12}, "utan försköning hugga ned dem som kommo i deras väg octi att dräpa", "utan förskoning hugga ned dem som kommo i deras väg och att dräpa"},
                 {"2Macc", {6, 21}, "skaffa dit kött som han", "skaffa dit kött, som han"},
                 {"2Macc", {7, 6}, "han förbarma sig'»", "han förbarma sig.'»"},
                 {"2Macc", {7, 30}, "Vad vanten I på?", "Vad vänten I på?"},
                 {"2Macc", {7, 34}, "förhävdig icke fåfängt", "förhäv dig icke fåfängt"},
                 {"Jer", {8, 12}, "alls icke skam", "alls icke någon skam"},
                 {"2Macc", {6, 17}, "avb rrott vilja vi nu åtgå", "avbrott vilja vi nu återgå"},
                 {"Jdt", {5, 8}, "förjagade med från", "förjagade dem från"},
                 {"Tob", {3, 4}, "smädligt orsdpråk", "smädligt ordspråk"},
                 {"Sir", {38, 17}, "såsom kan förtjänar", "såsom han förtjänar"},
                 {"2Macc", {8, 8}, "komma kunungens sak", "komma konungens sak"},
                 {"2Macc", {9, 8}, "slagen till marken låta", "slagen till marken, låta"},
                 {"2Macc", {9, 10}, "himmel ens stjärnor", "himmelens stjärnor"},
                 {"2Macc", {9, 13}, "icke ni er skulle", "icke mer skulle"},
                 {"2Macc", {10, 3}, "och fram-buro så", "och framburo så"},
                 {"2Macc", {12, 4}, "de icke förenade något misstänkt", "de icke förehade något misstänkt"},
                 {"2Macc", {14, 14}, "till Nikanor,! hopp", "till Nikanor, i hopp"},
                 {"2Macc", {15, 22}, "din ängel, O Herre", "din ängel, o Herre"},
                 {"2Macc", {15, 34}, "plats obesmittadl»", "plats obesmittad!»"},
                 {"EsthGr", {2, 6}, "misskund eller försköning", "misskund eller förskoning"},
                 {"EsthGr", {6, 7}, "äldre historiska skildringar som", "äldre historiska skildringar, som"},
                 {"Matt", {1, 19}, "utsätta henne for vanära", "utsätta henne för vanära"},
                 {"Matt", {2, 19}, "en Herrens ängel for Josef", "en Herrens ängel för Josef"},
                 {"Matt", {9, 17}, "nytt vin I gamla", "nytt vin i gamla"},
                 {"Matt", {10, 27}, "det skolen säga i ljuset", "det skolen I säga i ljuset"},
                 {"Matt", {3, 15}, "all rättfärdighet. Då", "all rättfärdighet.» Då"},
                 {"Matt", {1, 24}, "gjorde han som Herrens ängel", "gjorde han såsom Herrens ängel"},
                 {"Matt", {10, 11}, "och stannen hos honom", "och stannen så hos honom"},
                 {"Matt", {14, 20}, "blevo mätta Sedan", "blevo mätta. Sedan"},
                 {"Matt", {17, 27}, "åt dem for mig och dig", "åt dem för mig och dig"},
                 {"Matt", {18, 5}, "ett sådant barn I mitt namn", "ett sådant barn i mitt namn"},
                 {"Matt", {20, 11}, "knorrade de mot husbonden.", "knorrade de mot husbonden"},
                 {"Matt", {21, 24}, "myndighet jag gör detta».", "myndighet jag gör detta."},
                 {"Matt", {21, 42}, "byggningsmännen förkastade. den", "byggningsmännen förkastade, den"},
                 {"Matt", {23, 23}, "skriftlärde och fariséer. I skrymtare", "skriftlärde och fariséer, I skrymtare"},
                 {"Matt", {25, 46}, "dessa skola då då bort", "dessa skola då gå bort"},
                 {"Matt", {27, 47}, "som stodo där borde detta", "som stodo där hörde detta"},
                 {"Mark", {2, 28}, "Så år då Människosonen", "Så är då Människosonen"},
                 {"Mark", {3, 11}, "för honom och och ropade", "för honom och ropade"},
                 {"Mark", {3, 33}, "och sade: Vilken är min moder", "och sade: »Vilken är min moder"},
                 {"Mark", {6, 16}, "Han bar uppstått från de döda", "Han har uppstått från de döda"},
                 {"Mark", {6, 19}, "Därför hyste nu Herodes agg till honom och ville döda honom, men han hade icke makt därtill.", "Därför hyste nu Herodias agg till honom och ville döda honom, men hon hade icke makt därtill."},
                 {"Mark", {10, 19}, "tillkommer', Hedra din fader", "tillkommer', 'Hedra din fader"},
                 {"Mark", {11, 3}, "sända den tillbaka hit.»", "sända den tillbaka hit.'»"},
                 {"Mark", {13, 34}, "bjuder portvaktaren att vaka.", "bjuder portvaktaren att vaka,"},
                 {"Mark", {16, 7}, "såsom han bar sagt eder", "såsom han har sagt eder"},
                 {"Luke", {1, 70}, "sin forntida heliga profeters mun", "sina forntida heliga profeters mun"},
                 {"Luke", {1, 73}, "lovat för vår fader Abraham,", "lovat för vår fader Abraham."},
                 {"Luke", {3, 4}, "i öknen: Bereden vägen", "i öknen: 'Bereden vägen"},
                 {"Luke", {3, 14}, "Vad skola då vi göra? Han", "Vad skola då vi göra?» Han"},
                 {"Luke", {3, 16}, "tog till orda och sade: till dem alla:", "tog till orda och sade till dem alla:"},
                 {"Luke", {3, 22}, "över honom I lekamlig skepnad", "över honom i lekamlig skepnad"},
                 {"Luke", {4, 16}, "in i synagogan: och där", "in i synagogan; och där"},
                 {"Luke", {4, 25}, "på Elias' tid då himmelen", "på Elias' tid – då himmelen"},
                 {"Luke", {5, 31}, "»De är icke de friska", "»Det är icke de friska"},
                 {"Luke", {6, 2}, "Då sade några av fariséerna; »Huru kunnen I göra vad som icke är lovligt att göra på sabbaten?", "Då sade några av fariséerna: »Huru kunnen I göra vad som icke är lovligt att göra på sabbaten?»"},
                 {"Luke", {6, 20}, "och säg på sina lärjungar", "och såg på sina lärjungar"},
                 {"Luke", {6, 23}, "På samma satt gjorde", "På samma sätt gjorde"},
                 {"Luke", {6, 26}, "gjorde ju deras fader i fråga", "gjorde ju deras fäder i fråga"},
                 {"Luke", {6, 42}, "att du kan tala ut grandet", "att du kan taga ut grandet"},
                 {"Luke", {7, 7}, "värdig att komma till dig Men säg", "värdig att komma till dig. Men säg"},
                 {"Luke", {7, 16}, "och sade: Den stor profet", "och sade: »En stor profet"},
                 {"Luke", {7, 20}, "När mannen kommo fram", "När männen kommo fram"},
                 {"Luke", {7, 43}, "»Rätt dömde du»", "»Rätt dömde du.»"},
                 {"Luke", {8, 10}, "för att de med seende ögon", "för att de 'med seende ögon"},
                 {"Luke", {8, 20}, "Din moder och dina broder", "Din moder och dina bröder"},
                 {"Luke", {9, 4}, "kommit in något hus", "kommit in i något hus"},
                 {"Luke", {9, 10}, "De tog han dem med sig", "Då tog han dem med sig"},
                 {"Luke", {9, 16}, "och säg upp till himmelen", "och såg upp till himmelen"},
                 {"Luke", {9, 18}, "var stadd i byn", "var stadd i bön"},
                 {"Luke", {9, 26}, "när han kom mer i sin", "när han kommer i sin"},
                 {"Luke", {9, 30}, "Och de, två män stodo", "Och se, två män stodo"},
                 {"Luke", {9, 33}, "Mästare, har är oss gott", "Mästare, här är oss gott"},
                 {"Luke", {9, 49}, "Mästare, sågo huru en man drev ut onda andar genom ditt namn; och du ville hindra honom", "Mästare, vi sågo huru en man drev ut onda andar genom ditt namn; och vi ville hindra honom"},
                 {"Luke", {9, 54}, "lärjungarna Jakob i och Johannes", "lärjungarna Jakob och Johannes"},
                 {"Luke", {9, 59}, "Föl; mig.", "Följ mig."},
                 {"Luke", {10, 15}, "Och du. Kapernaum", "Och du, Kapernaum"},
                 {"Luke", {10, 20}, "över att änglarna äro eder underdåniga", "över att andarna äro eder underdåniga"},
                 {"Luke", {10, 22}, "blivit för trott åt mig", "blivit förtrott åt mig"},
                 {"Luke", {10, 28}, "Gör det, så får du leva,", "Gör det, så får du leva.»"},
                 {"Luke", {11, 7}, "stå upp och göra dig något", "stå upp och giva dig något"},
                 {"Luke", {11, 11}, "i stallet för en fisk", "i stället för en fisk"},
                 {"Luke", {11, 27}, "hov en kvinna in folkhopen", "hov en kvinna i folkhopen"},
                 {"Luke", {11, 28}, "saliga är de som höra", "saliga äro de som höra"},
                 {"Luke", {11, 29}, "ett ont släkte Det begär", "ett ont släkte. Det begär"},
                 {"Luke", {11, 31}, "och se, har är vad som är mer än Salomo", "och se, här är vad som är mer än Salomo"},
                 {"Luke", {12, 33}, "där man icke fördärvar", "där mal icke fördärvar"},
                 {"Luke", {12, 40}, "Så varen ock I redo ty", "Så varen ock I redo, ty"},
                 {"Luke", {12, 54}, "Han hade också till folket", "Han sade också till folket"},
                 {"Luke", {13, 9}, "till nästa å bära frukt", "till nästa år bära frukt"},
                 {"Luke", {13, 12}, "sade till henne »Kvinna, du är fri ifrån din sjuk dom»,", "sade till henne: »Kvinna, du är fri ifrån din sjukdom»,"},
                 {"Luke", {13, 34}, "Men haven icke velat.", "Men I haven icke velat."},
                 {"Luke", {13, 35}, "förrän den tid kommen, då I sägen: 'Välsignad vare han som kommer, i Herrens namn.'", "förrän den tid kommer, då I sägen: 'Välsignad vare han som kommer, i Herrens namn.'»"},
                 {"Luke", {14, 22}, "men har är ännu rum", "men här är ännu rum"},
                 {"Luke", {14, 24}, "skall smaka sin måltid", "skall smaka min måltid"},
                 {"Luke", {14, 28}, "kostnaden och mer till", "kostnaden och ser till"},
                 {"Luke", {15, 14}, "kom en svär hungersnöd", "kom en svår hungersnöd"},
                 {"Luke", {15, 17}, "medan jag har förgås av hunger", "medan jag här förgås av hunger"},
                 {"Luke", {15, 18}, "och säga till honom: Fader,", "och säga till honom: 'Fader,"},
                 {"Luke", {15, 22}, "Då sade fadern till sina tjänare '", "Då sade fadern till sina tjänare: '"},
                 {"Luke", {15, 23}, "så vilja vi äta och gör oss glada", "så vilja vi äta och göra oss glada"},
                 {"Luke", {15, 28}, "och talade vanligt med honom", "och talade vänligt med honom"},
                 {"Luke", {15, 29}, "och lik väl har du åt mig", "och likväl har du åt mig"},
                 {"Luke", {16, 26}, "över till oss.»", "över till oss.'"},
                 {"Luke", {17, 3}, "Om din broder försyndat sig", "Om din broder försyndar sig"},
                 {"Luke", {17, 16}, "på mitt ansikte för Jesu fötter", "på sitt ansikte för Jesu fötter"},
                 {"Luke", {17, 24}, "på hand dag", "på hans dag"},
                 {"Luke", {18, 14}, "rättfärdig mer an den andre", "rättfärdig mer än den andre"},
                 {"Luke", {18, 30}, "som icke skall mångfaldigt igen", "som icke skall få mångfaldigt igen"},
                 {"Luke", {19, 13}, "till sig tio sin tjänare", "till sig tio sina tjänare"},
                 {"Luke", {19, 14}, "denne skall bliva konung över oss.»", "denne skall bliva konung över oss.'"},
                 {"Luke", {19, 27}, "fören dem hit huggen ned dem", "fören dem hit, och huggen ned dem"},
                 {"Luke", {20, 2}, "har rivit dig sådan myndighet", "har givit dig sådan myndighet"},
                 {"Luke", {20, 9}, "och för utrikes för lång tid", "och for utrikes för lång tid"},
                 {"Luke", {20, 14}, "Men när vingårdsmannen fingo se honom", "Men när vingårdsmännen fingo se honom"},
                 {"Luke", {20, 28}, "hustru till akta", "hustru till äkta"},
                 {"Luke", {20, 35}, "varken män tag sig hustrur, eller hustrur givas män", "varken män taga sig hustrur, eller hustrur givas åt män"},
                 {"Luke", {20, 36}, "mer dö ty de äro lika änglarna och äro, Guds söner", "mer dö; ty de äro lika änglarna och äro Guds söner"},
                 {"Luke", {20, 42}, "Psalmernas bok: Herren sade till min herre:", "Psalmernas bok: 'Herren sade till min herre:"},
                 {"Luke", {20, 47}, "hålla långa baner. Del skola", "hålla långa böner. De skola"},
                 {"Luke", {21, 25}, "i solen och månen och i stjärnorna", "i solen och i månen och i stjärnorna"},
                 {"Luke", {22, 7}, "i det osyrad brödets högtid", "i det osyrade brödets högtid"},
                 {"Luke", {22, 9}, "De frågade honom: Var vill du", "De frågade honom: »Var vill du"},
                 {"Luke", {22, 10}, "Han svarade dem: När I kommen", "Han svarade dem: »När I kommen"},
                 {"Luke", {22, 37}, "det går nu i fullbordan»", "det går nu i fullbordan.»"},
                 {"Luke", {22, 53}, "händer emot mig men detta", "händer emot mig; men detta"},
                 {"Luke", {22, 56}, "där han satt vid elden fäste ögonen på honom och sade: »Också denne var med honom.", "där han satt vid elden, fäste ögonen på honom och sade: »Också denne var med honom.»"},
                 {"Luke", {22, 58}, "en av mannen, se honom", "en av männen, se honom"},
                 {"Luke", {23, 3}, "och sade: Är du judarnas konung?»", "och sade: »Är du judarnas konung?»"},
                 {"Luke", {23, 7}, "under dessa dalar också", "under dessa dagar också"},
                 {"Luke", {23, 22}, "som förtjänar döden Därför vill jag", "som förtjänar döden. Därför vill jag"},
                 {"Luke", {23, 30}, "säga till: bergen", "säga till bergen"},
                 {"Luke", {23, 34}, "Men Jesus sade. »Fader, förlåt dem; ty de veta icke vad de göra.", "Men Jesus sade: »Fader, förlåt dem; ty de veta icke vad de göra.»"},
                 {"Luke", {23, 50}, "en rådsherre, vi namn Josef", "en rådsherre, vid namn Josef"},
                 {"Luke", {24, 4}, "då stodo två man framför dem", "då stodo två män framför dem"},
                 {"Luke", {24, 5}, "Då sade mannen till dem »", "Då sade männen till dem: »"},
                 {"Luke", {24, 6}, "Han är icke har, han är uppstånden.", "Han är icke här, han är uppstånden."},
                 {"Luke", {24, 26}, "för I att så ingå", "för att så ingå"},
                 {"John", {1, 40}, "hört var Johannes sade", "hört vad Johannes sade"},
                 {"John", {3, 12}, "Tron i icke, när jag talar", "Tron I icke, när jag talar"},
                 {"John", {3, 19}, "eftersom deras gärningar voro onda,", "eftersom deras gärningar voro onda."},
                 {"John", {3, 27}, "givet från himmelen.»", "givet från himmelen."},
                 {"John", {4, 17}, "att du icke har någon man.»", "att du icke har någon man."},
                 {"John", {4, 34}, "och att fullborda hans verk.»", "och att fullborda hans verk."},
                 {"John", {4, 40}, "både de honom att stanna", "bådo de honom att stanna"},
                 {"John", {5, 12}, "att du skulle taga sin säng", "att du skulle taga din säng"},
                 {"John", {5, 23}, "såsom de ära Faderns.", "såsom de ära Fadern."},
                 {"John", {5, 25}, "Sannerligen säger jag eder: Den stund kommer, ja, den är redan inne, så de döda", "Sannerligen, sannerligen säger jag eder: Den stund kommer, ja, den är redan inne, då de döda"},
                 {"John", {5, 33}, "och han har vittnat för sanningen,", "och han har vittnat för sanningen."},
                 {"John", {6, 14}, "Då nu människorna hade det tecken", "Då nu människorna hade sett det tecken"},
                 {"John", {8, 21}, "Dig jag går, dit kunnen I", "Dit jag går, dit kunnen I"},
                 {"John", {8, 28}, "då skolen I första att", "då skolen I förstå att"},
                 {"John", {8, 36}, "så bliven i verkligen fria", "så bliven I verkligen fria"},
                 {"John", {8, 44}, "det viljen i göra", "det viljen I göra"},
                 {"John", {8, 54}, "han som I säger vara eder Gud", "han som I sägen vara eder Gud"},
                 {"John", {9, 8}, "den man som att och tiggde", "den man som satt och tiggde"},
                 {"John", {9, 18}, "till sig mannen föräldrar", "till sig mannens föräldrar"},
                 {"John", {9, 20}, "Då svarade han föräldrar", "Då svarade hans föräldrar"},
                 {"John", {11, 20}, "Då nu Maria fick höra att Jesus kom", "Då nu Marta fick höra att Jesus kom"},
                 {"John", {11, 56}, "där de stodo i helgedom:", "där de stodo i helgedomen:"},
                 {"John", {12, 30}, "utan för eder skull.»", "utan för eder skull."},
                 {"John", {12, 42}, "för fariséernas skulle ville", "för fariséernas skull ville"},
                 {"John", {13, 13}, "och I säger rätt", "och I sägen rätt"},
                 {"John", {15, 8}, "att i bären mycken frukt", "att I bären mycken frukt"},
                 {"John", {16, 20}, "säger jag eder: I skolen bliva bedrövade", "säger jag eder: I skolen komma att gråta och jämra eder, men världen skall glädja sig; I skolen bliva bedrövade"},
                 {"John", {16, 32}, "redan kommen, så I skolen förskingras", "redan kommen, då I skolen förskingras"},
                 {"John", {16, 33}, "I världen liden i betryck", "I världen liden I betryck"},
                 {"John", {17, 8}, "har jag givit åt dem: och de", "har jag givit åt dem; och de"},
                 {"John", {17, 13}, "Nu går jag till tid; dock", "Nu går jag till dig; dock"},
                 {"John", {19, 20}, "låg nära staden: och den var", "låg nära staden; och den var"},
                 {"John", {19, 29}, "Där stod då en kärl", "Där stod då ett kärl"},
                 {"John", {19, 34}, "stack upp han sida med ett spjut", "stack upp hans sida med ett spjut"},
                 {"John", {20, 5}, "när han lutade sig ditin, så han linnebindlarna", "när han lutade sig ditin, såg han linnebindlarna"},
                 {"John", {20, 26}, "och stod mitt ibland de, och sade", "och stod mitt ibland dem, och sade"},
                 {"John", {20, 27}, "Räck hit dit finger", "Räck hit ditt finger"},
                 {"Acts", {2, 41}, "togo emot hans ort", "togo emot hans ord"},
                 {"Acts", {2, 47}, "allt folket vad dem", "allt folket var dem"},
                 {"Acts", {3, 21}, "de tider nå allt", "de tider då allt"},
                 {"Acts", {4, 7}, "Av vilken makt eller i genom", "Av vilken makt eller genom"},
                 {"Acts", {4, 19}, "eder mer är Gud", "eder mer än Gud"},
                 {"Acts", {4, 30}, "du uträcker din hand", "du utsträcker din hand"},
                 {"Acts", {4, 32}, "utan du hade allting", "utan de hade allting"},
                 {"Acts", {5, 24}, "visst de icke", "visste de icke"},
                 {"Acts", {5, 25}, "berättade för den:", "berättade för dem:"},
                 {"Acts", {7, 4}, "efter han faders död", "efter hans faders död"},
                 {"Acts", {7, 13}, "fick kunskap och Josefs släkt", "fick kunskap om Josefs släkt"},
                 {"Acts", {7, 30}, "när fyrtio är äter voro", "när fyrtio år åter voro"},
                 {"Acts", {7, 38}, "Det var och han,", "Det var ock han,"},
                 {"Acts", {9, 39}, "Dorkas hade gjord,", "Dorkas hade gjort,"},
                 {"Acts", {9, 41}, "levande framför den.", "levande framför dem."},
                 {"Acts", {10, 32}, "ock kallas Petrus: han", "ock kallas Petrus; han"},
                 {"Acts", {12, 8}, "Och han gjorde så. därefter", "Och han gjorde så. Därefter"},
                 {"Acts", {13, 25}, "lösa av han fötter", "lösa av hans fötter"},
                 {"Acts", {13, 38}, "veta, mena bröder", "veta, mina bröder"},
                 {"Acts", {13, 42}, "bad men dem", "bad man dem"},
                 {"Acts", {15, 35}, "jämte många andra förkunnade", "jämte många andra, förkunnade"},
                 {"Acts", {16, 17}, "Denne följde efter Paulus", "Denna följde efter Paulus"},
                 {"Acts", {16, 20}, "fört dem tid fram", "fört dem dit fram"},
                 {"Acts", {17, 16}, "Men, Paulus nu väntade", "Medan Paulus nu väntade"},
                 {"Acts", {18, 25}, "Denne sade blivit undervisad", "Denne hade blivit undervisad"},
                 {"Acts", {19, 3}, "Johannes' döpelse»", "Johannes' döpelse.»"},
                 {"Acts", {19, 25}, "»I man, I veten", "»I män, I veten"},
                 {"Acts", {19, 37}, "ej heller smäda vår gudinna,", "ej heller smäda vår gudinna."},
                 {"Acts", {20, 13}, "fara land vägen.", "fara landvägen."},
                 {"Acts", {20, 18}, "umgåtts med eder:", "umgåtts med eder;"},
                 {"Acts", {22, 11}, "icke mer kunde se togo", "icke mer kunde se, togo"},
                 {"Acts", {22, 12}, "en efter lagen fram man", "en efter lagen from man"},
                 {"Acts", {22, 21}, "Då sade han till mig: Gå;", "Då sade han till mig: 'Gå;"},
                 {"Acts", {22, 28}, "Översten sade då: Mig", "Översten sade då: »Mig"},
                 {"Acts", {22, 30}, "av judarna, låt han", "av judarna, lät han"},
                 {"Acts", {25, 24}, "som ären har tillstädes", "som ären här tillstädes"},
                 {"Acts", {25, 26}, "rannsakning har; blivit", "rannsakning har blivit"},
                 {"Acts", {26, 14}, "förföljer du mig'?", "förföljer du mig?"},
                 {"Acts", {26, 24}, "på detta satt försvarade", "på detta sätt försvarade"},
                 {"Acts", {27, 10}, "»I mån, jag ser", "»I män, jag ser"},
                 {"Acts", {28, 27}, "helade av mig'", "helade av mig.'"},
                 {"Acts", {28, 28}, "hedningarna bar denna", "hedningarna har denna"},
                 {"Rom", {2, 14}, "sig själv en lag", "sig själva en lag"},
                 {"Rom", {8, 13}, "Ty om i leven", "Ty om I leven"},
                 {"Rom", {10, 15}, "Så är och skrivet:", "Så är ock skrivet:"},
                 {"Rom", {16, 23}, "Kvartus hälsar eder.", "Kvartus hälsa eder."},
                 {"Sir", {5, 12}, "handen på munnen,", "handen på munnen."}}) {
            const auto text = corpus.verse("sv1917", correction.book, correction.ref)->text;
            check(text.find(correction.after) != std::string::npos &&
                      text.find(correction.before) == std::string::npos,
                  "scan-verified erratum in " + correction.book);
        }
        check(corpus.verse("sv1917", "Acts", {2, 35})->text.ends_with("dig till en fotapall.'"),
              "Acts 2:35 retains the scan-verified inner quotation ending");
        check(corpus.verse("sv1917", "Acts", {7, 34})->text.ends_with("sända dig till Egypten.'"),
              "Acts 7:34 retains the scan-verified inner quotation ending");
        check(corpus.verse("sv1917", "Acts", {17, 7})->text.ends_with("Jesus, är konung.»"),
              "Acts 17:7 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {19, 13})->text.ends_with("som Paulus predikar.»"),
              "Acts 19:13 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {19, 29})->text.ends_with("Paulus' följeslagare."),
              "Acts 19:29 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {20, 9})->text.ends_with("var han död."),
              "Acts 20:9 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {21, 32})->text.ends_with("upphörde de att slå Paulus."),
              "Acts 21:32 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {27, 34})->text.ends_with("ett huvudhår gå förlorat.»"),
              "Acts 27:34 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Acts", {28, 22})->text.ends_with("allestädes mötes med gensägelse.»"),
              "Acts 28:22 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {3, 9})->text.ends_with("och kastat på elden.»"),
              "Luke 3:9 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {7, 40})->text.ends_with("»Mästare, säg det.»"),
              "Luke 7:40 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {8, 44})->text.ends_with("strax stannade hennes blodgång."),
              "Luke 8:44 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {9, 44})->text.ends_with("bliva överlämnad i människors händer.»"),
              "Luke 9:44 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {9, 48})->text.ends_with("han är störst.»"),
              "Luke 9:48 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {9, 57})->text.ends_with("Jag vill följa dig, varthelst du går.»"),
              "Luke 9:57 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {10, 1})->text.ends_with("dit han själv tänkte komma."),
              "Luke 10:1 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {12, 19})->text.ends_with("Sedan vill jag säga till min själ: 'Kära själ, du har mycket gott förvarat för många år; giv dig nu ro, ät, drick och var glad.'"),
              "Luke 12:19 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {12, 53})->text.ends_with("sonhustrun mot sin svärmoder.»"),
              "Luke 12:53 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {13, 14})->text.ends_with("men icke på sabbatsdagen.»"),
              "Luke 13:14 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {15, 10})->text.ends_with("en enda syndare som gör bättring.»"),
              "Luke 15:10 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {19, 8})->text.ends_with("så giver jag fyradubbelt igen.»"),
              "Luke 19:8 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Luke", {24, 36})->text.ends_with("»Frid vare med eder.»"),
              "Luke 24:36 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "John", {4, 18})->text.ends_with("däri sade du sant.»"),
              "John 4:18 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "John", {7, 49})->text.ends_with("det är förbannat.»"),
              "John 7:49 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "John", {8, 18})->text.ends_with("Fadern, som har sänt mig.»"),
              "John 8:18 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "John", {8, 47})->text.ends_with("som I icke lyssnen därtill.»"),
              "John 8:47 retains the scan-verified verse ending");
        check(corpus.verse("sv1917", "Mark", {10, 12})->text.ends_with("då begår hon äktenskapsbrott.»"),
              "Mark 10:12 closes Jesus' reply before the following narration");
        check(corpus.verse("sv1917", "Matt", {4, 24})->text.find("hemsökta av olika slags lidanden") != std::string::npos,
              "Matthew 4:24 retains olika as confirmed by supplementary print photograph V008");
        check(corpus.verse("sv1917", "Matt", {13, 15})->text.ends_with("helade av mig.'") &&
                  corpus.verse("sv1917", "Matt", {13, 27})->text.ends_with("fått ogräs?'"),
              "Matthew 13:15 and 13:27 retain the scan-verified closing quotation marks");
        check(corpus.verse("sv1917", "1Macc", {10, 27})->text == "Fortfaren nu ock allt framgent att iakttaga trohet mot oss, så skola vi bereda eder fördelar, till tack för vad I gören till vårt gagn;" &&
                  corpus.verse("sv1917", "1Macc", {10, 28})->text == "vi skola bevilja eder många eftergifter och tilldela eder många förmåner.",
              "First Maccabees 10:27-28 follow the printed boundary without losing or duplicating words");
        for (const VerseRef& ref : {VerseRef{10, 27}, VerseRef{10, 28}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "1Macc", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "1Macc", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "corrected First Maccabees verses align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "2Macc", {1, 1})->text.starts_with("»De judiska bröderna i Jerusalem") &&
                  corpus.verse("sv1917", "2Macc", {2, 1})->text.starts_with("»Man finner i urkunderna"),
              "Second Maccabees preserves the printed opening quotation marks in both letter sections");
        check(corpus.verse("sv1917", "Wis", {14, 20})->text.starts_with("och så begynte mängden, som fängslades genom") &&
                  corpus.verse("sv1917", "Wis", {16, 8})->text.ends_with("att du är den som frälsar ifrån allt ont."),
              "Wisdom verse boundaries retain their complete opening and closing phrases");
        check(corpus.verse("sv1917", "Tob", {11, 19})->text ==
                  "och Tobias’ bröllop firades med gamman i sju dagar." &&
                  corpus.verse("sv1917", "Tob", {13, 18})->text ==
                  "och alla dess gränder skola säga: »Halleluja», och lova Gud: »Lovad vare Gud evinnerligen, han som har upphöjt dig!»",
              "Tobit restores separate verse records for 11:19 and 13:18");
        for (const VerseRef& ref : {VerseRef{11, 18}, VerseRef{11, 19}, VerseRef{13, 17}, VerseRef{13, 18}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Tob", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Tob", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "restored Tobit verses align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Tob", {4, 19})->text.find("efter som honom behagar") != std::string::npos,
              "Tobit's confirmed printed efter som is preserved");
        check(!corpus.verse("sv1917", "Sir", {3, 25}) &&
                  corpus.verse("sv1917", "Sir", {3, 26})->text.starts_with("Ett förmätet hjärta går det på sistone illa,") &&
                  corpus.verse("sv1917", "Sir", {3, 27})->text.starts_with("Ett förmätet hjärta bliver nedtyngt av vedermödor,"),
              "Sirach 3:26-27 retain the printed numbering without inventing 3:25");
        for (const VerseRef& ref : {VerseRef{3, 26}, VerseRef{3, 27}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Sir", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Sir", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "renumbered Sirach verses align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Sir", {11, 30})->text.ends_with("och lik spejaren spanar han efter ditt fall.") &&
                  corpus.verse("sv1917", "Sir", {12, 2})->text.ends_with("så dock av den Högste.") &&
                  corpus.verse("sv1917", "Sir", {12, 3})->text == "Välgärningar må ej bevisas den som framhärdar i det onda eller den som icke själv övar välgörenhet." &&
                  corpus.verse("sv1917", "Sir", {12, 4})->text == "Giv åt den som är god, men tag dig ej an syndaren.",
              "Sirach 11:30-31 and 12:2-4 follow the printed verse boundaries");
        for (const VerseRef& ref : {VerseRef{12, 2}, VerseRef{12, 3}, VerseRef{12, 4}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Sir", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Sir", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "restored Sirach 12:2-4 align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Sir", {13, 15})->text == "Vart levande väsen älskar sin like, och var människa den som står henne närmast." &&
                  corpus.verse("sv1917", "Sir", {13, 16})->text == "Alla varelser sluta sig samman efter sina släkten; så bör ock en man hålla sig till sin like.",
              "Sirach 13:15-16 retain individual verses without the stray OCR colon");
        for (const VerseRef& ref : {VerseRef{13, 15}, VerseRef{13, 16}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Sir", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Sir", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "restored Sirach 13:15-16 align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Sir", {40, 15})->text == "De ogudaktigas avkomlingar skjuta icke grenar i mängd, och orena rotskott växa på kala klippan." &&
                  corpus.verse("sv1917", "Sir", {40, 16})->text == "De likna vassen vid var vattensamling och vid var flodbrädd, vilken ryckes bort före allt annat gräs.",
              "Sirach 40:15-16 retain the printed verse boundary");
        for (const VerseRef& ref : {VerseRef{40, 15}, VerseRef{40, 16}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Sir", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Sir", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "restored Sirach 40:15-16 align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Sir", {47, 4})->text == "Slog han icke i sin ungdom ihjäl jätten och tog så bort försmädelsen ifrån folket, i det han lyfte sin hand med slungstenen och slog Goljats stolta skryt till marken?" &&
                  corpus.verse("sv1917", "Sir", {47, 5})->text == "Ja, han åkallade Herren, den Högste, och Herren lade styrka i hans högra hand, till att nedgöra den starke kämpen och till att upphöja hans folks horn.",
              "Sirach 47:4-5 retain the printed boundary and den reading");
        for (const VerseRef& ref : {VerseRef{47, 4}, VerseRef{47, 5}}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Sir", ref);
                const auto parallel = corpus.parallel_verse("sv1917", source, "Sir", ref);
                check(swedish && swedish->ref == ref && swedish->last.value_or(swedish->ref) == ref &&
                          parallel && parallel->ref == ref && parallel->last.value_or(parallel->ref) == ref,
                      "restored Sirach 47:4-5 align individually in both directions");
            }
        }
        check(corpus.verse("sv1917", "Baruch", {3, 37})->text == "Han utrönte all vishetens väg och gav henne åt Jakob, sin tjänare, och åt Israel, sin älskade." &&
                  corpus.verse("sv1917", "Baruch", {3, 38})->text == "Därefter blev hon skådad på jorden och hade sin umgängelse bland människorna.",
              "Baruch 3:37-38 retain the printed verse boundary");
        for (int verse : {37, 38}) {
            for (const std::string source : {"grc-lxx", "en-lxx2012"}) {
                const VerseRef swedish_ref{3, verse};
                const VerseRef other_ref{3, verse - 1};
                const auto swedish = corpus.parallel_verse(source, "sv1917", "Baruch", other_ref);
                const auto other = corpus.parallel_verse("sv1917", source, "Baruch", swedish_ref);
                check(swedish && swedish->ref == swedish_ref && swedish->last.value_or(swedish->ref) == swedish_ref &&
                          other && other->ref == other_ref && other->last.value_or(other->ref) == other_ref,
                      "restored Baruch verses align individually with the numbering offset");
            }
        }
        check(corpus.verse("sv1917", "Jdt", {5, 3})->text.find("och hvad för städer") != std::string::npos,
              "Judith's confirmed historical spelling hvad is preserved");
        check(corpus.verse("sv1917", "Hag", {2, 16})->text.ends_with("till HERRENS tempel;"),
              "Haggai's sentence continues across the verse boundary with a semicolon");
        check(corpus.verse("sv1917", "PrAzar", {1, 42})->text.ends_with("i evighet."),
              "the printed final period is restored in the song of the three men");
        check(corpus.verse("sv1917", "2Macc", {7, 11})->text.find("för Hans lags skull") != std::string::npos,
              "Second Maccabees preserves the confirmed capital Hans");
        check(
            make_utterance("Och förlåten i templet rämnade. Vilkens överträdelse är förlåten.", "sv", lexicon)
                    .speech_text ==
                "Och ⟦f'Y:l.,o:tE-n⟧ i templet rämnade. Vilkens överträdelse är förlåten.",
            "the veil is a phrase; forgiven is the voice's reading");
        check(make_utterance("Han hov upp sin röst i ditt hov.", "sv", lexicon).speech_text ==
                  "⟦h'an⟧ ⟦h'u:v⟧ upp sin röst i ditt ⟦h'o:v⟧.",
              "hov is häva's past tense, except a court");
        check(make_utterance("Intet bete för sina får; han skulle bete sig vanvettigt.", "sv", lexicon)
                      .speech_text == "Intet bete för sina får; ⟦h'an⟧ skulle ⟦bE-t'e:⟧ sig vanvettigt.",
              "bete is pasture, except the verb bete sig");
        check(make_utterance("Av Dans stam, under sång och dans.", "sv", lexicon).speech_text ==
                  "Av ⟦d'A:ns⟧ stam, under sång och ⟦d'ans⟧.",
              "Dans is the tribe before its noun, otherwise a dance");
        {
            // A review preview or correction of the word applies outside the phrases.
            auto preview = lexicon;
            preview.push_back({"sv", "förlåten", "förlåtet", "", 2000});
            check(make_utterance("förlåten i templet, innanför förlåten; förlåten", "sv", preview)
                          .speech_text == "⟦f'Y:l.,o:tE-n⟧ i templet, innanför ⟦f'Y:l.,o:tE-n⟧; förlåtet",
                  "phrases outrank per-word corrections");
        }
        {
            // Every sv1917 occurrence of each word, with the verses of its first reading.
            struct Homograph {
                std::string word, first, second;
                int occurrences;
                std::set<std::string> verses;
                // Verses with both readings: how many take the first.
                std::map<std::string, int> mixed = {};
            };
            const std::string veil = "⟦f'Y:l.,o:tE-n⟧";
            const std::vector<Homograph> homographs{
                {"förlåten",
                 "förlåten",
                 veil,
                 42,
                 {"Luke 6:37", "2Cor 2:10", "Acts 8:22", "Col 3:13", "Deut 21:8", "Eph 4:32", "Isa 33:24",
                  "John 20:23", "Lev 19:22", "Mark 11:25", "Matt 12:31", "Matt 18:35", "Matt 6:14",
                  "Matt 6:15", "Ps 32:1"}},
                {"hov", "⟦h'o:v⟧", "⟦h'u:v⟧", 24, {"Dan 2:49", "Ps 45:10"}},
                {"bete", "⟦bE-t'e:⟧", "bete", 17, {"1Sam 1:14", "1Sam 21:15", "2Kgs 7:9", "Hos 3:3"}},
                {"dans",
                 "⟦d'ans⟧",
                 "⟦d'A:ns⟧",
                 48,
                 {"1Sam 18:6", "Exod 15:20", "Jdt 15:12", "Jdt 15:13", "Jdt 3:7", "Jer 31:13", "Jer 31:4",
                  "Judg 11:34", "Lam 5:15", "Luke 15:25", "Ps 149:3", "Ps 150:4", "Ps 87:7"}},
                {"hos", "", "⟦h'Us⟧", 760, {}},
                {"vikt", "", "⟦v'Ikt⟧", 61, {}},
                {"vänt", "", "⟦v'Ent⟧", 44, {}},
                {"rest", "", "⟦R'e:st⟧", 17, {}},
                {"tomt", "", "⟦t'Umt⟧", 14, {}},
                {"predikat", "", "⟦pRE-d'i:kat⟧", 13, {}},
                {"förleder", "", "⟦fWl.'e:dE-R⟧", 5, {}},
                {"förena", "", "⟦fWR'e:na⟧", 4, {}},
                {"kista", "", "⟦S;'Ista⟧", 4, {}},
                {"ko", "", "⟦k'u:⟧", 4, {}},
                {"svans", "", "⟦sv'ans⟧", 4, {}},
                {"duken", "", "⟦d'u-kE-n⟧", 3, {}},
                {"kosta", "", "⟦k'Osta⟧", 3, {}},
                {"logen", "", "⟦l'u:gE-n⟧", 3, {}},
                {"rene", "", "⟦R'e:nE-⟧", 3, {}},
                {"saks", "", "⟦s'A:ks⟧", 3, {}},
                {"synts", "", "⟦s'y:nts⟧", 3, {}},
                {"ton", "", "⟦t'u:n⟧", 3, {}},
                {"eggen", "", "⟦'EgE-n⟧", 2, {}},
                {"klor", "", "⟦kl'o:R⟧", 2, {}},
                {"menat", "", "⟦m'e:nat⟧", 2, {}},
                {"demon", "", "⟦dE-m'o:n⟧", 1, {}},
                {"korsar", "", "⟦k'Os.aR⟧", 1, {}},
                {"makes", "", "⟦m'A:kE-s⟧", 1, {}},
                {"mats", "", "⟦m'A:ts⟧", 1, {}},
                {"murat", "", "⟦m'u-Rat⟧", 1, {}},
                {"ron", "", "⟦R'u:n⟧", 1, {}},
                {"tima", "", "⟦t'i:ma⟧", 1, {}},
                {"timat", "", "⟦t'i:mat⟧", 1, {}},
                {"trumpet", "", "⟦tR8mp'e:t⟧", 1, {}},
                {"van", "", "⟦v'A:n⟧", 1, {}},
                {"åse", "", "⟦'o:s,e:⟧", 1, {}},
                {"öst", "", "⟦'Y:st⟧", 1, {}},
                {"skylt", "", "⟦sx'y:lt⟧", 3, {}},
                {"fördraget", "", "⟦fWRdR'A:gE-t⟧", 4, {}},
                {"joses", "", "⟦j'u:sE-s⟧", 4, {}},
                {"jo", "", "⟦j'u:⟧", 51, {}},
                {"vi", "", "⟦v'i:⟧", 2334, {}},
                {"krubba", "", "⟦kR'8ba⟧", 4, {}},
                {"krubban", "", "⟦kR'8ban⟧", 3, {}},
                {"jordan", "", "⟦jURd'A:n⟧", 187, {}},
                {"jordans", "", "⟦jURd'A:ns⟧", 12, {}},
                {"betsaida", "", "⟦bEts'ajda⟧", 7, {}},
                {"tattenai", "", "⟦tatEn'A:I⟧", 4, {}},
                {"sefarvaims", "", "⟦sEfaRv'A:Ims⟧", 5, {}},
                {"spetälsk", "", "⟦spE-t'Elsk⟧", 12, {}},
                {"spetälska", "", "⟦spE-t'Elska⟧", 48, {}},
                {"spetälskan", "", "⟦spE-t'Elskan⟧", 5, {}},
                {"välan", "", "⟦vEl'an⟧", 36, {}},
                {"välsignen", "", "⟦vEls'INnE-n⟧", 9, {}},
                {"jerobeam", "", "⟦jER'u:bEam⟧", 55, {}},
                {"rehabeam", "", "⟦REh'A:bEam⟧", 47, {}},
                {"väl",
                 "⟦v'E:l⟧",
                 "⟦v'El⟧",
                 649,
                 {"1Chr 12:32",  "1Cor 11:6",   "1Cor 5:6",    "1Cor 7:1",    "1Cor 7:26",   "1Cor 7:37",
                  "1Cor 7:38",   "1Cor 7:8",    "1Kgs 8:18",   "1Macc 10:19", "1Macc 11:43", "1Macc 12:22",
                  "1Macc 14:4",  "1Macc 14:9",  "1Macc 4:55",  "1Macc 4:7",   "1Macc 6:60",  "1Macc 6:62",
                  "1Macc 8:21",  "1Macc 8:23",  "1Sam 17:18",  "1Sam 26:16",  "1Thess 5:3",  "1Tim 3:12",
                  "1Tim 3:13",   "1Tim 3:2",    "1Tim 3:4",    "2Chr 13:12",  "2Chr 14:7",   "2Chr 26:5",
                  "2Chr 30:22",  "2Chr 31:21",  "2Chr 6:8",    "2Chr 7:11",   "2Cor 10:15",  "2Cor 11:4",
                  "2Cor 6:9",    "2Cor 9:13",   "2Kgs 10:30",  "2Kgs 25:24",  "2Kgs 2:17",   "2Kgs 8:11",
                  "2Macc 11:21", "2Macc 11:26", "2Macc 11:28", "2Macc 11:33", "2Macc 11:38", "2Macc 12:13",
                  "2Macc 13:3",  "2Macc 15:17", "2Macc 15:38", "2Macc 15:39", "2Macc 2:16",  "2Macc 3:6",
                  "2Macc 8:9",   "2Macc 9:20",  "2Macc 9:27",  "2Peter 1:19", "2Sam 11:7",   "2Sam 18:28",
                  "2Sam 18:29",  "2Sam 18:32",  "2Sam 20:9",   "2Sam 3:36",   "2Tim 2:24",   "2Tim 3:17",
                  "3John 1:2",   "3John 1:6",   "Acts 10:33",  "Acts 15:29",  "Acts 19:15",  "Acts 20:24",
                  "Acts 24:22",  "Acts 25:10",  "Acts 26:3",   "Acts 27:12",  "Acts 2:47",   "Baruch 6:63",
                  "Col 1:23",    "Col 4:5",     "Dan 11:36",   "Dan 8:12",    "Dan 8:24",    "Dan 8:25",
                  "Deut 10:13",  "Deut 12:25",  "Deut 12:28",  "Deut 19:13",  "Deut 22:7",   "Deut 29:19",
                  "Deut 2:4",    "Deut 4:40",   "Deut 4:9",    "Deut 5:16",   "Deut 5:29",   "Deut 5:33",
                  "Deut 6:18",   "Deut 6:24",   "Deut 6:3",    "Deut 9:21",   "Eccl 10:17",  "Eccl 7:4",
                  "Eccl 8:12",   "Eccl 8:13",   "Eph 5:16",    "Eph 6:3",     "Esth 5:9",    "EsthGr 4:13",
                  "EsthGr 6:17", "Exod 1:20",   "Ezek 13:10",  "Ezek 13:16",  "Ezek 17:10",  "Ezek 17:15",
                  "Ezek 17:9",   "Ezra 7:6",    "Ezra 8:22",   "Gal 5:7",     "Gen 12:13",   "Gen 12:16",
                  "Gen 29:6",    "Gen 37:14",   "Gen 39:23",   "Gen 39:3",    "Gen 40:14",   "Gen 43:27",
                  "Gen 43:28",   "Gen 45:16",   "Isa 25:6",    "Isa 28:11",   "Isa 3:10",    "James 2:8",
                  "Jdt 10:16",   "Jdt 11:22",   "Jdt 11:4",    "Jdt 5:17",    "Jer 12:1",    "Jer 15:11",
                  "Jer 15:5",    "Jer 22:15",   "Jer 22:16",   "Jer 22:21",   "Jer 23:17",   "Jer 29:7",
                  "Jer 32:39",   "Jer 38:20",   "Jer 40:9",    "Jer 42:19",   "Jer 42:6",    "Jer 44:17",
                  "Jer 4:10",    "Jer 6:14",    "Jer 7:23",    "Jer 8:11",    "Job 13:9",    "Job 34:16",
                  "Job 34:9",    "Job 5:27",    "Josh 24:20",  "Josh 6:18",   "Judg 3:25",   "Judg 9:16",
                  "Lam 1:5",     "Lev 19:17",   "Lev 24:22",   "Luke 15:29",  "Luke 4:10",   "Luke 4:24",
                  "Luke 6:26",   "Luke 6:38",   "Mal 2:15",    "Mal 2:16",    "Mal 3:4",     "Mark 12:28",
                  "Mark 7:37",   "Micah 3:5",   "Neh 2:20",    "Neh 4:12",    "Num 14:41",   "Num 9:14",
                  "Phil 4:14",   "Phil 4:18",   "PrAzar 1:7",  "Prov 11:1",   "Prov 11:10",  "Prov 11:14",
                  "Prov 11:17",  "Prov 11:20",  "Prov 12:22",  "Prov 14:35",  "Prov 15:8",   "Prov 16:13",
                  "Prov 16:7",   "Prov 20:18",  "Prov 22:29",  "Prov 23:1",   "Prov 24:25",  "Prov 24:6",
                  "Prov 27:23",  "Prov 28:13",  "Prov 31:18",  "Ps 104:34",   "Ps 112:5",    "Ps 116:7",
                  "Ps 118:25",   "Ps 119:122",  "Ps 119:140",  "Ps 119:17",   "Ps 122:3",    "Ps 122:6",
                  "Ps 128:2",    "Ps 139:14",   "Ps 13:6",     "Ps 142:8",    "Ps 16:6",     "Ps 1:3",
                  "Ps 30:7",     "Ps 37:35",    "Ps 46:2",     "Ps 51:20",    "Ps 51:4",     "Ps 73:12",
                  "Ps 73:3",     "Ps 86:17",    "Rom 14:21",   "Rom 15:31",   "Ruth 3:1",    "Sir 15:9",
                  "Sir 19:9",    "Sir 1:13",    "Sir 21:10",   "Sir 21:22",   "Sir 22:23",   "Sir 25:1",
                  "Sir 25:4",    "Sir 25:5",    "Sir 26:14",   "Sir 37:28",   "Sir 38:21",   "Sir 3:1",
                  "Sir 42:21",   "Sir 42:6",    "Sir 6:11",    "Sir 9:13",    "Song 3:8",    "Song 4:4",
                  "Tob 12:11",   "Tob 12:7",    "Tob 14:9",    "Tob 4:14",    "Tob 7:4",     "Tob 7:5",
                  "Wis 11:11",   "Wis 14:22",   "Wis 16:2",    "Wis 5:21",    "Wis 6:24",    "Wis 8:1",
                  "Jer 17:21",   "Prov 20:7"},
                 {{"Ezek 33:32", 1}}},
                {"gibea", "", "⟦gIb'e:a⟧", 41, {}},
                {"ahabs", "", "⟦'A:habs⟧", 35, {}},
                {"baesa", "", "⟦ba'e:sa⟧", 22, {}},
                {"us", "", "⟦'u-s⟧", 8, {}},
                {"ana", "⟦'A:na⟧", "ana", 9, {"2Macc 8:11"}},
                {"förband", "⟦f'Y:Rb,and⟧", "förband", 5, {"Ezek 30:21"}},
            };
            const PronunciationMatcher matcher("sv", lexicon);
            const auto count = [](const std::string& text, const std::string& word) {
                int result = 0;
                std::istringstream pieces(text);
                for (std::string piece; pieces >> piece;)
                    result += word.starts_with("⟦") ? piece.find(word) != std::string::npos
                                                    : contains_speech_word(piece, word);
                return result;
            };
            std::map<std::string, int> found;
            sqlite3* corpus_db = nullptr;
            check(sqlite3_open_v2(argv[1], &corpus_db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
                  "open corpus for homographs");
            sqlite3_stmt* verses = nullptr;
            sqlite3_prepare_v2(
                corpus_db,
                "SELECT book.code||' '||chapter||':'||verse,text FROM verse JOIN book ON "
                "book.id=book_id JOIN source ON source.id=source_id WHERE source.code='sv1917'",
                -1, &verses, nullptr);
            while (sqlite3_step(verses) == SQLITE_ROW) {
                const std::string ref = reinterpret_cast<const char*>(sqlite3_column_text(verses, 0));
                const std::string text = reinterpret_cast<const char*>(sqlite3_column_text(verses, 1));
                for (const auto& homograph : homographs) {
                    const int n = count(text, homograph.word);
                    if (!n)
                        continue;
                    found[homograph.word] += n;
                    const auto speech = matcher.utterance(text).speech_text;
                    if (const auto mixed = homograph.mixed.find(ref); mixed != homograph.mixed.end()) {
                        check(count(speech, homograph.first) == mixed->second &&
                                  count(speech, homograph.second) == n - mixed->second,
                              "homograph " + homograph.word + " in " + ref);
                        continue;
                    }
                    const auto& expected =
                        homograph.verses.contains(ref) ? homograph.first : homograph.second;
                    check(count(speech, expected) == n, "homograph " + homograph.word + " in " + ref);
                }
            }
            sqlite3_finalize(verses);
            sqlite3_close(corpus_db);
            for (const auto& homograph : homographs)
                check(found[homograph.word] == homograph.occurrences, "occurrences of " + homograph.word);
        }
        lexicon.push_back({"sv", "Manasse", "Manasse-respelt", "", 1000});
        check(make_utterance("Manasse", "sv", lexicon).speech_text == "Manasse-respelt",
              "review corrections outrank bundled phonemes");
        std::string long_speech;
        for (int i = 0; i < 24; ++i)
            long_speech += "Herren är min herde.  Ἐν ἀρχῇ ἦν ὁ λόγος.\n";
        std::string recovered;
        const auto compact = [](std::string s) {
            std::erase_if(s, [](unsigned char c) {
                return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
            });
            return s;
        };
        for (const auto& chunk : speech_chunks(long_speech)) {
            check(!chunk.empty() && chunk.size() <= 320, "bounded speech chunks");
            recovered += chunk;
        }
        check(compact(recovered) == compact(long_speech),
              "chunking must retain every Swedish and polytonic Greek byte in order");
        check(speech_chunks(std::string(300, 'x')) == std::vector<std::string>{std::string(300, 'x')},
              "a long word must never be cut into invalid pieces");
        const auto epistle = make_utterance(corpus.verse("sv1917", "Phil", {2, 12})->text, "sv", lexicon);
        check(speech_chunks(epistle.speech_text) == std::vector<std::string>{epistle.speech_text},
              "Philippians 2:12 must keep 'då jag är frånvarande' in one synthesis chunk");
        const auto sentence = std::string(230, 'a') + " då jag är frånvarande.";
        check(speech_chunks(sentence + " Nästa mening.")[0] == sentence,
              "A nearby sentence ending must take precedence over a split inside its last phrase");
        // eSpeak codes use . and ; as letters; a long verse must not split inside an override.
        for (const std::size_t lead : {225, 233, 238, 241, 300}) {
            const auto text = std::string(lead, 'a') + " ⟦S;'Is.ta⟧ " + std::string(400, 'b') + " slut.";
            const auto chunks = speech_chunks(text);
            check(chunks.size() > 1 &&
                      std::ranges::all_of(chunks,
                                          [](const std::string& chunk) {
                                              const auto open = chunk.find("⟦");
                                              return open == std::string::npos
                                                         ? chunk.find("⟧") == std::string::npos
                                                         : chunk.find("⟧", open) != std::string::npos;
                                          }),
                  "chunks keep each ⟦…⟧ override whole");
        }
        check(speech_text_weight("⟦S;'Os.⟧") == 8 && speech_text_weight("s.") == 5,
              "punctuation inside an override weighs as a letter");
        const std::filesystem::path user_path(argv[2]);
        std::filesystem::remove(user_path);
        const WindowPlacement placement{-1200, 80, 960, 720, true};
        {
            UserDb user(user_path);
            check(!user.window_placement(), "a new profile has no saved window geometry");
            user.window_placement(placement);
            user.save(Settings{});
        }
        {
            UserDb user(user_path);
            check(user.window_placement() == placement,
                  "normal window size, negative desktop coordinates and maximized state survive reopening "
                  "and settings saves");
            bool rejected = false;
            try {
                user.window_placement({0, 0, 0, 720});
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected && user.window_placement() == placement,
                  "invalid window sizes preserve the previous geometry");
            sqlite3* fixture = nullptr;
            check(sqlite3_open(user_path.string().c_str(), &fixture) == SQLITE_OK, "open geometry fixture");
            check(sqlite3_exec(fixture, "UPDATE settings SET value='960junk' WHERE key='window.width'",
                               nullptr, nullptr, nullptr) == SQLITE_OK &&
                      !user.window_placement(),
                  "malformed saved geometry falls back to the default window");
            user.window_placement(placement);
            check(sqlite3_exec(fixture,
                               "CREATE TRIGGER reject_geometry BEFORE INSERT ON settings WHEN "
                               "NEW.key='window.height' BEGIN SELECT RAISE(ABORT,'test interruption'); END",
                               nullptr, nullptr, nullptr) == SQLITE_OK,
                  "window geometry transaction fixture");
            rejected = false;
            try {
                user.window_placement({10, 20, 800, 600});
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected && user.window_placement() == placement,
                  "a failed geometry save rolls back all coordinates and dimensions");
            sqlite3_close(fixture);
        }
        std::filesystem::remove(user_path);
        {
            UserDb user(user_path);
            auto s = user.load();
            check(s.theme == Theme::System && s.color_scheme == ColorScheme::RosePine &&
                      s.speech_voice == "espeak" && !s.speech_highlight && s.tradition == Tradition::Greek &&
                      s.update_channel == UpdateChannel::Releases,
                  "default theme, voice, reading order, releases channel, and disabled read-aloud highlight");
            sqlite3* legacy = nullptr;
            check(sqlite3_open(user_path.string().c_str(), &legacy) == SQLITE_OK,
                  "open legacy user settings");
            const int legacy_result = sqlite3_exec(
                legacy, "INSERT INTO settings VALUES('tradition','antiochian')", nullptr, nullptr, nullptr);
            sqlite3_close(legacy);
            check(legacy_result == SQLITE_OK && user.load().tradition == Tradition::Greek,
                  "a reading order other than Slavic selects the Greek family");
            s.theme = Theme::Dark;
            s.color_scheme = ColorScheme::TokyoNight;
            s.calendar = CalendarStyle::Old;
            s.tradition = Tradition::Slavic;
            s.split_view = true;
            s.text_zoom = 125;
            s.speech_rate = 175;
            s.speech_voice = "espeak-f2";
            s.speech_highlight = true;
            s.update_channel = UpdateChannel::Nightly;
            user.save(s);
        }
        {
            UserDb user(user_path);
            auto s = user.load();
            check(s.theme == Theme::Dark && s.color_scheme == ColorScheme::TokyoNight &&
                      s.calendar == CalendarStyle::Old && s.tradition == Tradition::Slavic && s.split_view &&
                      s.text_zoom == 125 && s.speech_rate == 175 && s.speech_voice == "espeak-f2" &&
                      s.speech_highlight && s.update_channel == UpdateChannel::Nightly,
                  "persisted settings");
            s.update_channel = UpdateChannel::Releases;
            s.speech_voice = "espeak:sv+adam";
            user.save(s);
            user.complete("plan:nt:1");
            user.complete("plan:nt:2");
            user.complete("plan:nt:2");
            user.complete("plan:full:1");
            user.complete("day:2026-10-04:2Cor 9:6-9:11");
        }
        {
            UserDb user(user_path);
            check(user.load().update_channel == UpdateChannel::Releases,
                  "switching back to releases survives reopening the profile");
            check(user.load().speech_voice == "espeak:sv+adam",
                  "an installed eSpeak voice selection survives reopening the profile");
            sqlite3* inspect = nullptr;
            check(sqlite3_open(user_path.string().c_str(), &inspect) == SQLITE_OK,
                  "open update channel fallback fixture");
            const int result =
                sqlite3_exec(inspect, "UPDATE settings SET value='unknown' WHERE key='update_channel'",
                             nullptr, nullptr, nullptr);
            sqlite3_close(inspect);
            check(result == SQLITE_OK && user.load().update_channel == UpdateChannel::Releases,
                  "unknown update channels fall back to releases");
            check(user.completed().size() == 4 && user.completed().contains("plan:nt:2"),
                  "persisted progress");
            user.complete("plan:nt:1", false);
            user.forget("plan:full:");
            const auto left = user.completed();
            check(left == std::set<std::string>{"plan:nt:2", "day:2026-10-04:2Cor 9:6-9:11"},
                  "forgetting one plan keeps the others");
        }
        std::filesystem::remove(user_path);
        {
            UserDb user(user_path);
            auto s = user.load();
            s.theme = Theme::Dark;
            s.text_zoom = 150;
            user.save(s);
            sqlite3* inspect = nullptr;
            sqlite3_open(user_path.string().c_str(), &inspect);
            sqlite3_stmt* query = nullptr;
            sqlite3_prepare_v2(inspect,
                               "SELECT (SELECT strict FROM pragma_table_list WHERE name='settings'),(SELECT "
                               "journal_mode FROM pragma_journal_mode)",
                               -1, &query, nullptr);
            check(sqlite3_step(query) == SQLITE_ROW && sqlite3_column_int(query, 0) == 1 &&
                      std::string(reinterpret_cast<const char*>(sqlite3_column_text(query, 1))) == "wal",
                  "settings storage policy");
            sqlite3_finalize(query);
            check(sqlite3_exec(inspect,
                               "CREATE TRIGGER reject_setting BEFORE INSERT ON settings WHEN "
                               "NEW.key='calendar' BEGIN SELECT RAISE(ABORT,'test interruption'); END",
                               nullptr, nullptr, nullptr) == SQLITE_OK,
                  "atomicity fixture");
            sqlite3_close(inspect);
            s.theme = Theme::Light;
            s.text_zoom = 175;
            bool rejected = false;
            try {
                user.save(s);
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected && user.load().theme == Theme::Dark && user.load().text_zoom == 150,
                  "failed save must roll back every preference");
        }
        std::filesystem::remove(user_path);
        std::cout << checks << " checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
