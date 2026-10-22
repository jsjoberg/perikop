#pragma once
#include "core/navigation.hpp"
#include "speech/speech.hpp"

namespace ortho {
enum class AddressAction { Browse, CloseBrowse, ChoosePart };
enum class PlaybackAction { None, StartReading, StartSelection, Pause, Resume };
struct ReaderContext {
    Screen screen;
    SpeechState playback;
    bool selection = false, several_parts = false;
    // Whether the open reading has Swedish text to read aloud.
    bool speakable = true;
};
// One policy supplies the toolbar, menus, and command guards. It has no native UI dependencies.
struct ReaderControls {
    bool listen, pause, stop, back, columns, word_audio, address_dropdown;
    PlaybackAction playback_action;
    AddressAction address_action;
    std::string listen_label, playback_label;
};
ReaderControls reader_controls(const ReaderContext&);
} // namespace ortho
