#include "ui/reader_controls.hpp"

namespace ortho {
ReaderControls reader_controls(const ReaderContext& context) {
    const bool auxiliary = auxiliary_screen(context.screen), reader = context.screen == Screen::Reader;
    const bool active = speech_active(context.playback), paused = context.playback == SpeechState::Paused;
    const std::string label = active                        ? (paused ? "Fortsätt" : "Pausa")
                              : reader && context.selection ? "Läs markering"
                                                            : "Lyssna";
    const auto action = !reader             ? PlaybackAction::None
                        : active            ? (paused ? PlaybackAction::Resume : PlaybackAction::Pause)
                        : context.selection ? PlaybackAction::StartSelection
                                            : PlaybackAction::StartReading;
    const auto address = context.screen == Screen::Bible   ? AddressAction::CloseBrowse
                         : reader && context.several_parts ? AddressAction::ChoosePart
                                                           : AddressAction::Browse;
    return {.listen = reader && (!active || paused),
            .pause = reader && active && !paused,
            .stop = !auxiliary && ((reader && active) || context.preparing),
            .prepare = !auxiliary && !active && !context.preparing && context.daily_readings,
            .back = auxiliary || reader,
            .columns = reader,
            .word_audio = !active && !context.preparing,
            .address_dropdown = !context.preparing && address == AddressAction::ChoosePart,
            .playback_action = action,
            .address_action = address,
            .listen_label = active ? "Fortsätt" : label,
            .playback_label = label};
}
} // namespace ortho
