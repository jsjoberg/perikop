#include "core/navigation.hpp"
#include "speech/playback_controller.hpp"
#include "ui/reader_controls.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace ortho;
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
class HeldSpeech final : public SpeechEngine {
public:
    SpeechPlayback state;
    SpeechPreparation preparation_state;
    int pauses = 0, resumes = 0, stops = 0;
    void speak(const SpeechUtterance&) override {}
    void pause() override {
        ++pauses;
        state.state = SpeechState::Paused;
    }
    void resume() override {
        ++resumes;
        state.state = SpeechState::Playing;
    }
    void stop() override {
        ++stops;
        state = {SpeechState::Stopped, {}, 0, 0};
        preparation_state = {};
    }
    SpeechPlayback playback() const override {
        return state;
    }
    void prepare_batch(const std::vector<SpeechUtterance>& queue) override {
        preparation_state = {PreparationState::Running, 0, queue.size(), "Preparing"};
    }
    SpeechPreparation preparation() const override {
        return preparation_state;
    }
};
void navigation_test() {
    NavigationState navigation;
    check(navigation.current() == NavigationState::View{Screen::Day, 0} && !navigation.pop(),
          "Back on the day page must leave its root unchanged");
    for (auto root : {Screen::Reader, Screen::Day}) {
        navigation.show_root(root);
        const auto about = navigation.push(Screen::About);
        const auto bible = navigation.push(Screen::Bible);
        check(about.id != bible.id && navigation.current() == bible && navigation.history_size() == 1,
              "Nested pages must keep distinct view identities and a return history");
        check(navigation.pop() == bible && navigation.current() == about,
              "Closing Browse must restore the exact previous page");
        check(navigation.pop() == about && navigation.current() == NavigationState::View{root, 0},
              "Closing the last page must restore the original reader or day page");
        check(!navigation.auxiliary() && navigation.history_size() == 0,
              "Returning to the root must empty the page history");
        const auto reopened = navigation.push(Screen::Bible);
        check(reopened.id != bible.id, "A destroyed view must never be confused with a reopened one");
        bool rejected = false;
        try {
            navigation.show_root(Screen::Reader);
        } catch (const std::logic_error&) {
            rejected = true;
        }
        check(rejected && navigation.current() == reopened,
              "A root change must not silently abandon live auxiliary views");
        navigation.pop();
    }
}
void controls_test() {
    for (auto screen :
         {Screen::Day, Screen::Reader, Screen::Bible, Screen::Plans, Screen::Settings, Screen::About})
        for (auto state :
             {SpeechState::Idle, SpeechState::Loading, SpeechState::Buffering, SpeechState::Playing,
              SpeechState::Paused, SpeechState::Stopped, SpeechState::Completed, SpeechState::Error})
            for (bool preparing : {false, true})
                for (bool selection : {false, true}) {
                    const auto policy = reader_controls({screen, state, preparing, selection, true, true});
                    check(!(policy.listen && policy.pause), "Listen and Pause must not both be enabled");
                    if (screen != Screen::Reader)
                        check(!policy.listen && !policy.pause && !policy.columns &&
                                  policy.playback_action == PlaybackAction::None,
                              "Playback actions and reader columns must be unavailable outside the reader");
                    if (auxiliary_screen(screen))
                        check(!policy.stop && !policy.prepare && policy.back,
                              "Auxiliary pages must suspend playback while keeping Back available");
                    if (screen == Screen::Bible)
                        check(policy.address_action == AddressAction::CloseBrowse && !policy.address_dropdown,
                              "The address field must close Browse even when parts are selected");
                }
    const auto paused = reader_controls({Screen::Reader, SpeechState::Paused});
    check(paused.listen && !paused.pause && paused.stop && paused.playback_action == PlaybackAction::Resume &&
              paused.listen_label == "Fortsätt",
          "A paused reader must offer Resume and Stop");
    const auto loading = reader_controls({Screen::Reader, SpeechState::Loading});
    check(!loading.listen && loading.pause && loading.playback_action == PlaybackAction::Pause,
          "Loading must be pausable without allowing a second playback start");
    const auto selected = reader_controls({Screen::Reader, SpeechState::Stopped, false, true});
    check(selected.listen && selected.playback_action == PlaybackAction::StartSelection &&
              selected.playback_label == "Läs markering",
          "A stopped reader must offer its selected verses");
    const auto english =
        reader_controls({.screen = Screen::Reader, .playback = SpeechState::Stopped, .speakable = false});
    check(!english.listen && english.playback_action == PlaybackAction::None,
          "A reading without Swedish text must not offer playback");
    const auto english_paused =
        reader_controls({.screen = Screen::Reader, .playback = SpeechState::Paused, .speakable = false});
    check(english_paused.listen && english_paused.playback_action == PlaybackAction::Resume,
          "Playback started elsewhere must stay resumable over a reading without Swedish text");
    const auto home = reader_controls({Screen::Day, SpeechState::Paused});
    check(!home.stop && !home.listen && !home.pause, "A paused day page must disable all playback controls");
    const auto preparing = reader_controls({Screen::Day, SpeechState::Stopped, true, false, false, true});
    check(preparing.stop && !preparing.prepare && !preparing.listen,
          "Audio preparation must remain cancellable without enabling playback on the day page");
}
void playback_test() {
    PlaybackController playback;
    HeldSpeech engine;
    const Reading reading{ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}};
    playback.start({reading});
    for (auto state : {SpeechState::Playing, SpeechState::Loading, SpeechState::Buffering}) {
        engine.state = {state, SpeechCue{0, 0, "John", "sv1917", {1, 2}, {1, 2}}, 0.3, 0.4};
        const int pauses = engine.pauses, resumes = engine.resumes;
        playback.leave_reader(engine);
        playback.update(engine.playback());
        check(engine.pauses == pauses + 1 && engine.state.state == SpeechState::Paused &&
                  engine.state.progress == 0.4 && engine.state.cue->verse == VerseRef{1, 2} &&
                  !playback.polling(),
              "Navigation must pause audible or pending playback without losing its position");
        playback.leave_reader(engine);
        check(engine.pauses == pauses + 1 && engine.resumes == resumes,
              "Nested navigation must not pause twice or resume a held reading");
        playback.toggle_pause(engine);
        playback.update(engine.playback());
        check(playback.polling() && engine.state.state == SpeechState::Playing,
              "Explicit Resume must continue the retained playback session");
    }
    check(playback.playback().progress == 0.4, "A reading from its start must report the engine's progress");
    playback.start({reading}, 0.5);
    for (auto [played, shown] : {std::pair{0.0, 0.5}, {0.5, 0.75}, {1.0, 1.0}}) {
        engine.state.progress = played;
        playback.update(engine.playback());
        check(playback.playback().progress == shown,
              "A resumed reading must continue its progress from the skipped share to the end");
    }
    playback.start({reading});
    playback.update(engine.playback());
    check(playback.playback().progress == 1, "Starting from the beginning must not keep an earlier resume");
    playback.location({0, 0});
    check(!playback.needs_location({0, 0}, true) && playback.needs_location({0, 0}, false) &&
              playback.needs_location({0, 1}, true),
          "Playback must open a view only when its location or reader visibility changes");
    const int stops = engine.stops;
    check(playback.select_reading(engine) && engine.stops == stops + 1 && !playback.location() &&
              engine.state.state == SpeechState::Stopped,
          "A user selection must end the previous session");
    playback.update(engine.playback());
    check(!playback.select_reading(engine) && engine.stops == stops + 1,
          "Opening a reading with no active audio must leave the engine alone");
    engine.state.state = SpeechState::Playing;
    check(playback.select_reading(engine),
          "A selection must stop playback before its first UI status arrives");
    const auto date = parse_date("2026-10-14").value();
    playback.prepare(engine, {SpeechUtterance{"text", "text", "sv"}}, date);
    check(playback.preparing(engine) && playback.prepared_day() == date, "Preparation must retain its day");
    playback.clear_preparation(engine);
    check(!playback.prepared_day() && engine.preparation().state == PreparationState::Idle,
          "Changing the day must cancel unfinished preparation");
    playback.prepare(engine, {SpeechUtterance{"text", "text", "sv"}}, date);
    engine.preparation_state.state = PreparationState::Ready;
    const int ready_stops = engine.stops;
    playback.clear_preparation(engine);
    check(engine.stops == ready_stops && !playback.prepared_day(),
          "Clearing a completed preparation must retain reusable cached audio");
}
} // namespace
int main() {
    try {
        navigation_test();
        controls_test();
        playback_test();
        std::cout << "Reader navigation, controls, and playback policies passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
