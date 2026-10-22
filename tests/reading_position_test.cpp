#include "core/navigation.hpp"
#include "core/reading_position.hpp"
#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace ortho;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void history_test() {
    NavigationHistory history;
    check(!history.can_back() && !history.can_forward(), "Initial history must be empty");
    const auto first = history.visit(), second = history.visit();
    check(history.back() && history.current() == first && history.can_forward(), "Back retains Forward");
    check(history.forward() && history.current() == second, "Forward restores the exact visit");
    history.back();
    const auto replacement = history.visit();
    check(replacement != second && !history.can_forward() && !history.contains(second),
          "A new destination discards the old Forward branch");
    for (int i = 0; i < 100; ++i)
        history.visit();
    int back = 0;
    while (history.back())
        ++back;
    check(back == 63 && !history.contains(first), "Session history stays bounded");
    NavigationState navigation;
    const auto saved = navigation;
    const auto old = navigation.push(Screen::Bible);
    navigation.restore(saved);
    check(navigation.push(Screen::About).id > old.id, "Restoring views must not reuse live window IDs");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected corpus and temporary profile paths");
        history_test();
        CorpusDb corpus(argv[1]);
        const Reading reading{ReadingKind::Gospel, {"John", {1, 1}, {1, 10}}};
        const auto position = reading_position(corpus, reading, 0, {1, 4});
        check(position && position->percent == 30 && position->book == "John",
              "Progress counts preceding verses");
        check(!reading_position(corpus, reading, 0, {2, 1}) && !reading_position(corpus, reading, 1, {1, 4}),
              "Stale and unrelated positions are rejected");
        const Reading parts{ReadingKind::Gospel, {"John", {1, 1}, {1, 3}}, {{"John", {2, 1}, {2, 2}}}};
        check(reading_position(corpus, parts, 1, {2, 1})->percent == 60,
              "Progress follows disjoint sections in reading order");
        check(reading_key(corpus, corpus.localize(reading)) == reading_key(corpus, reading),
              "Localized references share their resume identity");
        auto queue = reading_speech(corpus, {reading}, [](const std::string&) {
            return std::vector<Pronunciation>{};
        });
        const auto full_verse = queue[3];
        check(resume_speech(queue, *position) == 0.3 && queue.size() == 7 &&
                  queue.front().cue->verse == VerseRef{1, 4} &&
                  queue.front().speech_text == full_verse.speech_text && queue.front().cue->section == 0,
              "Resume starts at the whole current verse and keeps its original cue");
        std::vector<SpeechUtterance> merged{{"earlier verse", "earlier verse", "sv",
                                             SpeechCue{0, 0, "John", position->source, {1, 1}, {1, 1}}},
                                            {"whole merged verse", "whole merged verse", "sv",
                                             SpeechCue{0, 1, "John", position->source, {2, 1}, {2, 2}}}};
        check(resume_speech(merged, ReadingPosition{1, "John", position->source, {2, 2}}) == 0.5 &&
                  merged.size() == 1 && merged.front().cue->verse == VerseRef{2, 1},
              "Merged verses resume from their complete spoken verse");
        const std::filesystem::path path(argv[2]);
        std::filesystem::remove(path);
        {
            auto old = storage::open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
            storage::exec(
                old.get(),
                "CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;"
                "INSERT INTO settings VALUES('speech_voice','espeak-f2');"
                "CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT;"
                "INSERT INTO progress VALUES('plan:nt:1','2026-10-18')");
        }
        {
            UserDb user(path);
            check(user.load().speech_voice == "espeak-f2" && user.completed().contains("plan:nt:1"),
                  "Adding resume storage preserves existing preferences and completed readings");
            user.position(reading_key(corpus, reading), *position);
            user.position("plan:nt:2", *position);
        }
        {
            UserDb user(path);
            check(user.position(reading_key(corpus, reading)) == position,
                  "Verse checkpoints survive reopening the profile");
            user.reset_position(reading_key(corpus, reading));
            check(!user.position(reading_key(corpus, reading)), "Stop clears the reading checkpoint");
            user.forget("plan:nt:");
            check(!user.position("plan:nt:2") && user.completed().empty(),
                  "Starting a plan over clears partial and complete progress");
        }
        std::cout << "Reading checkpoints, whole-verse resume, and session navigation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
