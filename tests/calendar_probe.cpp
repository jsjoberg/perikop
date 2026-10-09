#include "core/model.hpp"
#include "storage/database.hpp"
#include <iostream>
int main(int argc, char** argv) {
    try {
        if (argc < 3)
            return 2;
        ortho::CorpusDb corpus(argv[1]);
        ortho::Lectionary calendar(corpus);
        const auto start = ortho::parse_date(argv[2]).value();
        const int length = argc > 3 ? std::stoi(argv[3]) : 1;
        const auto style =
            argc > 4 && std::string(argv[4]) == "old" ? ortho::CalendarStyle::Old : ortho::CalendarStyle::New;
        const std::string order = argc > 5 ? argv[5] : "antiochian";
        const auto tradition = order == "greek"    ? ortho::Tradition::Greek
                               : order == "slavic" ? ortho::Tradition::Slavic
                                                   : ortho::Tradition::Antiochian;
        for (int i = 0; i < length; ++i) {
            auto day = calendar.readings_for(ortho::shift_date(start, i), style, tradition);
            std::cout << ortho::date_iso(day.day.civil_date);
            for (const auto& reading : day.readings) {
                std::cout << '\t' << int(reading.kind) << '=';
                bool first = true;
                for (const auto& p : reading.segments()) {
                    if (!first)
                        std::cout << '|';
                    first = false;
                    std::cout << p.book << '_' << p.first.chapter * 1000 + p.first.verse << '_'
                              << p.last.chapter * 1000 + p.last.verse;
                }
            }
            std::cout << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
