#include "core/model.hpp"
#include "core/online_bibles.hpp"
#include "core/reading_plan.hpp"
#include "speech/reading_speech.hpp"
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
            sqlite3_prepare_v2(corpus_db,
                               "SELECT DISTINCT description FROM reading_rule "
                               "WHERE description<>'' AND description<>'Composite'",
                               -1, &occasions, nullptr);
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
        const auto swedish = corpus.map_passage("grc-lxx", "sv1917", daniel[0]);
        check(swedish.size() == 2 && swedish[0].book == "Dan" && swedish[0].last == VerseRef{3, 23} &&
                  swedish[1].book == "PrAzar",
              "Holy Saturday Daniel reading in Swedish");
        const auto baruch = std::find_if(rules.begin(), rules.end(), [](const auto& r) {
            return r.reading.passage.book == "Baruch";
        });
        check(baruch != rules.end() && corpus.localize(baruch->reading).passage.first == VerseRef{3, 35},
              "Baruch reading in LXX numbering");
        check(
            corpus.map_passage("grc-lxx", "sv1917", corpus.localize(baruch->reading).passage).front().first ==
                VerseRef{3, 36},
            "Baruch 3:35 is Swedish 3:36");
        // The OSB order frames the Old Testament by the Septuagint; Brenton's 2 Esdras 11 is Nehemiah 1.
        check(corpus.canon_book("Ezra", 11) && corpus.canon_book("Ezra", 11)->code == "Neh" &&
                  corpus.canon_book("Ezra", 11)->offset() == 10,
              "Nehemiah within Greek 2 Esdras");
        check(corpus.canon_book("EsthGr", 1) && corpus.canon_book("EsthGr", 1)->code == "Esth",
              "Greek Esther is the OSB's Esther");
        // As in Bibel 2000, the reader names the Greek texts of Esther and Daniel; the 1917 books keep their
        // names.
        check(corpus.title("Esth") == "Ester enligt den grekiska texten" &&
                  corpus.short_title("Esth") == "Est gr" &&
                  corpus.title("Dan") == "Daniel enligt den grekiska texten" &&
                  corpus.title("Gen") == "Första Moseboken" && corpus.book_name("Esth") == "Ester",
              "reader titles for the Greek Esther and Daniel");
        check(corpus.frame_source("sv", "Ps") == "grc-lxx" && corpus.frame_source("sv", "John") == "sv1917",
              "Septuagint frames only the Old Testament");
        // Book data comes from books.tsv and canon.tsv through the corpus.
        check(corpus.canon().size() == 79 && corpus.canon().front().code == "Gen" &&
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
            check(plans.size() == 3 && plans[0].parts.size() == 30 && plans[1].parts.size() == 30 &&
                      plans[2].parts.size() == 14,
                  "three reading plans");
            const auto no_lexicon = [](const std::string&) {
                return std::vector<Pronunciation>{};
            };
            std::map<std::string, std::set<int>> covered;
            for (const auto& plan : plans)
                for (const auto& part : plan.parts) {
                    const auto reading = plan_reading(corpus, part, "sv");
                    check(reading.segments().size() == part.size(),
                          "plan range without text: " + plan_label(corpus, part));
                    check(!reading_speech(corpus, {reading}, no_lexicon).empty(),
                          "plan part cannot be read aloud: " + plan_label(corpus, part));
                    for (const auto& range : part)
                        for (int chapter = range.first; chapter <= range.last; ++chapter)
                            check(covered[range.book].insert(chapter).second,
                                  "chapter in two plan parts: " + range.book);
                }
            check(plan_label(corpus, plans[0].parts[22]) == "2 Tim 4 + Tit 1–3 + Filem + Hebr 1–4",
                  "plan part label");
            check(plan_key(plans[1], 6) == "plan:ot:7", "plan part key");
            // Together the plans cover the Septuagint canon and the New Testament, except the books
            // that have no Swedish text yet.
            std::vector<std::string> missing;
            for (const auto& book : corpus.canon()) {
                const auto frame = corpus.frame_source("sv", book.frame_book);
                std::set<int> chapters;
                for (const auto& ref : corpus.coordinates(frame, book.frame_book))
                    if (book.first_chapter <= ref.chapter && ref.chapter <= book.last_chapter)
                        chapters.insert(ref.chapter - book.offset());
                for (int chapter : chapters)
                    if (!covered[book.code].contains(chapter))
                        missing.push_back(book.code + " " + std::to_string(chapter));
            }
            std::string listed;
            for (const auto& item : missing)
                listed += item + ", ";
            check(missing == std::vector<std::string>{"1Esd 1", "1Esd 2", "1Esd 3", "1Esd 4", "1Esd 5",
                                                      "1Esd 6", "1Esd 7", "1Esd 8", "1Esd 9", "3Macc 1",
                                                      "3Macc 2", "3Macc 3", "3Macc 4", "3Macc 5", "3Macc 6",
                                                      "3Macc 7", "Ps 151"},
                  "plans leave out only books without Swedish text: " + listed);
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
                  "⟦manˈasə⟧ och ⟦manˈasəs⟧ söner",
              "bundled name phonemes and genitive");
        const std::string joses = "Joses, Joses' moder; Joses’ moder. Josef och Josefs.";
        const auto joses_utterance = make_utterance(joses, "sv", lexicon);
        check(joses_utterance.display_text == joses,
              "Joses and possessive punctuation remain in display text");
        check(joses_utterance.speech_text ==
                  "⟦jˈuːsəs⟧, ⟦jˈuːsəs⟧' moder; ⟦jˈuːsəs⟧’ moder. Josef och Josefs.",
              "Joses and its possessive keep the Swedish j instead of Spanish Jose's pronunciation");
        check(make_utterance("Han bar sitt kors, och hans ben", "sv", lexicon).speech_text ==
                  "⟦hˈan⟧ bar sitt ⟦kˈɔʂ⟧, och ⟦hˈans⟧ ⟦bˈeːn⟧",
              "bundled homographs take the sense of this Bible, not a name's or another word's");
        check(make_utterance("en ko, sin kos och Kos.", "sv", lexicon).speech_text ==
                  "en ⟦kˈuː⟧, sin kos och Kos.",
              "word-only corrections do not turn the place Kos or sin kos into a cow's genitive");
        check(
            make_utterance("Och förlåten i templet rämnade. Vilkens överträdelse är förlåten.", "sv", lexicon)
                    .speech_text == "Och ⟦fˈøːɭˌoːtən⟧ i templet rämnade. Vilkens överträdelse är förlåten.",
            "the veil is a phrase; forgiven is the voice's reading");
        check(make_utterance("Han hov upp sin röst i ditt hov.", "sv", lexicon).speech_text ==
                  "⟦hˈan⟧ ⟦hˈuːv⟧ upp sin röst i ditt ⟦hˈoːv⟧.",
              "hov is häva's past tense, except a court");
        check(make_utterance("Intet bete för sina får; han skulle bete sig vanvettigt.", "sv", lexicon)
                      .speech_text == "Intet bete för sina får; ⟦hˈan⟧ skulle ⟦bətˈeː⟧ sig vanvettigt.",
              "bete is pasture, except the verb bete sig");
        check(make_utterance("Av Dans stam, under sång och dans.", "sv", lexicon).speech_text ==
                  "Av ⟦dˈɑːns⟧ stam, under sång och ⟦dˈans⟧.",
              "Dans is the tribe before its noun, otherwise a dance");
        {
            // A review preview or correction of the word applies outside the phrases.
            auto preview = lexicon;
            preview.push_back({"sv", "förlåten", "förlåtet", "", 2000});
            check(make_utterance("förlåten i templet, innanför förlåten; förlåten", "sv", preview)
                          .speech_text == "⟦fˈøːɭˌoːtən⟧ i templet, innanför ⟦fˈøːɭˌoːtən⟧; förlåtet",
                  "phrases outrank per-word corrections");
        }
        {
            // Every sv1917 occurrence of each word, with the verses of its first reading.
            struct Homograph {
                std::string word, first, second;
                int occurrences;
                std::set<std::string> verses;
            };
            const std::string veil = "⟦fˈøːɭˌoːtən⟧";
            const std::vector<Homograph> homographs{
                {"förlåten",
                 "förlåten",
                 veil,
                 42,
                 {"Luke 6:37", "2Cor 2:10", "Acts 8:22", "Col 3:13", "Deut 21:8", "Eph 4:32", "Isa 33:24",
                  "John 20:23", "Lev 19:22", "Mark 11:25", "Matt 12:31", "Matt 18:35", "Matt 6:14",
                  "Matt 6:15", "Ps 32:1"}},
                {"hov", "⟦hˈoːv⟧", "⟦hˈuːv⟧", 24, {"Dan 2:49", "Ps 45:10"}},
                {"bete", "⟦bətˈeː⟧", "bete", 17, {"1Sam 1:14", "1Sam 21:15", "2Kgs 7:9", "Hos 3:3"}},
                {"dans",
                 "⟦dˈans⟧",
                 "⟦dˈɑːns⟧",
                 48,
                 {"1Sam 18:6", "Exod 15:20", "Jdt 15:12", "Jdt 15:13", "Jdt 3:7", "Jer 31:13", "Jer 31:4",
                  "Judg 11:34", "Lam 5:15", "Luke 15:25", "Ps 149:3", "Ps 150:4", "Ps 87:7"}},
                {"hos", "", "⟦hˈʊs⟧", 760, {}},
                {"vikt", "", "⟦vˈɪkt⟧", 61, {}},
                {"vänt", "", "⟦vˈɛnt⟧", 44, {}},
                {"rest", "", "⟦rˈeːst⟧", 17, {}},
                {"tomt", "", "⟦tˈʊmt⟧", 14, {}},
                {"predikat", "", "⟦prədˈiːkat⟧", 13, {}},
                {"förleder", "", "⟦fœɭˈeːdər⟧", 5, {}},
                {"förena", "", "⟦fœrˈeːna⟧", 4, {}},
                {"kista", "", "⟦ɕˈɪsta⟧", 4, {}},
                {"ko", "", "⟦kˈuː⟧", 4, {}},
                {"svans", "", "⟦svˈans⟧", 4, {}},
                {"duken", "", "⟦dˈʉːkən⟧", 3, {}},
                {"kosta", "", "⟦kˈɔsta⟧", 3, {}},
                {"logen", "", "⟦lˈuːɡən⟧", 3, {}},
                {"rene", "", "⟦rˈeːnə⟧", 3, {}},
                {"saks", "", "⟦sˈɑːks⟧", 3, {}},
                {"synts", "", "⟦sˈyːnts⟧", 3, {}},
                {"ton", "", "⟦tˈuːn⟧", 3, {}},
                {"eggen", "", "⟦ˈɛɡən⟧", 2, {}},
                {"klor", "", "⟦klˈoːr⟧", 2, {}},
                {"menat", "", "⟦mˈeːnat⟧", 2, {}},
                {"demon", "", "⟦dəmˈoːn⟧", 1, {}},
                {"korsar", "", "⟦kˈɔʂar⟧", 1, {}},
                {"makes", "", "⟦mˈɑːkəs⟧", 1, {}},
                {"mats", "", "⟦mˈɑːts⟧", 1, {}},
                {"murat", "", "⟦mˈʉːrat⟧", 1, {}},
                {"ron", "", "⟦rˈuːn⟧", 1, {}},
                {"tima", "", "⟦tˈiːma⟧", 1, {}},
                {"timat", "", "⟦tˈiːmat⟧", 1, {}},
                {"trumpet", "", "⟦trɵmpˈeːt⟧", 1, {}},
                {"van", "", "⟦vˈɑːn⟧", 1, {}},
                {"åse", "", "⟦ˈoːsˌeː⟧", 1, {}},
                {"öst", "", "⟦ˈøːst⟧", 1, {}},
                {"skylt", "", "⟦ɧˈyːlt⟧", 3, {}},
                {"fördraget", "", "⟦fœɖrˈɑːɡət⟧", 4, {}},
                {"joses", "", "⟦jˈuːsəs⟧", 4, {}},
                {"krubba", "", "⟦krˈɵba⟧", 4, {}},
                {"krubban", "", "⟦krˈɵban⟧", 3, {}},
                {"ana", "⟦ˈɑːna⟧", "ana", 9, {"2Macc 8:11"}},
                {"förband", "⟦fˈøːrbˌand⟧", "förband", 5, {"Ezek 30:21"}},
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
                    const auto& expected =
                        homograph.verses.contains(ref) ? homograph.first : homograph.second;
                    check(count(matcher.utterance(text).speech_text, expected) == n,
                          "homograph " + homograph.word + " in " + ref);
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
        const std::filesystem::path user_path(argv[2]);
        std::filesystem::remove(user_path);
        {
            UserDb user(user_path);
            auto s = user.load();
            check(s.theme == Theme::System && s.speech_voice == "alice" && !s.speech_highlight &&
                      s.tradition == Tradition::Greek,
                  "default theme, voice, reading order, and disabled read-aloud highlight");
            sqlite3* legacy = nullptr;
            check(sqlite3_open(user_path.string().c_str(), &legacy) == SQLITE_OK,
                  "open legacy user settings");
            const int legacy_result = sqlite3_exec(
                legacy, "INSERT INTO settings VALUES('tradition','antiochian')", nullptr, nullptr, nullptr);
            sqlite3_close(legacy);
            check(legacy_result == SQLITE_OK && user.load().tradition == Tradition::Greek,
                  "legacy Antiochian settings select the Greek family");
            s.theme = Theme::Dark;
            s.calendar = CalendarStyle::Old;
            s.tradition = Tradition::Slavic;
            s.parallel = "en";
            s.font_size = 24;
            s.speech_rate = 175;
            s.speech_voice = "bjorn";
            s.speech_highlight = true;
            user.save(s);
        }
        {
            UserDb user(user_path);
            auto s = user.load();
            check(s.theme == Theme::Dark && s.calendar == CalendarStyle::Old &&
                      s.tradition == Tradition::Slavic && s.parallel == "en" && s.font_size == 24 &&
                      s.speech_rate == 175 && s.speech_voice == "bjorn" && s.speech_highlight,
                  "persisted settings");
            user.complete("plan:nt:1");
            user.complete("plan:nt:2");
            user.complete("plan:nt:2");
            user.complete("plan:full:1");
            user.complete("day:2026-10-04:2Cor 9:6-9:11");
        }
        {
            UserDb user(user_path);
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
            s.font_size = 25;
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
            s.font_size = 28;
            bool rejected = false;
            try {
                user.save(s);
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected && user.load().theme == Theme::Dark && user.load().font_size == 25,
                  "failed save must roll back every preference");
        }
        std::filesystem::remove(user_path);
        const auto cache_path = user_path.parent_path() / std::filesystem::path(u8"test-speech-Å.db");
        std::filesystem::remove(cache_path);
        const std::vector<float> pcm = {0.0f, 0.125f, -0.75f, 1.0f, -1.0f};
        {
            SpeechCache cache(cache_path);
            check(!cache.load("voice-a", "sv", "Herren"), "empty speech cache");
            cache.save("voice-a", "sv", "Herren", pcm);
            check(cache.load("voice-a", "sv", "Herren") == pcm, "speech PCM round trip");
            check(!cache.load("voice-b", "sv", "Herren") && !cache.load("voice-a", "el", "Herren") &&
                      !cache.load("voice-a", "sv", "Ordet"),
                  "speech cache must isolate model, language, and text");
            bool invalid = false;
            try {
                cache.save("voice-a", "sv", "Herren", {std::numeric_limits<float>::quiet_NaN()});
            } catch (const std::exception&) {
                invalid = true;
            }
            check(invalid && cache.load("voice-a", "sv", "Herren") == pcm,
                  "invalid audio must not replace a valid cache entry");
        }
        {
            SpeechCache cache(cache_path);
            check(cache.load("voice-a", "sv", "Herren") == pcm,
                  "speech cache persists across process restarts");
        }
        const auto cache_utf8 = cache_path.u8string();
        sqlite3* future_cache = nullptr;
        sqlite3_open(reinterpret_cast<const char*>(cache_utf8.c_str()), &future_cache);
        sqlite3_exec(future_cache, "PRAGMA user_version=99", nullptr, nullptr, nullptr);
        sqlite3_close(future_cache);
        bool future_rejected = false;
        try {
            SpeechCache cache(cache_path);
        } catch (const std::exception&) {
            future_rejected = true;
        }
        check(future_rejected, "unknown future speech cache schema must not be overwritten");
        std::filesystem::remove(cache_path);
        std::cout << checks << " checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
