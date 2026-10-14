#pragma once
#include "core/navigation.hpp"
#include "speech/speech.hpp"

namespace ortho {
enum class AddressAction { Browse, CloseBrowse, ChoosePart };
enum class PlaybackAction { None, StartReading, StartSelection, Pause, Resume };
struct ReaderContext {
    Screen screen;
    SpeechState playback;
    bool preparing = false, selection = false, several_parts = false, daily_readings = false;
};
// One policy supplies the toolbar, menus, and command guards. It has no native UI dependencies.
struct ReaderControls {
    bool listen, pause, stop, prepare, back, columns, menus, word_audio, address_dropdown;
    PlaybackAction playback_action;
    AddressAction address_action;
    std::string listen_label, playback_label;
};
ReaderControls reader_controls(const ReaderContext&);
} // namespace ortho
