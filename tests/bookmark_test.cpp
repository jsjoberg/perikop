#include "storage/database.hpp"
#include <filesystem>
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
        if (argc != 3)
            throw std::runtime_error("Expected corpus and temporary profile paths");
        CorpusDb corpus(argv[1]);
        const std::filesystem::path path(argv[2]);
        std::filesystem::remove(path);
        const Passage john{"John", {3, 16}, {4, 2}};
        {
            UserDb user(path);
            user.bookmark("en-web", john);
            user.bookmark("en-web", john);
            user.bookmark("grc-lxx", {"Ps", {22, 1}, {22, 6}});
            user.bookmark("grc-lxx", {"Gen", {1, 1}, {1, 5}});
            user.bookmark("en-web", {"John", {1, 1, "a"}, {1, 1, "b"}});
            user.bookmark("grc-lxx", {"Ezra", {11, 1}, {11, 3}});
            user.bookmark("grc-lxx", {"Ezra", {1, 1}, {1, 3}});
            const auto marks = user.bookmarks(corpus);
            check(marks.size() == 6, "Adding the same bookmark twice creates one entry");
            check(marks[0].passage.book == "Gen" && marks[1].passage.book == "Ezra" &&
                      marks[1].passage.first.chapter == 1 && marks[2].passage.first.chapter == 11 &&
                      marks[3].passage.book == "Ps" && marks[4].passage.book == "John" &&
                      marks[4].passage.first.suffix == "a" && marks[5].passage.first == john.first,
                  "Bookmarks follow canon order, including framed canon books, then verse order");
        }
        {
            UserDb user(path);
            const auto marks = user.bookmarks(corpus);
            check(marks.size() == 6 && marks.back().source == "en-web" &&
                      marks.back().passage.last == john.last,
                  "Source and complete passage survive reopening the profile");
            user.remove_bookmark(marks.back().id);
            check(user.bookmarks(corpus).size() == 5, "Removing a bookmark preserves the other bookmarks");
            user.save(Settings{});
            user.complete("plan:test:0");
            user.forget("plan:");
            check(user.bookmarks(corpus).size() == 5, "Settings and reading-plan changes preserve bookmarks");
        }
        {
            UserDb user(path);
            check(user.bookmarks(corpus).size() == 5, "Bookmark removal survives reopening");
        }
        std::cout << "Bookmarks: order, deduplication, persistence, source, and removal passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
