#include "core/reading_display.hpp"
#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "core/source_preferences.hpp"
#include "storage/content_import.hpp"
#include "storage/sqlite.hpp"
#include "ui/app_icon.hpp"
#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/month_calendar.hpp"
#include "ui/reading_tooltips.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <wx/access.h>
#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/fontenum.h>
#include <wx/graphics.h>
#include <wx/hyperlink.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/modalhook.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif
namespace ortho {
namespace {
// Offers a version without any network, so the smoke test can step through the update controls.
class SmokeUpdates final : public UpdateBackend {
public:
    std::string unavailable() const override {
        return {};
    }
    std::optional<UpdateOffer> check(UpdateChannel) override {
        return UpdateOffer{"9.9.9", 1'500'000, false};
    }
    void download(const std::function<void(int)>& progress) override {
        progress(100);
    }
    void apply() override {}
};
// Navigation tests need a held playback position without model loading or sound output.
class NavigationSpeech final : public SpeechEngine {
public:
    SpeechPlayback held;
    std::vector<SpeechUtterance> spoken;
    std::string voice;
    double speed = 1;
    void set_speed(double value) override {
        speed = value;
    }
    void set_voice(const std::string& selected, const std::string& = "sv") override {
        voice = selected;
    }
    void speak_batch(const std::vector<SpeechUtterance>& queue) override {
        spoken = queue;
        changed = true;
        held = {SpeechState::Playing, queue.empty() ? std::optional<SpeechCue>{} : queue.front().cue, 0, 0};
    }
    bool changed = false;
    std::vector<std::function<void(std::string)>> lookups;
    void speak(const SpeechUtterance&) override {
        changed = true;
    }
    void pause() override {
        changed = true;
        held.state = SpeechState::Paused;
    }
    void resume() override {
        changed = true;
        held.state = SpeechState::Playing;
    }
    void stop() override {
        changed = true;
        held = {SpeechState::Stopped, {}, 0, 0};
    }
    SpeechPlayback playback() const override {
        return held;
    }
    void pronunciation_async(const std::string&, std::function<void(std::string)> deliver) override {
        lookups.push_back(std::move(deliver));
    }
};
class BookmarkConfirmation final : public wxModalDialogHook {
public:
    wxString title = ui::utf8("Ta bort bokmärke");
    int answer = wxID_NO;
    int questions = 0;
    bool safe_default = true;
    int Enter(wxDialog* dialog) override {
        auto* question = dynamic_cast<wxMessageDialog*>(dialog);
        if (!question || question->GetTitle() != title)
            return wxID_CANCEL;
        ++questions;
        safe_default &= (question->GetMessageDialogStyle() & wxNO_DEFAULT) != 0;
        return answer;
    }
};
} // namespace
bool MainFrame::smoke_test(const wxString& screenshot_path) {
    std::cout << std::unitbuf; // Keep completed checks in CI logs even if a later check times out.
    const auto original = settings_;
    const auto set_parallel = [this](const std::string& language) {
        settings_.split_view = !language.empty();
        settings_.secondary_sources = Settings{}.secondary_sources;
        std::ranges::stable_sort(settings_.secondary_sources, {}, [&](const SourcePreference& source) {
            return source_language(source.source) != language;
        });
    };
    const auto date = selected_.date();
    // Rendering fixtures use a fresh reading; resume behavior is exercised below.
    const auto open_fresh_psalm = [this] {
        open_psalm();
        stop_speech();
    };
    open_fresh_psalm();
    Layout();
    bool ok = true;
    // Names the failed check, since the checks otherwise fail silently.
    const auto fail = [&ok](int line) {
        std::cerr << "Smoke check failed: main_frame_smoke.cpp:" << line << '\n';
        ok = false;
    };
    // Exercise the actual UI commands and SQLite profile without sound or model loading.
    {
        const auto data = data_ / "resume-navigation";
        UserDb profile(data / "user.db");
        const Reading reading{ReadingKind::Gospel, {"John", {1, 1}, {1, 10}}};
        const auto key = reading_key(corpus_, reading);
        {
            auto candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                         std::make_unique<SmokeUpdates>(), true);
            auto held = std::make_unique<NavigationSpeech>();
            auto* engine = held.get();
            candidate->speech_ = std::move(held);
            std::set<std::pair<int, int>> shortcuts;
            for (std::size_t i = 0; i < candidate->menu_bar_->GetMenuCount(); ++i)
                for (auto* item : candidate->menu_bar_->GetMenu(i)->GetMenuItems())
                    if (const std::unique_ptr<wxAcceleratorEntry> key{item->GetAccel()})
                        if (!shortcuts.emplace(key->GetFlags(), key->GetKeyCode()).second)
                            fail(__LINE__);
            candidate->Show();
            candidate->open_tracked(reading, {"plan:resume:0", "Resume smoke test"});
            candidate->Layout();
            candidate->playback_.start({reading});
            candidate->speech_tracked_ = candidate->tracked_;
            engine->held = {SpeechState::Playing, SpeechCue{0, 0, "John", "sv1917", {1, 4}, {1, 4}}, 0.6,
                            0.36};
            candidate->display_playback(engine->held);
            candidate->toggle_pause();
            const auto saved = profile.position(key);
            if (!saved || saved->verse != VerseRef{1, 4} || saved->percent != 30 ||
                profile.position("plan:resume:0") != saved || !candidate->stop_->IsEnabled())
                fail(__LINE__);
            candidate->play_or_pause();
            if (engine->spoken.size() != 7 || !engine->spoken.front().cue ||
                engine->spoken.front().cue->verse != VerseRef{1, 4} || engine->held.verse_progress != 0 ||
                candidate->scripture_->reading().passage.first != VerseRef{1, 1})
                fail(__LINE__);
            candidate->set_speech_rate(125);
            if (engine->speed != 1.25 || engine->held.state != SpeechState::Playing ||
                engine->spoken.size() != 7 || !engine->spoken.front().cue ||
                engine->spoken.front().cue->verse != VerseRef{1, 4} || !candidate->speech_tracked_ ||
                candidate->speech_tracked_->key != "plan:resume:0")
                fail(__LINE__);
            candidate->about();
            engine->changed = false;
#ifdef __WXMSW__
            candidate->MSWWindowProc(WM_APPCOMMAND, 0,
                                     MAKELPARAM(0, APPCOMMAND_BROWSER_BACKWARD | FAPPCOMMAND_MOUSE));
            if (!candidate->navigation_.reader())
                fail(__LINE__);
            candidate->MSWWindowProc(WM_APPCOMMAND, 0,
                                     MAKELPARAM(0, APPCOMMAND_BROWSER_FORWARD | FAPPCOMMAND_MOUSE));
            if (candidate->navigation_.current().screen != Screen::About)
                fail(__LINE__);
#endif
            wxMouseEvent back_button(wxEVT_AUX1_UP);
            back_button.SetEventObject(candidate->scripture_);
            candidate->scripture_->GetEventHandler()->ProcessEvent(back_button);
            if (!candidate->navigation_.reader() || engine->changed || !candidate->forward_->IsEnabled())
                fail(__LINE__);
            wxMouseEvent forward_button(wxEVT_AUX2_UP);
            forward_button.SetEventObject(candidate->address_);
            candidate->address_->GetEventHandler()->ProcessEvent(forward_button);
            if (candidate->navigation_.current().screen != Screen::About || engine->changed)
                fail(__LINE__);
            candidate->back();
            candidate->show_settings();
            if (candidate->history_.can_forward())
                fail(__LINE__);
            candidate->back();
            candidate->open_reading({ReadingKind::Gospel, {"John", {2, 1}, {2, 5}}});
            candidate->back();
            if (!candidate->visible_reading_ ||
                candidate->visible_reading_->passage.first != VerseRef{1, 1} || !candidate->tracked_ ||
                candidate->tracked_->key != "plan:resume:0")
                fail(__LINE__);
            candidate->forward();
            if (candidate->visible_reading_->passage.first != VerseRef{2, 1})
                fail(__LINE__);
            candidate->back();
            candidate->stop_speech();
            if (profile.position(key) || profile.position("plan:resume:0") || candidate->part_ != 0)
                fail(__LINE__);
            candidate->about();
            candidate->back();
            if (profile.position(key))
                fail(__LINE__);
            candidate->play_or_pause();
            if (engine->spoken.size() != 10 || engine->spoken.front().cue->verse != VerseRef{1, 1})
                fail(__LINE__);
            candidate->stop_speech();
            candidate->scripture_->select_verses({1, 4}, {1, 5});
            candidate->play_or_pause();
            engine->held.state = SpeechState::Completed;
            candidate->display_playback(engine->held);
            wxTheApp->ProcessPendingEvents();
            if (candidate->tracked_ || profile.completed().contains("plan:resume:0") ||
                engine->spoken.size() != 2)
                fail(__LINE__);
            candidate->back();
            candidate->scripture_->show_verse({1, 10});
            candidate->show_readings();
            if (profile.completed().contains("plan:resume:0"))
                fail(__LINE__);
            candidate->back();
            candidate->playback_.start({reading});
            candidate->speech_tracked_ = candidate->tracked_;
            engine->held = {SpeechState::Playing, SpeechCue{0, 0, "John", "sv1917", {1, 6}, {1, 6}}, 0.5,
                            0.55};
            candidate->display_playback(engine->held);
            candidate->Hide();
        }
        if (!profile.position(key) || profile.position(key)->verse != VerseRef{1, 6})
            fail(__LINE__);
        {
            auto reopened = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                        std::make_unique<SmokeUpdates>(), true);
            auto held = std::make_unique<NavigationSpeech>();
            auto* engine = held.get();
            reopened->speech_ = std::move(held);
            reopened->open_tracked(reading, {"plan:resume:0", "Resume smoke test"});
            if (!reopened->stop_->IsEnabled() || reopened->play_->GetLabel() != ui::utf8("Fortsätt"))
                fail(__LINE__);
            reopened->play_or_pause();
            if (engine->spoken.size() != 5 || engine->spoken.front().cue->verse != VerseRef{1, 6})
                fail(__LINE__);
            reopened->stop_speech();
            const Reading split{ReadingKind::Gospel, {"John", {1, 1}, {1, 10}}, {{"John", {2, 1}, {2, 5}}}};
            reopened->open_tracked(split, {"plan:resume:1", "Multiple sections"});
            reopened->playback_.start({split});
            reopened->speech_tracked_ = reopened->tracked_;
            engine->held = {SpeechState::Playing, SpeechCue{0, 1, "John", "sv1917", {2, 3}, {2, 3}}, 0.6,
                            0.8};
            reopened->display_playback(engine->held);
            reopened->toggle_pause();
            if (profile.position("plan:resume:1")->percent != 80)
                fail(__LINE__);
            reopened->play_or_pause();
            if (engine->spoken.size() != 3 || engine->spoken.front().cue->section != 1 ||
                reopened->part_ != 1 || reopened->scripture_->reading().segments().size() != 2)
                fail(__LINE__);
            reopened->stop_speech();
            if (reopened->part_ != 0 || profile.position("plan:resume:1"))
                fail(__LINE__);
            reopened->play_or_pause();
            engine->held.state = SpeechState::Completed;
            reopened->display_playback(engine->held);
            wxTheApp->ProcessPendingEvents();
            if (!profile.completed().contains("plan:resume:1") || profile.position("plan:resume:1"))
                fail(__LINE__);
            reopened->stop_speech();
        }
        if (profile.position(key))
            fail(__LINE__);
    }
    std::cout << "Automatic verse resume, Stop/reset, mouse Back/Forward, and profile reopen.\n";
    if (!corpus_.read_only())
        fail(__LINE__);
    // Windows/Linux use only the overflow menu and accelerators. macOS keeps its native bar.
    if (!menu_ || !menu_->IsShown() || !menu_bar_)
        fail(__LINE__);
#ifdef __WXOSX__
    if (GetMenuBar() != menu_bar_)
#else
    if (GetMenuBar())
#endif
        fail(__LINE__);
    // Edit follows focus; Select All belongs to text fields, never the Scripture book.
    {
        const auto data = data_ / "desktop-conventions";
        UserDb profile(data / "user.db");
        auto candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                     std::make_unique<SmokeUpdates>(), true);
        candidate->Show();
        candidate->Raise();
        wxTheApp->Yield(true);
        candidate->open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}});
        candidate->scripture_->select_verses({1, 1}, {1, 2});
        // Exercise native editing without requiring the runner to grant OS keyboard focus.
        wxWindow* edit_focus = candidate->scripture_;
        const auto enabled = [&](int id) {
            return candidate->edit_enabled(id, edit_focus);
        };
        if (candidate->menu_bar_->FindMenu("Redigera") == wxNOT_FOUND || !enabled(wxID_COPY) ||
            enabled(wxID_CUT) || enabled(wxID_PASTE) || enabled(wxID_SELECTALL))
            fail(__LINE__);
        const auto command = [&](int id) {
            candidate->edit(id, edit_focus);
        };
        const auto clipboard_text = [&] {
            wxTextDataObject text;
            if (!wxTheClipboard->Open()) {
                fail(__LINE__);
                return wxString{};
            }
            if (!wxTheClipboard->GetData(text))
                fail(__LINE__);
            wxTheClipboard->Close();
            return text.GetText();
        };
        command(wxID_COPY);
        if (clipboard_text() != ui::utf8(candidate->scripture_->selected_text()))
            fail(__LINE__);
        // A multiline control owns its native selection; macOS single-line fields share a
        // field editor that is unavailable when the test application cannot become active.
        auto* field = new wxTextCtrl(candidate->root_, wxID_ANY, "before after", wxDefaultPosition,
                                     wxDefaultSize, wxTE_MULTILINE);
        edit_focus = field;
        field->SetSelection(0, 6);
        if (!enabled(wxID_COPY) || !enabled(wxID_CUT) || !enabled(wxID_SELECTALL) ||
            enabled(wxID_UNDO) != field->CanUndo() || enabled(wxID_REDO) != field->CanRedo())
            fail(__LINE__);
        command(wxID_COPY);
        if (clipboard_text() != "before")
            fail(__LINE__); // Native text copy must not include the reader's Scripture citation.
        command(wxID_CUT);
        if (field->GetValue() != " after")
            fail(__LINE__);
        command(wxID_PASTE);
        if (field->GetValue() != "before after")
            fail(__LINE__);
        command(wxID_SELECTALL);
        if (field->GetStringSelection() != field->GetValue())
            fail(__LINE__);
        field->SetEditable(false);
        if (!enabled(wxID_COPY) || enabled(wxID_CUT) || enabled(wxID_PASTE))
            fail(__LINE__);
        field->Destroy();
        edit_focus = candidate->scripture_;
        command(wxID_SELECTALL);
        const auto selected = candidate->scripture_->selections();
        if (selected.size() != 1 || selected.front().last != VerseRef{1, 2})
            fail(__LINE__);
        const auto work = wxDisplay(0u).GetClientArea();
        candidate->SetSize(work.x + 16, work.y + 16, std::min(800, work.width - 32),
                           std::min(640, work.height - 32));
        wxTheApp->Yield(true);
        candidate->save_window_placement();
        const auto normal = profile.window_placement();
        if (!normal || normal->maximized ||
            candidate->ToDIP(candidate->GetSize()) != wxSize(normal->width, normal->height))
            fail(__LINE__);
        candidate->Maximize();
        // A bare Xvfb server has no window manager to honor a maximize request.
        for (int attempt = 0; attempt < 50 && !candidate->IsMaximized(); ++attempt) {
            wxTheApp->Yield(true);
            wxMilliSleep(10);
        }
        wxTheApp->Yield(true);
        const bool can_maximize = candidate->IsMaximized();
        candidate->save_window_placement();
        const auto maximized = profile.window_placement();
        if (!maximized || maximized->maximized != can_maximize || !normal ||
            maximized->width != normal->width || maximized->height != normal->height ||
            maximized->x != normal->x || maximized->y != normal->y)
            fail(__LINE__);
        candidate->Maximize(false);
        wxTheApp->Yield(true);
        candidate->SetSize(work.x, work.y, 600, 500);
        candidate->restore_window_placement();
        wxTheApp->Yield(true);
        if (candidate->IsMaximized() != can_maximize ||
            (normal && candidate->normal_window_size_dip_ != wxSize(normal->width, normal->height)))
            fail(__LINE__);
        candidate->Maximize(false);
        wxTheApp->Yield(true);
        profile.window_placement({100000, 100000, 90000, 90000});
        candidate->restore_window_placement();
        wxTheApp->Yield(true);
        if (!work.Contains(candidate->GetRect()))
            fail(__LINE__); // A disconnected monitor or smaller desktop cannot hide the window.
        candidate->SetSize(work.x + 16, work.y + 16, std::min(800, work.width - 32),
                           std::min(640, work.height - 32));
        wxTheApp->Yield(true);
        const auto expected = candidate->GetRect();
        candidate->diagnostic_ = false; // Exercise the real close/reopen persistence hooks.
        candidate.reset();
        candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                std::make_unique<SmokeUpdates>());
        if (candidate->GetRect() != expected)
            fail(__LINE__);
    }
    if (user_.window_placement())
        fail(__LINE__); // Diagnostic frames never overwrite window placement.
    // GetIcon() needs an exact system-size match on some ports. Check the stored artwork.
    if (GetIcons().IsEmpty() || !GetIcons().GetIconByIndex(0).IsOk())
        fail(__LINE__);
    if (!app_icon_image().IsOk() || std::filesystem::exists(resources_ / "icons"))
        fail(__LINE__);
    for (const auto* face : {"Literata", "IBM Plex Sans", "Noto Serif Hebrew"})
        if (!wxFontEnumerator::IsValidFacename(face)) {
            std::cerr << "Missing font: " << face << '\n';
            fail(__LINE__);
        }
    // The reader as drawn in a size, and a copy saved beside the screenshot.
    const auto render = [this](wxSize size = {}, bool cached = true, double scale = 1) {
        if (size == wxSize{})
            size = scripture_->GetClientSize();
        wxBitmap bitmap;
        bitmap.CreateWithLogicalSize(size, scale);
        {
            wxMemoryDC dc(bitmap);
            scripture_->render_to(dc, size, cached);
        }
        return bitmap.ConvertToImage();
    };
    const auto save = [&](const wxImage& image, const wxString& suffix) {
        if (!screenshot_path.empty())
            ok = image.SaveFile(screenshot_path.BeforeLast('.') + suffix + ".png", wxBITMAP_TYPE_PNG) && ok;
    };
    // Native drawing must use the same shaped widths as paragraph fitting.
    wxClientDC metrics(scripture_);
    metrics.SetFont(body_font(19));
    for (const auto& [language, text] : std::vector<std::pair<std::string, wxString>>{
             {"sv",
              ui::utf8("I begynnelsen skapade Gud himmel och jord. Och Gud såg att det var gott. Detta är en "
                       "längre text för att kontrollera styckets jämna radbrytning och mellanrum.")},
             {"el", wxString::FromUTF8("Ἐν ἀρχῇ ἦν ὁ Λόγος, καὶ ὁ Λόγος ἦν πρὸς τὸν Θεόν, καὶ Θεὸς ἦν ὁ "
                                       "Λόγος. Οὗτος ἦν ἐν ἀρχῇ πρὸς τὸν Θεόν.")},
             {"en", "In the beginning was the Word, and the Word was with God, and the Word was God. The "
                    "same was in the beginning with God."}}) {
        const auto layout = layout_paragraph(metrics, text, 280, language);
        wxString recovered;
        bool justified = false;
        for (std::size_t i = 0; i < layout.lines.size(); ++i) {
            const auto& line = layout.lines[i];
            if (line.width > 281)
                fail(__LINE__);
            if (line.justified) {
                justified = true;
                if (std::abs(line.width - 280) > 0.01)
                    fail(__LINE__);
            }
            if (i + 1 == layout.lines.size() && line.justified)
                fail(__LINE__);
            for (std::size_t w = 0; w < line.runs.size(); ++w) {
                auto run = line.runs[w].text;
                if (line.hyphenated && w + 1 == line.runs.size())
                    run.RemoveLast();
                recovered += run;
            }
        }
        auto original = text;
        original.Replace(" ", "");
        if (recovered != original || !justified)
            fail(__LINE__);
    }
    {
        const std::vector<wxString> words = {"one",   "two",   "three", "four", "five",   "six",
                                             "seven", "eight", "nine",  "ten",  "eleven", "twelve"};
        std::unique_ptr<wxGraphicsContext> gc(
            wxGraphicsRenderer::GetDefaultRenderer()->CreateMeasuringContext());
        const auto ppi = metrics.GetPPI();
        gc->SetFont(
            gc->GetRenderer()->CreateFontAtDPI(metrics.GetFont(), wxRealPoint(ppi.x, ppi.y), *wxBLACK));
        std::vector<double> widths;
        double space = 0;
        gc->GetTextExtent(" ", &space, nullptr);
        wxString text;
        for (const auto& word : words) {
            double w = 0;
            gc->GetTextExtent(word, &w, nullptr);
            widths.push_back(w);
            if (!text.empty())
                text += " ";
            text += word;
        }
        bool beats_greedy = false;
        int verified = 0;
        for (int width = 160; width <= 400; width += 2) {
            double best = std::numeric_limits<double>::infinity();
            std::function<void(int, int, double)> enumerate = [&](int start, int previous, double cost) {
                if (start == int(words.size())) {
                    best = std::min(best, cost);
                    return;
                }
                double natural = 0;
                for (int end = start; end < int(words.size()); ++end) {
                    natural += widths[end] + (end > start ? space : 0);
                    const int gaps = end - start;
                    const double difference = width - natural;
                    double ratio =
                        difference / std::max(1.0, gaps * space * (difference < 0 ? 1.0 / 3.0 : 0.5));
                    if (end + 1 == int(words.size()) && difference >= 0)
                        ratio = 0;
                    const double badness = 100 * std::pow(std::abs(ratio), 3);
                    if (ratio < -1 || badness > 100)
                        continue;
                    const int fitness = ratio < -0.5 ? 0 : ratio <= 0.5 ? 1 : ratio <= 1 ? 2 : 3;
                    const bool runt = end + 1 == int(words.size()) && start > 0 && end == start;
                    enumerate(end + 1, fitness,
                              cost + std::pow(10 + badness, 2) +
                                  (std::abs(previous - fitness) > 1 ? 10000 : 0) + (runt ? 5000 : 0));
                }
            };
            enumerate(0, 1, 0);
            if (!std::isfinite(best))
                continue;
            const auto fit = layout_paragraph(metrics, text, width, "no-patterns");
            if (std::abs(fit.demerits - best) > 0.001)
                fail(__LINE__);
            ++verified;
            int greedy = 0;
            double length = 0;
            while (greedy < int(words.size()) && length + widths[greedy] + (greedy ? space : 0) <= width) {
                length += widths[greedy] + (greedy ? space : 0);
                ++greedy;
            }
            beats_greedy |= int(fit.lines.front().runs.size()) != greedy;
        }
        if (!verified || !beats_greedy)
            fail(__LINE__);
        std::cout << "Paragraph oracle: " << verified
                  << " globally optimal fits; differs from greedy=" << beats_greedy << "\n";
        const std::vector<TextFragment> spans = {{"I begynnelsen skapade Gud himmel och jord.", "1", 0},
                                                 {ui::utf8("Och Gud såg att det var gott."), "2", 1}};
        const auto paragraph = layout_paragraph(metrics, spans, 280, "sv");
        int markers = 0;
        wxString recovered;
        for (const auto& line : paragraph.lines)
            for (std::size_t i = 0; i < line.runs.size(); ++i) {
                const auto& run = line.runs[i];
                if (run.marker) {
                    ++markers;
                    continue;
                }
                auto token = run.text;
                if (line.hyphenated && i + 1 == line.runs.size())
                    token.RemoveLast();
                recovered += token;
            }
        wxString expected;
        for (const auto& span : spans) {
            auto source = span.text;
            source.Replace(" ", "");
            expected += source;
        }
        if (markers != 2 || recovered != expected)
            fail(__LINE__);
    }
    // Repeated words must retain native metrics across font size, style and script changes.
    for (const int size : {19, 24, 19}) {
        for (const bool bold : {false, true, false}) {
            auto font = body_font(size);
            if (bold)
                font.SetWeight(wxFONTWEIGHT_BOLD);
            metrics.SetFont(font);
            const auto fit =
                layout_paragraph(metrics, std::vector<TextFragment>{{ui::utf8("Mmmm שלום Λόγος"), "7", 0}},
                                 2000, "no-patterns");
            std::unique_ptr<wxGraphicsContext> gc(
                wxGraphicsRenderer::GetDefaultRenderer()->CreateMeasuringContext());
            const auto ppi = metrics.GetPPI();
            if (fit.lines.size() != 1)
                fail(__LINE__);
            for (const auto& line : fit.lines)
                for (const auto& run : line.runs) {
                    auto chosen = font;
                    if (run.marker)
                        chosen.SetFractionalPointSize(font.GetFractionalPointSize() * 0.57);
                    else if (run.text == ui::utf8("שלום"))
                        chosen.SetFaceName("Noto Serif Hebrew");
                    gc->SetFont(
                        gc->GetRenderer()->CreateFontAtDPI(chosen, wxRealPoint(ppi.x, ppi.y), *wxBLACK));
                    double expected = 0;
                    gc->GetTextExtent(run.text, &expected, nullptr);
                    if (run.marker) {
                        gc->SetFont(
                            gc->GetRenderer()->CreateFontAtDPI(font, wxRealPoint(ppi.x, ppi.y), *wxBLACK));
                        double space = 0;
                        gc->GetTextExtent(" ", &space, nullptr);
                        expected += std::max(1.0, space) * 0.5;
                    }
                    if (std::abs(run.width - expected) > 0.01)
                        fail(__LINE__);
                }
        }
    }
    metrics.SetFont(body_font(19));
    if (hyphenation_points("begynnelsen", "sv") != std::vector<int>{2, 5, 8} ||
        hyphenation_points("beginning", "en") != std::vector<int>{2, 5} ||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"), "el") != std::vector<int>{2, 4, 5} ||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"), "el") != std::vector<int>{2, 4, 5})
        fail(__LINE__);
    if (!hyphenation_points("project", "en").empty() ||
        hyphenation_points("associate", "en") != std::vector<int>{2, 4} ||
        hyphenation_points("obligatory", "en") != std::vector<int>{5, 6})
        fail(__LINE__);
    const auto optical = layout_paragraph(metrics, wxString::FromUTF8("“Guds ord.”"), 280, "sv");
    if (optical.lines.front().left_protrusion <= 0 || optical.lines.back().right_protrusion <= 0)
        fail(__LINE__);
    {
        // Ragged lines keep natural spaces; verse turnovers start at the hanging indent.
        const auto psalm =
            ui::utf8("Om jag ock vandrar i dödsskuggans dal, fruktar jag intet ont, ty du är med "
                     "mig; din käpp och stav, de trösta mig.");
        const double space = metrics.GetTextExtent(" ").x;
        const auto verse = layout_paragraph(metrics, {{psalm, "4", 0}}, 280, "sv", {true, 30});
        if (verse.lines.size() < 2)
            fail(__LINE__);
        for (std::size_t i = 0; i < verse.lines.size(); ++i) {
            const auto& line = verse.lines[i];
            if (line.justified || std::abs(line.space - space) > 1 || line.width > 281)
                fail(__LINE__);
            if ((i == 0 ? 0 : 30) - line.runs.front().x > space)
                fail(__LINE__);
        }
        // A single word alone on the last line is avoided when another fit exists.
        if (verse.lines.back().runs.size() == 1)
            fail(__LINE__);
        // HERREN is set in small capitals after its first letter, and never divided.
        const auto name = layout_paragraph(metrics, wxString("HERREN"), 280, "sv");
        if (name.lines.front().runs.front().width >= metrics.GetTextExtent("HERREN").x ||
            !hyphenation_points("HERRENS", "sv").empty())
            fail(__LINE__);
    }
    settings_.theme = Theme::Light;
    set_parallel("");
    settings_.word_study = false;
    settings_.text_zoom = 100;
    apply_settings(false);
    scripture_->center_passage();
    {
        // A wide single column keeps a reading measure of two and a half alphabets.
        const auto image = render();
        const auto colors = palette(settings_.theme);
        int first = image.GetWidth(), last = -1;
        for (int y = scripture_->FromDIP(160); y < image.GetHeight() - scripture_->FromDIP(90); ++y)
            for (int x = 0; x < image.GetWidth(); ++x)
                if (image.GetRed(x, y) != colors.paper.Red() ||
                    image.GetGreen(x, y) != colors.paper.Green() ||
                    image.GetBlue(x, y) != colors.paper.Blue()) {
                    first = std::min(first, x);
                    last = std::max(last, x);
                }
        const double alphabet = metrics.GetTextExtent("abcdefghijklmnopqrstuvwxyz").x;
        // The ink also spans the verse bars and their gutter, left of the text.
        // Allow the page margins when the CI display cannot fit the full column.
        const double minimum = std::min(2.2 * alphabet, double(image.GetWidth() - scripture_->FromDIP(110)));
        if (last - first + 1 < minimum || last - first + 1 > 2.5 * alphabet + scripture_->FromDIP(60))
            fail(__LINE__);
        save(image, "-single");
    }
    // Every scheme paints the reader and the settings page in its own paper, light and dark.
    const auto each_scheme = [&](const std::function<void(const wxString&, const wxColour&)>& check) {
        for (std::size_t i = 0; i < scheme_names.size(); ++i)
            for (const auto theme : {Theme::Light, Theme::Dark}) {
                settings_.color_scheme = ColorScheme(i);
                settings_.theme = theme;
                apply_settings(false);
                check(wxString::Format("-%zu-%s", i, theme == Theme::Dark ? "dark" : "light"),
                      palette(ColorScheme(i), theme == Theme::Dark).paper);
            }
        settings_.color_scheme = Settings{}.color_scheme;
        settings_.theme = Theme::Light;
        apply_settings(false);
    };
    each_scheme([&](const wxString& suffix, const wxColour& paper) {
        const auto image = render();
        if (image.GetRed(0, 0) != paper.Red() || image.GetGreen(0, 0) != paper.Green() ||
            image.GetBlue(0, 0) != paper.Blue())
            fail(__LINE__);
        save(image, "-reader" + suffix);
    });
    show_settings();
    const auto settings_snapshot = [&](MainFrame* frame, const wxString& suffix) {
        if (screenshot_path.empty())
            return;
        frame->Update();
        wxBitmap bitmap(frame->GetClientSize());
        wxClientDC window(frame);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), "-settings" + suffix);
    };
    const auto select_settings = [&](SettingsCategory category) {
        auto* categories = static_cast<wxListBox*>(
            wxWindow::FindWindowByName(ui::utf8("Inställningskategorier"), current_page()));
        auto* sections = static_cast<wxSimplebook*>(
            wxWindow::FindWindowByName(ui::utf8("Inställningar för vald kategori"), current_page()));
        if (!categories || !sections || categories->GetCount() != 6 || sections->GetPageCount() != 6) {
            fail(__LINE__);
            return;
        }
        const auto found = std::ranges::find_if(settings_categories_, [category](const auto& entry) {
            return entry.first == category;
        });
        const int index = categories->FindString(ui::utf8(found->second));
        categories->SetSelection(index);
        wxCommandEvent select(wxEVT_LISTBOX, categories->GetId());
        select.SetInt(index);
        categories->GetEventHandler()->ProcessEvent(select);
        if (sections->GetSelection() != index || settings_category_ != category ||
            sections->GetPageText(index) != categories->GetString(index))
            fail(__LINE__);
        for (std::size_t i = 0; i < sections->GetPageCount(); ++i)
            if (sections->GetPage(i)->IsShown() != (int(i) == index))
                fail(__LINE__);
    };
    {
        for (const auto& [category, name] : settings_categories_)
            select_settings(category);
        select_settings(SettingsCategory::Calendar);
        settings_snapshot(this, "-calendar");
        select_settings(SettingsCategory::Import);
        auto* import_link = wxWindow::FindWindowByLabel(
            ui::utf8("Öppna Nerladdningsbara bibelböcker på bibel.se"), current_page());
        auto* source =
            wxWindow::FindWindowByName(ui::utf8("Huvudtext, Svenska 1917 med apokryfer"), current_page());
        if (!import_link || !import_link->IsShownOnScreen() || !source || source->IsShownOnScreen())
            fail(__LINE__);
        const auto import_button =
            wxWindow::FindWindowByLabel(ui::utf8("Importera båda PDF-filerna…"), current_page());
        const auto remove_button =
            wxWindow::FindWindowByLabel(ui::utf8("Ta bort importerad text…"), current_page());
        if (!import_button || !import_button->IsEnabled() || !remove_button || remove_button->IsEnabled())
            fail(__LINE__);
        settings_snapshot(this, "-import-empty");
        settings_.text_zoom = 125;
        apply_settings(false);
        auto* categories = wxWindow::FindWindowByName(ui::utf8("Inställningskategorier"), current_page());
        if (!categories ||
            static_cast<wxListBox*>(categories)->GetStringSelection() != ui::utf8("Importera PDF"))
            fail(__LINE__);
        about();
        back();
        categories = wxWindow::FindWindowByName(ui::utf8("Inställningskategorier"), current_page());
        if (!categories ||
            static_cast<wxListBox*>(categories)->GetStringSelection() != ui::utf8("Importera PDF"))
            fail(__LINE__);
        settings_.text_zoom = 100;
        apply_settings(false);
        select_settings(SettingsCategory::Sources);
    }
    {
        select_settings(SettingsCategory::Appearance);
        auto* size = static_cast<wxChoice*>(wxWindow::FindWindowByName("TEXTSTORLEK", current_page()));
        wxCommandEvent change(wxEVT_CHOICE, size->GetId());
        change.SetInt(6); // 150 percent, also used by the menu.
        size->GetEventHandler()->ProcessEvent(change);
        wxTheApp->ProcessPendingEvents();
        size = static_cast<wxChoice*>(wxWindow::FindWindowByName("TEXTSTORLEK", current_page()));
        if (settings_.text_zoom != 150 || text_zoom != 150 || size->GetSelection() != 6 ||
            user_.load().text_zoom != 150 || settings_category_ != SettingsCategory::Appearance)
            fail(__LINE__);
        settings_.text_zoom = 100;
        apply_settings(false);
        const auto previous_size = GetClientSize();
        SetClientSize(FromDIP(wxSize(520, 480)));
        Layout();
        wxTheApp->ProcessPendingEvents();
        settings_snapshot(this, "-appearance-narrow");
        SetClientSize(previous_size);
        Layout();
        select_settings(SettingsCategory::Sources);
    }
    {
        for (const auto* detail :
             {"Svenska · GT · NT · apokryfer", "Engelska · NT", "Grekiska · GT · apokryfer"})
            if (!wxWindow::FindWindowByLabel(ui::utf8(detail), current_page()))
                fail(__LINE__);
        settings_snapshot(this, "-sources");
        const auto preferences = settings_.primary_sources;
        const std::function<wxWindow*(wxWindow*, const wxString&)> named =
            [&](wxWindow* parent, const wxString& name) -> wxWindow* {
            for (auto* child : parent->GetChildren()) {
                if (child->GetName() == name)
                    return child;
                if (auto* found = named(child, name))
                    return found;
            }
            return nullptr;
        };
        auto* web = named(current_page(), ui::utf8("Huvudtext, World English Bible Classic"));
        auto* move =
            web ? named(web->GetParent(), ui::utf8("Flytta upp World English Bible Classic")) : nullptr;
        if (!web || !move || !move->IsEnabled())
            fail(__LINE__);
        else {
            wxCommandEvent click(wxEVT_BUTTON, move->GetId());
            move->GetEventHandler()->ProcessEvent(click);
            wxTheApp->ProcessPendingEvents();
            auto* moved = named(current_page(), ui::utf8("Huvudtext, World English Bible Classic"));
            if (settings_.primary_sources[1].source != "en-web" ||
                user_.load().primary_sources != settings_.primary_sources || !moved ||
                (moved->CanAcceptFocusFromKeyboard() && wxWindow::FindFocus() != moved))
                fail(__LINE__);
        }
        auto* greek = named(current_page(), ui::utf8("Huvudtext, Patriarkal grekiska 1904"));
        if (!greek || !greek->IsEnabled())
            fail(__LINE__);
        else {
            wxCommandEvent enable(wxEVT_CHECKBOX, greek->GetId());
            enable.SetInt(1);
            greek->GetEventHandler()->ProcessEvent(enable);
            wxTheApp->ProcessPendingEvents();
            const auto found =
                std::ranges::find(settings_.primary_sources, "grc-patriarchal", &SourcePreference::source);
            if (found == settings_.primary_sources.end() || !found->enabled)
                fail(__LINE__);
        }
        settings_.primary_sources = preferences;
        apply_settings(false);
        fill_settings(static_cast<wxScrolledWindow*>(current_page()));
        const auto previous_size = GetClientSize();
        SetClientSize(FromDIP(wxSize(520, 480)));
        Layout();
        wxTheApp->ProcessPendingEvents();
        settings_snapshot(this, "-sources-narrow");
        SetClientSize(previous_size);
        Layout();
    }
    select_settings(SettingsCategory::Speech);
    {
        const auto catalog = voice_catalog_;
        voice_catalog_["sv"] = {{"espeak", "Cached test voice"}};
        fill_settings(static_cast<wxScrolledWindow*>(current_page()));
        const auto* cached = static_cast<wxChoice*>(
            wxWindow::FindWindowByName(ui::utf8("Röst för uppläsning, svenska"), current_page()));
        if (!cached || cached->GetString(0) != "Cached test voice")
            fail(__LINE__);
        voice_catalog_ = catalog;
        fill_settings(static_cast<wxScrolledWindow*>(current_page()));
        auto real_speech = std::move(speech_);
        const auto previous_playback = playback_;
        const auto previous_voice = settings_.speech_voices.at("sv");
        const auto previous_rate = settings_.speech_rate;
        const auto previous_tracked = speech_tracked_;
        const Reading retained{ReadingKind::Gospel, {"John", {1, 1}, {1, 10}}, {}, "sv"};
        const auto key = reading_key(corpus_, retained);
        const auto previous_position = user_.position(key);
        const auto completed = user_.completed();
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        engine->held = {SpeechState::Paused, SpeechCue{0, 0, "John", "sv1917", {1, 4}, {1, 4}}, 0, 0.3};
        speech_ = std::move(held);
        playback_.start({retained});
        settings_.speech_voices.at("sv") = "espeak:sv+adam";
        speech_tracked_ = Tracked{"voice-preview-test", "Preview"};
        auto* settings_page = current_page();
        auto* rate = static_cast<wxChoice*>(
            wxWindow::FindWindowByName(ui::utf8("UPPLÄSNINGSHASTIGHET"), current_page()));
        wxCommandEvent change_rate(wxEVT_CHOICE, rate->GetId());
        change_rate.SetInt(3); // 1.25 times the voice's pace.
        rate->GetEventHandler()->ProcessEvent(change_rate);
        if (settings_.speech_rate != 125 || engine->speed != 1.25 || user_.load().speech_rate != 125)
            fail(__LINE__);
        preview_voice();
        const auto saved_position = user_.position(key);
        const auto sample = corpus_.verse("sv1917", "John", {3, 16});
        if (!voice_preview_ || !sample || engine->spoken.size() != 1 ||
            engine->spoken.front().display_text != sample->text || engine->spoken.front().cue ||
            engine->voice != settings_.speech_voices.at("sv") || !playback_.readings().empty() ||
            speech_tracked_ || !saved_position || saved_position->verse != VerseRef{1, 4} ||
            current_page() != settings_page)
            fail(__LINE__);
        change_rate.SetInt(6); // Changing speed during a preview restarts the sample.
        rate->GetEventHandler()->ProcessEvent(change_rate);
        if (!voice_preview_ || engine->speed != 2 || settings_.speech_rate != 200)
            fail(__LINE__);
        preview_voice();
        if (voice_preview_ || engine->held.state != SpeechState::Stopped)
            fail(__LINE__);
        preview_voice();
        engine->held.state = SpeechState::Completed;
        refresh_speech();
        wxTheApp->ProcessPendingEvents();
        if (voice_preview_ || user_.position(key) != saved_position || user_.completed() != completed ||
            current_page() != settings_page ||
            (voice_preview_button_ && voice_preview_button_->GetLabel() != ui::utf8("Prova rösten")))
            fail(__LINE__);
        preview_voice();
        select_settings(SettingsCategory::Sources);
        if (voice_preview_ || engine->held.state != SpeechState::Stopped)
            fail(__LINE__);
        select_settings(SettingsCategory::Speech);
        preview_voice();
        pause_for_navigation();
        if (voice_preview_ || engine->held.state != SpeechState::Stopped)
            fail(__LINE__);
        for (const std::string language : {"en", "el"}) {
            auto* choice = wxWindow::FindWindowByName(
                ui::utf8("Röst för uppläsning, ") + ui::speech_language_name(language), current_page());
            if (!choice || !choice->GetNextSibling()) {
                fail(__LINE__);
                continue;
            }
            preview_voice(language, choice->GetNextSibling());
            const auto source = corpus_.source_for_language(language, "John");
            const auto sample = corpus_.verse(source, "John", {3, 16});
            if (!voice_preview_ || voice_preview_language_ != language || !sample ||
                engine->spoken.size() != 1 || engine->spoken.front().language != language ||
                engine->spoken.front().display_text != sample->text ||
                engine->voice != settings_.speech_voices.at(language))
                fail(__LINE__);
        }
        set_speech_rate(125);
        if (!voice_preview_ || voice_preview_language_ != "el" || engine->spoken.front().language != "el")
            fail(__LINE__);
        stop_speech(false);
        if (user_.position(key) != saved_position || user_.completed() != completed)
            fail(__LINE__);
        settings_snapshot(this, "-speech-languages");
        if (previous_position)
            user_.position(key, *previous_position);
        else
            user_.reset_position(key);
        settings_.speech_voices.at("sv") = previous_voice;
        set_speech_rate(previous_rate);
        speech_ = std::move(real_speech);
        playback_ = previous_playback;
        speech_tracked_ = previous_tracked;
        refresh_speech();
    }
    select_settings(SettingsCategory::Appearance);
    each_scheme([&](const wxString& suffix, const wxColour& paper) {
        if (!current_page() || current_page()->GetBackgroundColour() != paper)
            fail(__LINE__);
        settings_snapshot(this, suffix);
    });
    {
        // Browsing Settings waits for a click; the menu action starts a check.
        updater_ = std::make_unique<Updater>(
            std::make_unique<SmokeUpdates>(),
            [this](const std::function<void()>& function) {
                CallAfter(function);
            },
            [this] {
                show_update_status();
            });
        show_update_status();
        select_settings(SettingsCategory::Updates);
        auto* button = update_button_.get();
        auto* text = static_cast<wxStaticText*>(update_text_.get());
        if (!button || !text || !button->IsShownOnScreen() || !button->IsEnabled() ||
            update_cancel_->IsShown() || updater_->status().state != UpdateStatus::State::Idle)
            fail(__LINE__);
        const auto reaches = [&](UpdateStatus::State state) {
            for (int i = 0; i < 1000 && updater_->status().state != state; ++i) {
                wxMilliSleep(5);
                wxTheApp->ProcessPendingEvents();
            }
            return updater_->status().state == state;
        };
        wxCommandEvent click(wxEVT_BUTTON, button->GetId());
        button->GetEventHandler()->ProcessEvent(click);
        if (button->IsEnabled() || !update_cancel_->IsShown())
            fail(__LINE__);
        wxCommandEvent cancel(wxEVT_BUTTON, update_cancel_->GetId());
        update_cancel_->GetEventHandler()->ProcessEvent(cancel);
        if (!reaches(UpdateStatus::State::Idle) || !button->IsEnabled() || update_cancel_->IsShown())
            fail(__LINE__);
        select_settings(SettingsCategory::Sources);
        const int update =
            menu_bar_->FindMenuItem(ui::utf8("Uppläsning"), ui::utf8("Sök efter uppdateringar…"));
        wxCommandEvent command(wxEVT_MENU, update);
        ProcessWindowEvent(command);
        button = update_button_.get();
        text = static_cast<wxStaticText*>(update_text_.get());
        if (settings_category_ != SettingsCategory::Updates || !button->IsShownOnScreen() ||
            !reaches(UpdateStatus::State::Available) ||
            button->GetLabel() != ui::utf8("Hämta version 9.9.9") || !text->GetLabel().Contains("2 MB"))
            fail(__LINE__);
        const auto previous_size = GetClientSize();
        SetClientSize(FromDIP(wxSize(520, 480)));
        Layout();
        wxTheApp->ProcessPendingEvents();
        if (!text->GetLabel().Contains("9.9.9"))
            fail(__LINE__);
        settings_snapshot(this, "-updates-narrow");
        SetClientSize(previous_size);
        Layout();
        update_clicked();
        if (!reaches(UpdateStatus::State::Ready) ||
            button->GetLabel() != ui::utf8("Starta om och installera"))
            fail(__LINE__);
        settings_.text_zoom = 125;
        apply_settings(false);
        button = update_button_.get();
        if (settings_category_ != SettingsCategory::Updates ||
            button->GetLabel() != ui::utf8("Starta om och installera"))
            fail(__LINE__);
        settings_snapshot(this, "-updates");
        settings_.text_zoom = 100;
        apply_settings(false);
        auto* releases = wxWindow::FindWindowByLabel(ui::utf8("Utgåvor (rekommenderas)"), current_page());
        wxCommandEvent channel(wxEVT_RADIOBUTTON, releases->GetId());
        releases->GetEventHandler()->ProcessEvent(channel);
        if (updater_->status().state != UpdateStatus::State::Idle ||
            update_button_->GetLabel() != ui::utf8("Sök efter uppdateringar"))
            fail(__LINE__);
        // Leaving Settings during a check and reopening it retains the result.
        update_clicked();
        about();
        if (!reaches(UpdateStatus::State::Available))
            fail(__LINE__);
        ProcessWindowEvent(command);
        if (settings_category_ != SettingsCategory::Updates ||
            update_button_->GetLabel() != ui::utf8("Hämta version 9.9.9"))
            fail(__LINE__);
        back();
        back();
        settings_.text_zoom = 125;
        apply_settings(false);
        if (!current_page()->IsDescendant(update_button_.get()) ||
            update_button_->GetLabel() != ui::utf8("Hämta version 9.9.9"))
            fail(__LINE__);
        settings_.text_zoom = 100;
        apply_settings(false);
        updater_->reset();
    }
    close_page();
    set_parallel("el");
    apply_settings(false);
    scripture_->center_passage();
    const double start = scripture_->scroll_position();
    scripture_->scroll_by(0.375);
    if (std::abs(scripture_->scroll_position() - start - 0.375) > 0.001)
        fail(__LINE__);
    scripture_->scroll_by(-0.375);
    const auto viewport = scripture_->GetClientSize();
    const auto first = render();
    scripture_->scroll_by(17);
    if (std::abs(scripture_->scroll_position() - start - 17) > 0.001)
        fail(__LINE__);
    const auto second = render();
    std::size_t equal = 0, total = 0;
    for (int y = 30; y < viewport.y - 60; ++y)
        for (int x = 0; x < viewport.x; ++x) {
            ++total;
            if (first.GetRed(x, y + 17) == second.GetRed(x, y) &&
                first.GetGreen(x, y + 17) == second.GetGreen(x, y) &&
                first.GetBlue(x, y + 17) == second.GetBlue(x, y))
                ++equal;
        }
    if (total == 0 || double(equal) / total < 0.995)
        fail(__LINE__);
#ifndef __APPLE__
    // Windows/Linux use the wheel timer; Cocoa scrolls immediately.
    // A wheel animation must not pull Home/End back to its old target.
    for (const int key : {WXK_HOME, WXK_END}) {
        scripture_->center_passage();
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL);
        wheel.m_wheelRotation = -120;
        wheel.m_wheelDelta = 120;
        wheel.m_linesPerAction = 3;
        scripture_->GetEventHandler()->ProcessEvent(wheel);
        const auto before_tick = scripture_->scroll_position();
        scripture_->advance_scroll();
        if (scripture_->scroll_position() <= before_tick)
            fail(__LINE__);
        wxKeyEvent navigate(wxEVT_CHAR_HOOK);
        navigate.m_keyCode = key;
        scripture_->GetEventHandler()->ProcessEvent(navigate);
        const auto position = scripture_->scroll_position();
        for (int tick = 0; tick < 3; ++tick)
            scripture_->advance_scroll();
        if (scripture_->scroll_position() != position || (key == WXK_HOME && position != 0))
            fail(__LINE__);
    }
#endif
    for (auto theme : {Theme::Light, Theme::Dark, Theme::System})
        for (auto mode : {"", "el", "en"}) {
            settings_.theme = theme;
            set_parallel(mode);
            apply_settings(false);
            scripture_->center_passage();
            const auto size = scripture_->GetClientSize();
            if (size.x < 1 || size.y < 1) {
                fail(__LINE__);
                continue;
            }
            const auto image = render();
            const auto cached_stats = scripture_->render_stats();
            render();
            const auto reused_stats = scripture_->render_stats();
            if (reused_stats.tile_builds != cached_stats.tile_builds ||
                reused_stats.layout_builds != cached_stats.layout_builds ||
                reused_stats.tile_hits <= cached_stats.tile_hits ||
                reused_stats.tile_bytes > std::size_t{32} * 1024 * 1024)
                fail(__LINE__);
            // Native text and cached alpha tiles retain the same typography.
            // Allow native antialiasing differences, but reject lost/clipped/moved text.
            for (double scale : {1.0, 2.0}) {
                const auto direct = render({}, false, scale);
                const auto cached = render({}, true, scale);
                const auto bytes = std::size_t(direct.GetWidth()) * direct.GetHeight() * 3;
                double difference = 0;
                for (std::size_t i = 0; i < bytes; ++i)
                    difference += std::abs(int(direct.GetData()[i]) - int(cached.GetData()[i]));
                if (difference / bytes > 1.5) {
                    std::cerr << "Cached text pixel difference=" << difference / bytes << ", scale=" << scale
                              << '\n';
                    const auto suffix = wxString::Format("-text-%d-%s-%g", int(theme), wxString(mode), scale);
                    save(direct, suffix + "-direct");
                    save(cached, suffix + "-cached");
                    fail(__LINE__);
                }
            }
            const auto colors = palette(settings_.theme);
            std::size_t changed = 0;
            for (int y = 0; y < image.GetHeight(); ++y)
                for (int x = 0; x < image.GetWidth(); ++x)
                    if (image.GetRed(x, y) != colors.paper.Red() ||
                        image.GetGreen(x, y) != colors.paper.Green() ||
                        image.GetBlue(x, y) != colors.paper.Blue())
                        ++changed;
            if (changed < 200 || scripture_->cached_rows() > 192)
                fail(__LINE__);
            if (!screenshot_path.empty() && std::string(mode) == "el" &&
                (theme == Theme::Light || theme == Theme::Dark)) {
                const auto page = render({size.x, 1100});
                if (theme == Theme::Light)
                    ok = page.SaveFile(screenshot_path, wxBITMAP_TYPE_PNG) && ok;
                else
                    save(page, "-dark");
            }
        }
    const auto original_size = GetSize();
    SetSize(FromDIP(wxSize(520, 650)));
    Layout();
    root_->Layout();
    set_parallel("en");
    settings_.theme = Theme::Dark;
    apply_settings(false);
    scripture_->center_passage();
    const auto narrow_size = scripture_->GetClientSize();
    if (narrow_size.x < 100 || narrow_size.y < 100)
        fail(__LINE__);
    else
        save(render(), "-narrow");
    SetSize(original_size);
    Layout();
    // The day page keeps the toolbar, with the reader's controls dimmed.
    show_readings();
    const auto ellipsis = ui::utf8("…");
    const auto sample = ui::text_preview(std::string(100, 'a') + " " + std::string(100, 'b'));
    if (sample != ui::utf8(std::string(100, 'a')) + ellipsis ||
        ui::text_preview("\n Å ä ö \t text  ") != ui::utf8("Å ä ö text"))
        fail(__LINE__);
    if (!day_.readings.empty() && day_.readings.front().can_open()) {
        const auto preview =
            ui::reading_tooltip(corpus_, corpus_.localize(reader_reading(day_.readings.front())));
        if (preview.empty() || std::none_of(home_rows_.begin(), home_rows_.end(), [&](auto* row) {
                return row->GetToolTipText().StartsWith(preview);
            }))
            fail(__LINE__);
    }
    for (auto* child : home_content_->GetChildren())
        if (auto* control = dynamic_cast<TextButton*>(child);
            control && control->GetLabel().Contains("Lyssna"))
            fail(__LINE__);
    if (!bar_->IsShown() || play_->IsEnabled() || stop_->IsEnabled() ||
        back_->IsEnabled() != history_.can_back() || panes_.front().second->IsEnabled() ||
        address_->GetLabel() != ui::utf8("Dagens läsningar"))
        fail(__LINE__);
    // Repeated layout keeps long explanations wrapped in the centered column.
    layout_home();
    layout_home();
    for (const auto& [label, text, beside] : home_labels_)
        if (text.length() > 100 && !label->GetLabel().Contains("\n"))
            fail(__LINE__);
    // The reading plans have their own page; a marked part shows as read.
    const auto plan_tiles = [&] {
        std::vector<TextButton*> tiles;
        if (current_page())
            for (auto* panel : current_page()->GetChildren())
                for (auto* child : panel->GetChildren())
                    if (auto* tile = dynamic_cast<TextButton*>(child);
                        tile && tile->GetName().Contains(", Del "))
                        tiles.push_back(tile);
        return tiles;
    };
    std::size_t plan_parts = 0;
    for (const auto& plan : reading_plans())
        plan_parts += plan.parts.size();
    show_plans();
    if (current_page_title() != ui::utf8("Läsplaner") || plan_tiles().size() != plan_parts ||
        readings_->IsShown())
        fail(__LINE__);
    user_.complete(plan_key(reading_plans().front(), 0));
    user_.position(plan_key(reading_plans().front(), 1), {0, "Matt", "sv1917", {7, 1}, 23});
    fill_plans(static_cast<wxScrolledWindow*>(current_page()));
    if (plan_tiles()[1]->GetLabel() != ui::utf8("2 · 23 %"))
        fail(__LINE__);
    // Wide windows show ten parts per row; narrow windows wrap without clipping labels.
    const auto plans_size = GetSize();
    const auto plan_columns = [&] {
        const auto tiles = plan_tiles();
        return std::ranges::count_if(tiles, [&](auto* tile) {
            return tile->GetPosition().y == tiles.front()->GetPosition().y;
        });
    };
    const auto check_plan_fit = [&] {
        for (auto* tile : plan_tiles()) {
            auto* panel = tile->GetParent();
            if (tile->GetSize().x < tile->GetBestSize().x ||
                tile->GetRect().GetRight() >= panel->GetClientSize().x ||
                panel->GetRect().GetRight() >= current_page()->GetClientSize().x)
                fail(__LINE__);
        }
    };
    SetSize(FromDIP(wxSize(1200, 800)));
    Layout();
    root_->Layout();
    if (plan_columns() != 10)
        fail(__LINE__);
    check_plan_fit();
    SetSize(FromDIP(wxSize(520, 650)));
    Layout();
    root_->Layout();
    if (plan_columns() < 1 || plan_columns() >= 10)
        fail(__LINE__);
    check_plan_fit();
    const auto plans_zoom = settings_.text_zoom;
    settings_.text_zoom = 200;
    apply_settings(false);
    check_plan_fit();
    settings_.text_zoom = plans_zoom;
    apply_settings(false);
    SetSize(plans_size);
    Layout();
    root_->Layout();
    if (!screenshot_path.empty()) {
        root_->Layout();
        current_page()->Refresh();
        current_page()->Update();
        Update();
        wxBitmap bitmap;
        bitmap.CreateWithLogicalSize(GetClientSize(), GetContentScaleFactor());
        wxClientDC window(this);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, GetClientSize().x, GetClientSize().y, &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), "-plans-progress");
    }
    if (plan_tiles().size() != plan_parts || !plan_tiles().front()->GetName().EndsWith(ui::utf8("läst")) ||
        plan_tiles()[1]->GetName().EndsWith(ui::utf8("läst")) ||
        plan_tiles().front()->GetToolTipText() != ui::utf8("Matt 1–5") ||
        plan_tiles()[4]->GetToolTipText() != ui::utf8("Matt 16–19"))
        fail(__LINE__);
    user_.forget("plan:");
    fill_plans(static_cast<wxScrolledWindow*>(current_page()));
    if (plan_tiles().empty() || plan_tiles().front()->GetName().EndsWith(ui::utf8("läst")))
        fail(__LINE__);
    // All plan parts have native button semantics and a descriptive accessible name.
    for (auto* tile : plan_tiles())
        if (!tile->AcceptsFocus() || !tile->GetName().Contains("Del ") || !tile->GetName().Contains(", "))
            fail(__LINE__);
#if wxUSE_ACCESSIBILITY
    for (auto* tile : plan_tiles()) {
        wxString name;
        wxAccRole role = wxROLE_NONE;
        auto* accessible = tile->GetAccessible();
        if (!accessible || accessible->GetName(wxACC_SELF, &name) != wxACC_OK ||
            accessible->GetRole(wxACC_SELF, &role) != wxACC_OK || name != tile->GetName() ||
            role != wxROLE_SYSTEM_PUSHBUTTON)
            fail(__LINE__);
    }
#endif
    if (const auto tiles = plan_tiles(); !tiles.empty()) {
        wxCommandEvent open_part(wxEVT_BUTTON, tiles.back()->GetId());
        tiles.back()->GetEventHandler()->ProcessEvent(open_part);
        wxTheApp->ProcessPendingEvents();
    }
    if (current_page() || !scripture_->IsShown() || !tracked_)
        fail(__LINE__);
    back();
    if (navigation_.current().screen != Screen::Plans)
        fail(__LINE__);
    back();
    if (!readings_->IsShown())
        fail(__LINE__);
    // Custom toolbar controls activate once, on key release, without mouse input.
    for (const int key : {WXK_SPACE, WXK_RETURN}) {
        int activations = 0;
        auto* button = new SymbolButton(
            root_, Symbol::Next, "Keyboard test",
            [&] {
                ++activations;
            },
            true);
#if wxUSE_ACCESSIBILITY
        wxAccRole role = wxROLE_NONE;
        long state = 0;
        button->checked(true);
        auto* accessible = button->GetAccessible();
        if (!accessible || accessible->GetRole(wxACC_SELF, &role) != wxACC_OK ||
            role != wxROLE_SYSTEM_CHECKBUTTON || accessible->GetState(wxACC_SELF, &state) != wxACC_OK ||
            !(state & wxACC_STATE_SYSTEM_CHECKED))
            fail(__LINE__);
        button->checked(false);
        if (accessible &&
            (accessible->GetState(wxACC_SELF, &state) != wxACC_OK || (state & wxACC_STATE_SYSTEM_CHECKED)))
            fail(__LINE__);
#endif
        wxKeyEvent down(wxEVT_KEY_DOWN), up(wxEVT_KEY_UP);
        down.m_keyCode = up.m_keyCode = key;
        button->GetEventHandler()->ProcessEvent(down);
        if (activations != 0)
            fail(__LINE__);
        button->GetEventHandler()->ProcessEvent(up);
        if (activations != 1 || !button->AcceptsFocus())
            fail(__LINE__);
        button->Enable(false);
        button->GetEventHandler()->ProcessEvent(down);
        button->GetEventHandler()->ProcessEvent(up);
        if (activations != 1)
            fail(__LINE__);
        delete button;
    }
    // Right-column symbols toggle, and the Visa menu follows them.
    set_parallel("");
    toggle_pane("split");
    if (!settings_.split_view || settings_.word_study)
        fail(__LINE__);
    toggle_pane("study");
    if (settings_.split_view || !settings_.word_study ||
        !menu_bar_->IsChecked(menu_bar_->FindMenuItem("Visa", "Ordstudium")))
        fail(__LINE__);
    toggle_pane("study");
    if (settings_.word_study || !menu_bar_->IsChecked(menu_bar_->FindMenuItem("Visa", "En text")))
        fail(__LINE__);
    if (panes_.size() != 2 || panes_[1].first != "split")
        fail(__LINE__);
    // Preferences choose each pane; chapter choices do not change those saved lists.
    {
        const auto saved = settings_;
        set_parallel("en");
        apply_settings(false);
        const Reading reading{ReadingKind::Gospel, {"John", {1, 1}, {2, 2}}};
        open_reading(reading);
        if (scripture_->chapter_source(1, 0) != "sv1917" || scripture_->chapter_source(1, 1) != "en-web")
            fail(__LINE__);
        // Explicit chapter choices remain selectable even when disabled in preferences or duplicated.
        scripture_->chapter_source(1, 1, "sv1917");
        if (scripture_->chapter_source(1, 0) != "sv1917" || scripture_->chapter_source(1, 1) != "sv1917" ||
            scripture_->chapter_source(2, 1) != "en-web")
            fail(__LINE__);
        save(render(), "-chapter-same-source");
        scripture_->chapter_source(1, 1, "");
        if (scripture_->chapter_source(1, 1) != "en-web")
            fail(__LINE__);
        scripture_->chapter_source(1, 0, "grc-patriarchal");
        scripture_->chapter_source(1, 1, "sv1917");
        if (scripture_->chapter_source(1, 0) != "grc-patriarchal" ||
            scripture_->chapter_source(1, 1) != "sv1917" || scripture_->chapter_source(2, 0) != "sv1917" ||
            !play_->IsEnabled() || controls().playback_action != PlaybackAction::StartReading ||
            settings_.primary_sources != saved.primary_sources)
            fail(__LINE__);
        save(render(), "-chapter-sources");
        scripture_->chapter_source(1, 0, "en-web");
        if (scripture_->primary_language() != "en" || !play_->IsEnabled())
            fail(__LINE__);
        open_reading(reading);
        if (scripture_->chapter_source(1, 0) != "sv1917" || scripture_->chapter_source(1, 1) != "en-web")
            fail(__LINE__);
        settings_.primary_sources = decode_sources("en-web=1", settings_.primary_sources);
        apply_settings(false);
        if (scripture_->chapter_source(1, 0) != "en-web" || !play_->IsEnabled())
            fail(__LINE__);
        settings_.primary_sources = decode_sources("grc-patriarchal=1", settings_.primary_sources);
        apply_settings(false);
        if (scripture_->chapter_source(1, 0) != "grc-patriarchal" || !play_->IsEnabled())
            fail(__LINE__);
        settings_ = saved;
        apply_settings(false);
        open_fresh_psalm();
    }
    {
        const auto data = data_ / "multilingual-speech";
        UserDb profile(data / "user.db");
        auto candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                     std::make_unique<SmokeUpdates>(), true);
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        candidate->speech_ = std::move(held);
        candidate->Show();
        candidate->open_reading({ReadingKind::OldTestament, {"1Esd", {1, 1}, {1, 2}}});
        if (!candidate->play_->IsEnabled() || candidate->scripture_->primary_language() != "en")
            fail(__LINE__);
        candidate->play_or_pause();
        if (engine->spoken.size() != 2 || engine->spoken.front().language != "en")
            fail(__LINE__);
        candidate->stop_speech(false);
        candidate->scripture_->select_verses({1, 2}, {1, 2});
        candidate->play_or_pause();
        if (engine->spoken.size() != 1 || engine->spoken.front().language != "en" ||
            engine->spoken.front().cue->verse != VerseRef{1, 2})
            fail(__LINE__);
        candidate->stop_speech(false);
        candidate->scripture_->clear_selection();
        candidate->open_reading({ReadingKind::OldTestament, {"1Esd", {1, 1}, {1, 2}}});
        candidate->stop_speech();
        candidate->scripture_->chapter_source(1, 0, "grc-lxx");
        candidate->play_or_pause();
        if (engine->spoken.size() != 2 || engine->spoken.front().language != "el" ||
            engine->spoken.front().display_text != corpus_.verse("grc-lxx", "1Esd", {1, 1})->text)
            fail(__LINE__);
        candidate->stop_speech(false);
        candidate->Hide();
    }
    // Imported speech follows the displayed chapter editions, also after Pause and selection playback.
    {
        const auto data = data_ / u8"speech-sources-åäö";
        const auto imported = data / "content/srb2026.db";
        std::filesystem::create_directories(imported.parent_path());
        {
            auto db = storage::open(imported, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
            storage::Transaction transaction(db.get());
            storage::exec(db.get(),
                          "CREATE TABLE content_metadata(key TEXT,value TEXT);"
                          "INSERT INTO content_metadata VALUES('srb2026',printf('%064d',0));"
                          "CREATE TABLE imported_verse(book TEXT,chapter INTEGER,verse INTEGER,text TEXT,"
                          "PRIMARY KEY(book,chapter,verse))");
            storage::Statement attach(db.get(), "ATTACH DATABASE ? AS reference");
            attach.text(1, (resources_ / "corpus/osb-reference.db").string());
            attach.row();
            storage::exec(db.get(),
                          "INSERT INTO imported_verse SELECT book,chapter,verse,"
                          "'SRB test '||book||' '||chapter||':'||verse FROM reference.source_verse "
                          "JOIN reference.source ON source=source.id WHERE code='sv-srb2026';"
                          // Unmapped verses are irrelevant here; retain the import's required row count.
                          "WITH RECURSIVE padding(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM padding "
                          "WHERE n<31171-(SELECT count(*) FROM imported_verse)) "
                          "INSERT INTO imported_verse SELECT 'Gen',999,n,'Test padding' FROM padding");
            transaction.commit();
        }
        CorpusDb corpus(resources_ / "corpus/corpus.db");
        const auto& before_import = corpus.coordinates(srb_source, "John");
        corpus.load_imported(imported);
        const auto& installed_coordinates = corpus.coordinates(srb_source, "John");
        const auto saved_coordinates = installed_coordinates;
        if (!before_import.empty() || installed_coordinates.empty() ||
            corpus.source_books(srb_source).size() != 66)
            fail(__LINE__);
        UserDb profile(data / "user.db");
        auto candidate = std::make_unique<MainFrame>(corpus, profile, date, resources_, data,
                                                     std::make_unique<SmokeUpdates>(), true);
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        candidate->speech_ = std::move(held);
        candidate->settings_.primary_sources =
            decode_sources("sv-srb2026=1,sv1917=1", candidate->settings_.primary_sources);
        candidate->apply_settings(false);
        candidate->Show();
        const Reading reading{ReadingKind::Gospel, {"John", {1, 50}, {2, 3}}};
        candidate->open_reading(reading);
        candidate->scripture_->chapter_source(2, 0, "sv1917");
        const auto check_sources = [&](bool srb_first, VerseRef last) {
            if (engine->spoken.empty() ||
                engine->spoken.front().display_text !=
                    (srb_first ? "SRB test John 1:50"
                               : corpus.parallel_verse(osb_reference, "sv1917", "John", {1, 50})->text) ||
                engine->spoken.back().display_text !=
                    (srb_first ? corpus.verse("sv1917", "John", last)->text
                               : "SRB test John 2:" + std::to_string(last.verse)) ||
                engine->spoken.back().cue != SpeechCue{0, 0, "John", osb_reference, last, last} ||
                candidate->scripture_->chapter_source(1, 0) != (srb_first ? srb_source : "sv1917") ||
                candidate->scripture_->chapter_source(2, 0) != (srb_first ? "sv1917" : srb_source))
                fail(__LINE__);
        };
        candidate->play_or_pause();
        check_sources(true, {2, 3});
        candidate->toggle_pause();
        candidate->toggle_pause();
        check_sources(true, {2, 3});
        candidate->stop_speech(false);
        candidate->scripture_->select_verses({1, 50}, {2, 2});
        candidate->play_or_pause();
        check_sources(true, {2, 2});
        candidate->stop_speech(false);
        candidate->scripture_->chapter_source(1, 0, "sv1917");
        candidate->scripture_->chapter_source(2, 0, srb_source);
        candidate->scripture_->select_verses({1, 50}, {2, 2});
        const auto marked = candidate->scripture_->marked_reading();
        if (!marked)
            fail(__LINE__);
        else {
            candidate->listen_to_selection(*marked);
            check_sources(false, {2, 2});
        }
        candidate->stop_speech(false);
        candidate->open_reading(reading);
        if (candidate->scripture_->chapter_source(1, 0) != srb_source ||
            candidate->scripture_->chapter_source(2, 0) != srb_source)
            fail(__LINE__);
        for (auto& source : candidate->settings_.primary_sources)
            source.enabled = source.source == srb_source;
        candidate->apply_settings(false);
        candidate->open_reading({ReadingKind::OldTestament, {"Tob", {1, 1}, {1, 1}}});
        if (candidate->play_->IsEnabled())
            fail(__LINE__);
        candidate->scripture_->chapter_source(1, 0, "sv1917");
        if (!candidate->play_->IsEnabled())
            fail(__LINE__);
        candidate->settings_.primary_sources =
            decode_sources("sv-srb2026=1,en-lxx2012=1,sv1917=0,en-web=0,grc-lxx=0,grc-patriarchal=0",
                           candidate->settings_.primary_sources);
        candidate->apply_settings(false);
        profile.bookmark(srb_source, {"John", {1, 50}, {1, 51}});
        const auto bookmarks = profile.bookmarks(corpus);
        const auto completed = profile.completed();
        const auto backup = data / "srb-backup.db";
        std::filesystem::copy_file(imported, backup);
        candidate->settings_category_ = SettingsCategory::Import;
        candidate->show_settings();
        const auto check_import_controls = [&](bool installed) {
            if (corpus.source_books(srb_source).empty() == installed)
                fail(__LINE__);
            const auto page = candidate->current_page();
            auto* import_button = wxWindow::FindWindowByLabel(ui::utf8("Importera båda PDF-filerna…"), page);
            auto* remove_button = wxWindow::FindWindowByLabel(ui::utf8("Ta bort importerad text…"), page);
            auto* link =
                wxWindow::FindWindowByLabel(ui::utf8("Öppna Nerladdningsbara bibelböcker på bibel.se"), page);
            if (!import_button || !remove_button || !link || !import_button->IsShownOnScreen() ||
                !remove_button->IsShownOnScreen() || !link->IsShownOnScreen() ||
                import_button->IsEnabled() == installed || remove_button->IsEnabled() != installed)
                fail(__LINE__);
        };
        check_import_controls(true);
        settings_snapshot(candidate.get(), "-import-loaded");
        {
            BookmarkConfirmation confirmation;
            confirmation.title = ui::utf8("Ta bort importerad text");
            confirmation.Register();
            candidate->remove_content();
            if (!corpus.has_imported() || !std::filesystem::exists(imported))
                fail(__LINE__);
            confirmation.answer = wxID_YES;
            auto* remove =
                wxWindow::FindWindowByLabel(ui::utf8("Ta bort importerad text…"), candidate->current_page());
            wxCommandEvent click(wxEVT_BUTTON, remove->GetId());
            remove->GetEventHandler()->ProcessEvent(click);
            wxTheApp->ProcessPendingEvents();
            if (confirmation.questions != 2 || !confirmation.safe_default)
                fail(__LINE__);
        }
        check_import_controls(false);
        const auto& removed_coordinates = corpus.coordinates(srb_source, "John");
        if (!before_import.empty() || installed_coordinates != saved_coordinates ||
            !removed_coordinates.empty())
            fail(__LINE__);
        if (corpus.has_imported() || std::filesystem::exists(imported) ||
            !corpus.coordinates(srb_source, "John").empty() || corpus.verse(srb_source, "John", {1, 50}) ||
            profile.bookmarks(corpus).size() != bookmarks.size() || profile.completed() != completed ||
            std::ranges::any_of(profile.load().primary_sources, [](const auto& source) {
                return source.source == srb_source;
            }))
            fail(__LINE__);
        candidate->back();
        candidate->open_reading(reading);
        if (candidate->scripture_->chapter_source(1, 0) != "sv1917" || !candidate->play_->IsEnabled())
            fail(__LINE__);
        candidate->open_reading({ReadingKind::OldTestament, {"Gen", {1, 1}, {1, 2}}});
        if (candidate->scripture_->chapter_source(1, 0) != "en-lxx2012")
            fail(__LINE__);
        // Reinstalling clears the empty lookups cached after removal.
        std::filesystem::copy_file(backup, imported);
        corpus.load_imported(imported);
        const auto reimported = corpus.verse(srb_source, "John", {1, 50});
        if (!corpus.has_imported() || !reimported || reimported->text != "SRB test John 1:50" ||
            !before_import.empty() || !removed_coordinates.empty() ||
            installed_coordinates != saved_coordinates)
            fail(__LINE__);
        candidate->Hide();
    }
    // The text size steps like Chrome's zoom, reaches every page, and returns to its default.
    {
        const auto size = settings_.text_zoom;
        const auto command = [this](const char* menu, const wxString& label) {
            wxCommandEvent event(wxEVT_MENU, menu_bar_->FindMenuItem(menu, label));
            ProcessWindowEvent(event);
        };
        const int home_width = home_content_->GetMinSize().x;
        command("Visa", ui::utf8("Större text"));
        if (settings_.text_zoom != 110 || text_zoom != 110 || ui_font().GetFractionalPointSize() <= 11 ||
            home_content_->GetMinSize().x <= home_width)
            fail(__LINE__);
        settings_.text_zoom = 200;
        apply_settings(false);
        command("Visa", ui::utf8("Större text"));
        if (settings_.text_zoom != 200 ||
            menu_bar_->IsEnabled(menu_bar_->FindMenuItem("Visa", ui::utf8("Större text"))))
            fail(__LINE__);
        command("Visa", "Normal textstorlek");
        if (settings_.text_zoom != Settings{}.text_zoom || text_zoom != Settings{}.text_zoom)
            fail(__LINE__);
        settings_.text_zoom = size;
        apply_settings(false);
    }
    // Speed steps the same way, and stops at either end.
    {
        const auto rate = settings_.speech_rate;
        const auto command = [this](const wxString& label) {
            wxCommandEvent event(wxEVT_MENU, menu_bar_->FindMenuItem(ui::utf8("Uppläsning"), label));
            ProcessWindowEvent(event);
        };
        settings_.speech_rate = 250;
        command("Snabbare");
        command("Snabbare");
        if (settings_.speech_rate != 300)
            fail(__LINE__);
        command(ui::utf8("Normal hastighet"));
        command(ui::utf8("Långsammare"));
        if (settings_.speech_rate != 75)
            fail(__LINE__);
        settings_.speech_rate = rate;
        speech_->set_speed(rate / 100.0);
        apply_settings(false);
    }
    set_parallel("");
    apply_settings(false);
    for (const auto& reading :
         std::vector<Reading>{{ReadingKind::OldTestament, {"Gen", {31, 50, "a"}, {31, 50, "a"}}, {}, "el"},
                              {ReadingKind::OldTestament, {"1Esd", {1, 1}, {1, 1}}}}) {
        auto referenced = reading;
        if (reading.passage.book == "Gen")
            referenced.reference = "grc-lxx";
        open_reading(referenced);
        scripture_->center_passage();
        // Read-aloud also works for the English fallback in books without Swedish text.
        if (scripture_->cached_rows() == 0 || !play_->IsEnabled())
            fail(__LINE__);
        save(render(), "-" + ui::utf8(reading.passage.book));
    }
    open_reading({ReadingKind::Epistle, {"1Cor", {4, 9}, {4, 16}}});
    save(render(), "-prose");
    // The end of a book keeps blank space below its last line.
    scripture_->scroll_by(1e9);
    {
        const auto image = render();
        const auto paper = palette(settings_.theme).paper;
        for (int y = image.GetHeight() - FromDIP(100); y < image.GetHeight(); ++y)
            for (int x = 0; x < image.GetWidth(); ++x)
                if (image.GetRed(x, y) != paper.Red() || image.GetGreen(x, y) != paper.Green() ||
                    image.GetBlue(x, y) != paper.Blue())
                    fail(__LINE__);
        save(image, "-end");
    }
    // Marked verses replace the reading as what Lyssna reads.
    open_fresh_psalm();
    scripture_->select_verses({23, 4}, {23, 2});
    const auto marked = scripture_->selections();
    if (marked.size() != 1 || marked.front().first != VerseRef{23, 2} ||
        marked.front().last != VerseRef{23, 4} || play_->GetLabel() != ui::utf8("Läs markering"))
        fail(__LINE__);
    scripture_->select_verses({23, 6}, {23, 6}, true);
    auto multiple = scripture_->selections();
    if (multiple.size() != 2 || scripture_->verse_selected({23, 5}) || !scripture_->verse_selected({23, 6}))
        fail(__LINE__);
    save(render(), "-ranges");
    scripture_->select_verses({23, 3}, {23, 6}, true);
    multiple = scripture_->selections();
    if (multiple.size() != 1 || multiple.front().first != VerseRef{23, 2} ||
        multiple.front().last != VerseRef{23, 6})
        fail(__LINE__);
    scripture_->clear_selection();
    if (play_->GetLabel() != "Lyssna")
        fail(__LINE__);
    const auto click_text = [&](wxPoint point, int modifiers = wxMOD_NONE) {
        if (point == wxDefaultPosition) {
            fail(__LINE__);
            return;
        }
        for (const auto type : {wxEVT_LEFT_DOWN, wxEVT_LEFT_UP}) {
            wxMouseEvent event(type);
            event.SetPosition(point);
            event.SetShiftDown((modifiers & wxMOD_SHIFT) != 0);
            event.SetAltDown((modifiers & wxMOD_ALT) != 0);
            event.SetControlDown((modifiers & wxMOD_CONTROL) != 0);
            event.SetEventObject(scripture_);
            scripture_->GetEventHandler()->ProcessEvent(event);
        }
    };
    const auto verse_point = [&](VerseRef ref, int column, bool stacked = false) {
        const auto size = scripture_->GetClientSize();
        const int first = stacked || column == 0 ? 0 : size.x / 2;
        const int last = stacked ? size.x : column == 0 ? size.x / 2 : size.x;
        // Stacked counterparts occur below the left text; find the last match there.
        wxPoint result = wxDefaultPosition;
        for (int y = 0; y < size.y; y += 4)
            for (int x = first; x < last; x += 4)
                if (scripture_->verse_at({x, y}) == ref) {
                    if (!stacked || column == 0)
                        return wxPoint{x, y};
                    result = {x, y};
                    break;
                }
        return result;
    };
    // Shift extends from the original click even when the endpoint moves backwards.
    const auto selection_test_size = GetSize();
    SetSize(FromDIP(wxSize(800, 600)));
    Layout();
    root_->Layout();
    const auto click_verse = [&](VerseRef ref, int modifiers = wxMOD_NONE) {
        scripture_->show_verse(ref);
        render();
        click_text(verse_point(ref, 0), modifiers);
    };
    open_fresh_psalm();
    click_verse({22, 3});
    click_verse({22, 5}, wxMOD_SHIFT);
    auto shifted = scripture_->selections();
    if (shifted.size() != 1 || shifted[0].first != VerseRef{22, 3} || shifted[0].last != VerseRef{22, 5})
        fail(__LINE__);
    click_verse({22, 2}, wxMOD_SHIFT);
    shifted = scripture_->selections();
    if (shifted.size() != 1 || shifted[0].first != VerseRef{22, 2} || shifted[0].last != VerseRef{22, 3})
        fail(__LINE__);
    click_verse({22, 6}, wxMOD_ALT);
    shifted = scripture_->selections();
    if (shifted.size() != 2 || scripture_->verse_selected({22, 4}) || !scripture_->verse_selected({22, 6}))
        fail(__LINE__);
    const auto copied = scripture_->selected_text();
    if (!copied.ends_with("Psaltaren 23:2–3; 23:6 (Svenska 1917 med apokryfer)") ||
        copied.find(corpus_.verse("sv1917", "Ps", {23, 4})->text) != std::string::npos)
        fail(__LINE__);
    wxCommandEvent copy_event(wxEVT_MENU, wxID_COPY);
    scripture_->GetEventHandler()->ProcessEvent(copy_event);
    if (wxTheClipboard->Open()) {
        wxTextDataObject clipboard;
        if (!wxTheClipboard->GetData(clipboard) || clipboard.GetText() != ui::utf8(copied))
            fail(__LINE__);
        wxTheClipboard->Close();
    } else
        fail(__LINE__);
    // An added drag that retreats must discard the old endpoint, keeping earlier ranges.
    scripture_->select_verses({22, 2}, {22, 2});
    scripture_->show_verse({22, 4});
    render();
    for (const auto& [type, ref] : std::vector<std::pair<wxEventType, VerseRef>>{{wxEVT_LEFT_DOWN, {22, 4}},
                                                                                 {wxEVT_MOTION, {22, 6}},
                                                                                 {wxEVT_MOTION, {22, 5}},
                                                                                 {wxEVT_LEFT_UP, {22, 5}}}) {
        wxMouseEvent event(type);
        event.SetPosition(verse_point(ref, 0));
        event.SetAltDown(true);
        event.SetLeftDown(type != wxEVT_LEFT_UP);
        scripture_->GetEventHandler()->ProcessEvent(event);
    }
    shifted = scripture_->selections();
    if (shifted.size() != 2 || shifted[0].first != VerseRef{22, 2} || shifted[1].first != VerseRef{22, 4} ||
        shifted[1].last != VerseRef{22, 5} || scripture_->verse_selected({22, 6}))
        fail(__LINE__);
    wxKeyEvent escape(wxEVT_CHAR_HOOK);
    escape.m_keyCode = WXK_ESCAPE;
    scripture_->GetEventHandler()->ProcessEvent(escape);
    if (!scripture_->selections().empty() || !scripture_->selected_text().empty())
        fail(__LINE__);
    // Opening another passage discards the old Shift-click anchor.
    open_fresh_psalm();
    click_verse({22, 2}, wxMOD_SHIFT);
    if (!scripture_->selections().empty())
        fail(__LINE__);
    scripture_->clear_selection();
    SetSize(selection_test_size);
    Layout();
    root_->Layout();
    const auto verse_tools = [&]() -> wxWindow* {
        for (auto* child : scripture_->GetChildren())
            if (child->GetName() == ui::utf8("Läs den markerade versen online"))
                return child;
        return nullptr;
    };
    const auto source_buttons = [](wxWindow* parent) {
        std::vector<TextButton*> buttons;
        if (parent)
            for (auto* child : parent->GetChildren())
                if (auto* button = dynamic_cast<TextButton*>(child);
                    button && button->GetLabel() != "Lyssna" && button->GetLabel() != "Kopiera")
                    buttons.push_back(button);
        return buttons;
    };
    // Both language columns open and close the same verse, in either theme.
    // A small desktop can start with stacked columns; these checks need two columns beside each other.
    SetSize(FromDIP(wxSize(1120, 900)));
    Layout();
    root_->Layout();
    for (const auto theme : {Theme::Light, Theme::Dark})
        for (const auto mode : {"el", "en"}) {
            settings_.theme = theme;
            set_parallel(mode);
            apply_settings(false);
            open_fresh_psalm();
            const auto plain = render();
            const auto left = verse_point({22, 1}, 0), right = verse_point({22, 1}, 1);
            click_text(left);
            const auto highlighted = scripture_->highlighted_verse();
            if (!highlighted || highlighted->first != VerseRef{22, 1} || !scripture_->selections().empty() ||
                play_->GetLabel() != "Lyssna")
                fail(__LINE__);
            auto* tools = verse_tools();
            if (tools && tools->IsShown())
                fail(__LINE__); // Bible source buttons are exclusive to single-pane Swedish.
            click_text({scripture_->GetClientSize().x / 2, left.y});
            if (!scripture_->highlighted_verse())
                fail(__LINE__); // The gutter does not toggle a verse.
            const auto marked = render();
            std::size_t left_pixels = 0, right_pixels = 0;
            for (int y = 0; y < plain.GetHeight(); ++y)
                for (int x = 0; x < plain.GetWidth(); ++x)
                    if (plain.GetRed(x, y) != marked.GetRed(x, y) ||
                        plain.GetGreen(x, y) != marked.GetGreen(x, y) ||
                        plain.GetBlue(x, y) != marked.GetBlue(x, y))
                        ++(x < plain.GetWidth() / 2 ? left_pixels : right_pixels);
            if (left_pixels < 100 || right_pixels < 100)
                fail(__LINE__);
            save(marked, "-verse-" + wxString(mode) + (theme == Theme::Dark ? "-dark" : ""));
            if (!screenshot_path.empty() && std::string(mode) == "el") {
                Layout();
                Update();
                wxBitmap bitmap(GetClientSize());
                wxClientDC window(this);
                {
                    wxMemoryDC image(bitmap);
                    image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
                }
                save(bitmap.ConvertToImage(),
                     theme == Theme::Dark ? "-verse-actions-dark" : "-verse-actions");
            }
            click_text(right);
            if (scripture_->highlighted_verse() || (tools && tools->IsShown()))
                fail(__LINE__);
            click_text(right);
            if (!scripture_->highlighted_verse() || (tools && tools->IsShown()))
                fail(__LINE__);
            const auto source = scripture_->chapter_source(22, 1);
            const auto targets = corpus_.counterparts(scripture_->frame(), source, "Ps", {22, 1});
            if (targets.empty() ||
                !scripture_->selected_text().starts_with(
                    corpus_.verse(source, targets.front().first, targets.front().second)->text))
                fail(__LINE__); // Copy follows the clicked pane's edition.
            click_text(verse_point({22, 1}, 0));
            if (scripture_->highlighted_verse())
                fail(__LINE__);
        }
    // A retained highlight uses the visible edition after its original pane closes.
    set_parallel("el");
    apply_settings(false);
    open_fresh_psalm();
    render();
    click_text(verse_point({22, 1}, 1));
    set_parallel("");
    apply_settings(false);
    if (!scripture_->highlighted_verse() ||
        !scripture_->selected_text().starts_with(corpus_.verse("sv1917", "Ps", {23, 1})->text) ||
        !scripture_->copy_selection())
        fail(__LINE__);
    // Single-pane Swedish retains mapped source links and follows the visible verse.
    for (const auto theme : {Theme::Light, Theme::Dark}) {
        set_parallel("");
        settings_.theme = theme;
        apply_settings(false);
        open_fresh_psalm();
        const auto point = verse_point({22, 1}, 0);
        click_text(point);
        auto* tools = verse_tools();
        const auto buttons = source_buttons(tools);
        if (!tools || !tools->IsShown() || buttons.size() != 4 ||
            std::any_of(buttons.begin(), buttons.end(), [](auto* button) {
                return !button->IsEnabled() || !button->GetToolTipText().Contains("23:1");
            }))
            fail(__LINE__);
        for (std::size_t i = 0; tools && i < buttons.size(); ++i)
            if (!tools->GetClientRect().Contains(buttons[i]->GetRect()) ||
                (i && buttons[i - 1]->GetRect().Intersects(buttons[i]->GetRect())))
                fail(__LINE__);
        if (tools && tools->GetRect().GetBottom() >= point.y)
            fail(__LINE__);
        const auto tools_y = tools ? tools->GetPosition().y : 0;
        scripture_->scroll_by(20);
        if (tools && tools->IsShown() && tools->GetPosition().y != tools_y - 20)
            fail(__LINE__);
        scripture_->scroll_by(std::max(0, point.y - 20 + 8));
        if (!tools || !tools->IsShown())
            fail(__LINE__);
        scripture_->scroll_by(1e9);
        if (tools && tools->IsShown())
            fail(__LINE__);
        scripture_->center_passage();
        if (!tools || !tools->IsShown())
            fail(__LINE__);
        // Changing panes hides and restores links without losing the verse highlight.
        set_parallel("el");
        apply_settings(false);
        if (!scripture_->highlighted_verse() || (tools && tools->IsShown()))
            fail(__LINE__);
        set_parallel("");
        apply_settings(false);
        if (!tools || !tools->IsShown() || !scripture_->highlighted_verse())
            fail(__LINE__);
        if (!screenshot_path.empty()) {
            Layout();
            Update();
            wxBitmap bitmap(GetClientSize());
            wxClientDC window(this);
            {
                wxMemoryDC image(bitmap);
                image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
            }
            save(bitmap.ConvertToImage(),
                 theme == Theme::Dark ? "-swedish-source-links-dark" : "-swedish-source-links");
        }
        click_text(verse_point({22, 1}, 0));
        if (scripture_->highlighted_verse() || (tools && tools->IsShown()))
            fail(__LINE__);
    }
    // Selection actions save complete ranges, reopen them, and require confirmation for removal.
    {
        const auto data = data_ / "bookmarks";
        UserDb profile(data / "user.db");
        auto candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data,
                                                     std::make_unique<SmokeUpdates>(), true);
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        candidate->speech_ = std::move(held);
        candidate->Show();
        candidate->open_reading({ReadingKind::Gospel, {"John", {1, 1}, {2, 3}}});
        candidate->scripture_->select_verses({1, 2}, {1, 5});
        const auto marked = candidate->scripture_->marked_reading();
        if (!marked || marked->passage.first != VerseRef{1, 2} || marked->passage.last != VerseRef{1, 5})
            fail(__LINE__);
        wxWindow* tools = nullptr;
        for (auto* child : candidate->scripture_->GetChildren())
            if (child->GetName() == ui::utf8("Läs den markerade versen online"))
                tools = child;
        wxWindow* bookmark = nullptr;
        TextButton* listen = nullptr;
        if (tools)
            for (auto* child : tools->GetChildren()) {
                if (child->GetName() == ui::utf8("Lägg till bokmärke"))
                    bookmark = child;
                if (auto* button = dynamic_cast<TextButton*>(child); button && button->GetLabel() == "Lyssna")
                    listen = button;
            }
        if (!tools || !tools->IsShown() || !bookmark || !listen)
            fail(__LINE__);
        if (!screenshot_path.empty()) {
            candidate->Layout();
            candidate->Update();
            wxBitmap bitmap(candidate->GetClientSize());
            wxClientDC window(candidate.get());
            {
                wxMemoryDC image(bitmap);
                image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
            }
            save(bitmap.ConvertToImage(), "-selection-actions");
        }
        if (bookmark) {
            for (const auto type : {wxEVT_KEY_DOWN, wxEVT_KEY_UP}) {
                wxKeyEvent event(type);
                event.m_keyCode = WXK_RETURN;
                bookmark->GetEventHandler()->ProcessEvent(event);
            }
            wxTheApp->ProcessPendingEvents();
        }
        const auto marks = profile.bookmarks(corpus_);
        if (marks.size() != 1 || marks.front().passage.last != VerseRef{1, 5})
            fail(__LINE__);
        if (listen) {
            wxCommandEvent event(wxEVT_BUTTON, listen->GetId());
            listen->GetEventHandler()->ProcessEvent(event);
            wxTheApp->ProcessPendingEvents();
            if (engine->spoken.empty() || !engine->spoken.front().cue ||
                engine->spoken.front().cue->verse != VerseRef{1, 2} || !engine->spoken.back().cue ||
                engine->spoken.back().cue->verse > VerseRef{1, 5} || candidate->speech_tracked_)
                fail(__LINE__);
        }
        candidate->stop_speech(false);
        if (marked)
            candidate->add_bookmarks(*marked);
        if (profile.bookmarks(corpus_).size() != 1)
            fail(__LINE__);
        if (!marks.empty()) {
            candidate->open_reading({ReadingKind::OldTestament, {"Gen", {1, 1}, {1, 5}}});
            candidate->open_bookmark(marks.front());
            const auto selected = candidate->scripture_->selections();
            if (!candidate->visible_reading_ || candidate->visible_reading_->passage.book != "John" ||
                selected.size() != 1 || selected.front().first != VerseRef{1, 2} ||
                selected.front().last != VerseRef{1, 5})
                fail(__LINE__);
            const int index = candidate->menu_bar_->FindMenu(ui::utf8("Bokmärken"));
            const auto* menu =
                index == wxNOT_FOUND ? nullptr : candidate->menu_bar_->GetMenu(std::size_t(index));
            if (!menu ||
                menu->FindItem(ui::utf8(passage_label(corpus_, {marks.front().passage}))) == wxNOT_FOUND ||
                !menu->GetMenuItems().back()->GetSubMenu())
                fail(__LINE__);
            BookmarkConfirmation confirmation;
            confirmation.Register();
            candidate->remove_bookmark(marks.front());
            if (profile.bookmarks(corpus_).size() != 1 || confirmation.questions != 1)
                fail(__LINE__);
            confirmation.answer = wxID_YES;
            candidate->remove_bookmark(marks.front());
            if (!profile.bookmarks(corpus_).empty() || confirmation.questions != 2 ||
                !confirmation.safe_default)
                fail(__LINE__);
        }
        candidate->scripture_->select_verses({1, 1}, {2, 3});
        candidate->scripture_->show_verse({2, 2});
        candidate->Update();
        if (!tools || !tools->IsShown())
            fail(__LINE__);
        candidate->settings_.primary_sources =
            decode_sources("en-web=1", candidate->settings_.primary_sources);
        candidate->apply_settings(false);
        candidate->open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}});
        candidate->scripture_->select_verses({1, 2}, {1, 5});
        const auto english = candidate->scripture_->marked_reading();
        if (!english || english->base_language != "en")
            fail(__LINE__);
        bool has_bookmark = false, has_listen = false;
        for (auto* child : tools->GetChildren()) {
            has_bookmark |= child->GetName() == ui::utf8("Lägg till bokmärke");
            if (auto* button = dynamic_cast<TextButton*>(child); button && button->GetLabel() == "Lyssna")
                has_listen = true;
        }
        if (!tools->IsShown() || !has_bookmark || !has_listen)
            fail(__LINE__);
        candidate->Hide();
        open_fresh_psalm();
    }
    for (const auto& source :
         {corpus_.source_for_language("el", "John"), corpus_.source_for_language("en", "John")}) {
        Reading reading{ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}};
        reading.source_override = source;
        scripture_->open(reading);
        click_text(verse_point({1, 1}, 0));
        if (!scripture_->highlighted_verse() || !verse_tools() || !verse_tools()->IsShown() ||
            !source_buttons(verse_tools()).empty())
            fail(__LINE__);
    }
    // A Swedish merged verse highlights both Greek counterparts and toggles from either.
    set_parallel("el");
    settings_.theme = Theme::Light;
    apply_settings(false);
    open_reading({ReadingKind::OldTestament, {"1Chr", {12, 4}, {12, 5}}});
    click_text(verse_point({12, 4}, 0));
    if (!scripture_->highlighted_verse() || scripture_->highlighted_verse()->last != VerseRef{12, 5})
        fail(__LINE__);
    click_text(verse_point({12, 5}, 1));
    if (scripture_->highlighted_verse())
        fail(__LINE__);
    // Narrow columns stack; the lower language still toggles the upper verse.
    SetSize(FromDIP(wxSize(520, 850)));
    Layout();
    root_->Layout();
    open_fresh_psalm();
    click_text(verse_point({22, 1}, 1, true));
    if (!scripture_->highlighted_verse())
        fail(__LINE__);
    save(render(), "-verse-stacked");
    click_text(verse_point({22, 1}, 0, true));
    if (scripture_->highlighted_verse())
        fail(__LINE__);
    SetSize(original_size);
    Layout();
    root_->Layout();
    set_parallel("");
    apply_settings(false);
    open_reading({ReadingKind::OldTestament, {"Tob", {1, 1}, {1, 1}}});
    click_text(verse_point({1, 1}, 0));
    const auto apocrypha_buttons = source_buttons(verse_tools());
    if (apocrypha_buttons.size() != 4 ||
        std::count_if(apocrypha_buttons.begin(), apocrypha_buttons.end(), [](auto* button) {
            return button->IsEnabled();
        }) != 1)
        fail(__LINE__);
    // Ordstudium replaces the right pane; a Swedish word shows Dalin and Nyström.
    settings_.word_study = true;
    set_parallel("");
    apply_settings(false);
    open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}});
    if (!study_->IsShown() || scripture_->base_source() != "sv1917")
        fail(__LINE__);
    if (scripture_->highlighted_verse() || (verse_tools() && verse_tools()->IsShown()))
        fail(__LINE__);
    const auto word_point = [&] {
        const auto size = scripture_->GetClientSize();
        for (int y = 0; y < size.y; y += 4)
            for (int x = 0; x < size.x; x += 4)
                if (const auto word = scripture_->word_at({x, y}); word && word->text == "begynnelsen")
                    return wxPoint{x, y};
        return wxDefaultPosition;
    }();
    click_text(word_point);
    if (!scripture_->highlighted_word() || scripture_->highlighted_verse() ||
        scripture_->highlighted_word()->text != "begynnelsen")
        fail(__LINE__);
    save(render(), "-word-highlight");
    click_text(word_point);
    if (scripture_->highlighted_word() || study_->text().empty() ||
        !study_->text().front().Contains(ui::utf8("Klicka på ett ord")))
        fail(__LINE__);
    click_text(word_point);
    if (!screenshot_path.empty()) {
        Layout();
        Update();
        wxBitmap bitmap(GetClientSize());
        wxClientDC window(this);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), "-word-study");
    }
    {
        NavigationSpeech deferred;
        auto panel = std::make_unique<StudyPanel>(root_, corpus_, deferred, [this](const auto& language) {
            return speech_lexicon(language);
        });
        panel->Hide();
        const auto has = [&](const std::string& value) {
            const auto lines = panel->text();
            return std::any_of(lines.begin(), lines.end(), [&](const auto& line) {
                return line.Contains(ui::utf8(value));
            });
        };
        panel->show({"John", "begynnelsen", {1, 1}, 0}, "sv1917");
        const auto source_buttons = [&] {
            std::vector<TextButton*> buttons;
            for (auto* child : panel->GetChildren()) {
                auto* button = dynamic_cast<TextButton*>(child);
                if (button && (button->GetLabel() == "SFB15" || button->GetLabel() == "SKB" ||
                               button->GetLabel() == "SRB" || button->GetLabel() == "B2026"))
                    buttons.push_back(button);
            }
            return buttons;
        };
        if (!source_buttons().empty() || has("LÄS VERSEN ONLINE"))
            fail(__LINE__);
        panel->show({"Tob", "Tobit", {1, 1}, 0}, "sv1917");
        if (!source_buttons().empty())
            fail(__LINE__);
        panel->clear();
        if (!source_buttons().empty())
            fail(__LINE__);
        panel->show({"John", "Jesu", {1, 17}, 0}, "sv1917");
        if (deferred.lookups.size() != 3)
            fail(__LINE__);
        else {
            deferred.lookups[0]("stale-pronunciation");
            wxTheApp->ProcessPendingEvents();
            if (has("stale-pronunciation"))
                fail(__LINE__);
            deferred.lookups[2]("current-pronunciation");
            wxTheApp->ProcessPendingEvents();
            panel->apply(Theme::Dark);
            if (!has("current-pronunciation") || deferred.lookups.size() != 3)
                fail(__LINE__);
            panel->clear();
            deferred.lookups[2]("cleared-pronunciation");
            wxTheApp->ProcessPendingEvents();
            if (has("cleared-pronunciation"))
                fail(__LINE__);
            panel.reset();
            deferred.lookups[2]("destroyed-pronunciation");
            wxTheApp->ProcessPendingEvents();
        }
    }
    {
        const auto lines = study_->text();
        const auto has = [&](const wxString& part) {
            return std::any_of(lines.begin(), lines.end(), [&](const wxString& line) {
                return line.Contains(part);
            });
        };
        if (!has("DALIN") || !has("SALDO") || !has("begynnelse") || has("STRONG"))
            fail(__LINE__);
    }
    study_->show({"John", "främst", {1, 17}, 0}, scripture_->base_source());
    {
        const auto lines = study_->text();
        const auto has = [&](const wxString& part) {
            return std::any_of(lines.begin(), lines.end(), [&](const wxString& line) {
                return line.Contains(part);
            });
        };
        if (!has("SALDO") || !has("adjektiv") || !has("adverb"))
            fail(__LINE__);
        bool source = false, license = false;
        for (auto* child : study_->GetChildren()) {
            if (auto* link = wxDynamicCast(child, wxHyperlinkCtrl)) {
                source |= link->GetURL() == "https://www.sprakbanken.se/resurser/saldo";
                license |= link->GetURL() == "https://creativecommons.org/licenses/by/4.0/";
            }
        }
        if (!source || !license)
            fail(__LINE__);
    }
    study_->show({"John", "orimligtpåhittat", {1, 17}, 0}, scripture_->base_source());
    for (const auto& line : study_->text())
        if (line.Contains("SALDO"))
            fail(__LINE__);
    study_->show({"John", "Jesu", {1, 17}, 0}, scripture_->base_source());
    {
        const auto lines = study_->text();
        const auto has = [&](const wxString& part) {
            return std::any_of(lines.begin(), lines.end(), [&](const wxString& line) {
                return line.Contains(part);
            });
        };
        if (!has(wxString::FromUTF8("NYSTRÖM 1896")) || !has("Jesus Kristus") || !has("Jesus Justus") ||
            has(wxString::FromUTF8("Ordet finns inte")))
            fail(__LINE__);
        // Long articles begin with a bounded preview; the complete text stays in the database.
        if (std::any_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.length() > 520;
            }))
            fail(__LINE__);
    }
    bool expanded = false;
    for (auto* child : study_->GetChildren()) {
        auto* more = dynamic_cast<TextButton*>(child);
        if (!more || more->GetLabel() != wxString::FromUTF8("Visa hela artikeln"))
            continue;
        wxCommandEvent click(wxEVT_BUTTON, more->GetId());
        click.SetEventObject(more);
        more->GetEventHandler()->ProcessEvent(click);
        expanded = true;
        break;
    }
    wxTheApp->ProcessPendingEvents();
    {
        const auto lines = study_->text();
        if (!expanded || std::none_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.length() > 520;
            }))
            fail(__LINE__);
    }
    study_->show({"John", "Kristi", {1, 17}, 0}, scripture_->base_source());
    {
        const auto lines = study_->text();
        if (std::none_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.Contains("Messias");
            }))
            fail(__LINE__);
    }
    // A word's Lyssna would stop the reading in progress, so it waits until the reading ends.
    {
        const auto word_listen = [&]() -> TextButton* {
            for (auto* child : study_->GetChildren())
                if (auto* button = dynamic_cast<TextButton*>(child); button && button->GetLabel() == "Lyssna")
                    return button;
            return nullptr;
        };
        if (!word_listen() || !word_listen()->IsEnabled())
            fail(__LINE__);
        display_playback({SpeechState::Playing, {}, 0, 0});
        if (!word_listen() || word_listen()->IsEnabled())
            fail(__LINE__);
        study_->show({"John", "Kristi", {1, 17}, 0}, scripture_->base_source());
        if (!word_listen() || word_listen()->IsEnabled())
            fail(__LINE__);
        display_playback({SpeechState::Stopped, {}, 0, 0});
        if (!word_listen() || !word_listen()->IsEnabled())
            fail(__LINE__);
    }
    // Embedded pages must preserve the live reader, marked verses, and word-study article.
    scripture_->select_verses({1, 1}, {1, 2});
    scripture_->scroll_by(120);
    const auto saved_scroll = scripture_->scroll_position();
    const auto study_content = [&] {
        auto lines = study_->text();
        // IPA may arrive while another page is open; the article and selected word stay put.
        std::erase_if(lines, [](const auto& line) {
            return line.StartsWith("/") && line.EndsWith("/");
        });
        return lines;
    };
    const auto saved_study = study_content();
    const auto saved_selection = scripture_->selections();
    const auto saved_reading = visible_reading_;
    tracked_ = Tracked{"smoke:preserved", "Urval"};
    for (int mode = 0; mode < 2; ++mode) {
        if (mode == 0)
            about();
        else
            browse_bible();
        if (!current_page() || scripture_->IsShown() || study_->IsShown() || !back_->IsEnabled() ||
            play_->IsEnabled() || stop_->IsEnabled() || !address_->IsEnabled() ||
            current_page()->GetClientSize().y < root_->GetClientSize().y - bar_->GetSize().y - FromDIP(4))
            fail(__LINE__);
        // Every command stays available from auxiliary pages; those for the day leave the page.
        const auto enabled = [this](const char* menu, const char* label) {
            return menu_bar_->IsEnabled(menu_bar_->FindMenuItem(ui::utf8(menu), ui::utf8(label)));
        };
        if (!enabled("Gå", "Idag") || !enabled("Gå", "Välj datum…") || !enabled("Gå", "Läsplaner") ||
            !enabled("Visa", "Större text") || !menu_bar_->IsEnabled(wxID_PREFERENCES) ||
            !menu_bar_->IsEnabled(wxID_ABOUT))
            fail(__LINE__);
        if (mode == 1) {
            auto* browser = dynamic_cast<BiblePicker*>(current_page());
            browser->select_book(corpus_.canon().front());
            browser->select_chapter(corpus_.canon().front(), 1);
            address_clicked();
        } else
            back();
        for (size_t i = 0; i < menu_bar_->GetMenuCount(); ++i)
            if (!menu_bar_->IsEnabledTop(i))
                fail(__LINE__);
        const auto restored = scripture_->selections();
        if (current_page() || !scripture_->IsShown() || !study_->IsShown() ||
            scripture_->scroll_position() != saved_scroll || study_content() != saved_study ||
            restored.size() != saved_selection.size() ||
            restored.front().first != saved_selection.front().first || !tracked_ ||
            tracked_->key != "smoke:preserved" ||
            visible_reading_->passage.book != saved_reading->passage.book ||
            visible_reading_->passage.first != saved_reading->passage.first)
            fail(__LINE__);
    }
    about();
    auto* saved_about = current_page();
    address_clicked();
    if (!dynamic_cast<BiblePicker*>(current_page()) || saved_about->IsShown() ||
        navigation_.history_size() != 1)
        fail(__LINE__);
    address_clicked();
    if (current_page() != saved_about || !current_page()->IsShown() || !address_->IsEnabled())
        fail(__LINE__);
    back();
    tracked_.reset();
    scripture_->clear_selection();
    // The normal browser prepares one range and opens it only after confirmation.
    browse_bible();
    auto* picker = dynamic_cast<BiblePicker*>(current_page());
    const auto& canon = corpus_.canon();
    const auto john = *std::find_if(canon.begin(), canon.end(), [](const auto& book) {
        return book.code == "John";
    });
    const auto psalms = *std::find_if(canon.begin(), canon.end(), [](const auto& book) {
        return book.code == "Ps";
    });
    if (picker->stacked() || !picker->parts().empty() || address_->GetLabel() != "Bibel" ||
        !address_->IsEnabled())
        fail(__LINE__);
    picker->open_selection();
    if (current_page() != picker)
        fail(__LINE__);
    picker->select_book(john);
    const auto john_verses = picker->verses_of(john);
    if (picker->first_ != john_verses.front() || picker->last_ != john_verses.back() ||
        picker->parts().size() != 1 || current_page() != picker ||
        address_->GetLabel() !=
            ui::utf8(passage_label(corpus_, {{"John", john_verses.front(), john_verses.back()}})))
        fail(__LINE__);
    picker->select_chapter(john, 1);
    const auto drawer = picker->book_drawer(john);
    const auto full_verse = corpus_.verse("sv1917", "John", {1, 1});
    if (!full_verse || drawer.back().cells.front().tip != ui::utf8(full_verse->text) + ellipsis ||
        drawer.back().drawer.back().cells.front().tip != ui::utf8(full_verse->text))
        fail(__LINE__);
    // John's chapters and verses unfold beneath its row, above the books that follow it.
    const auto chapters = picker->cells_->unfolded(1), verses = picker->cells_->unfolded(2);
    if (chapters.IsEmpty() || verses.IsEmpty() || !chapters.Contains(verses) ||
        chapters.GetBottom() + FromDIP(30) > picker->cells_->GetSize().y)
        fail(__LINE__);
    picker->select_verse(john, {1, 3});
    if (current_page() != picker || picker->first_ != VerseRef{1, 3} || picker->last_ != VerseRef{1, 3})
        fail(__LINE__);
    picker->open_selection();
    if (current_page() || scripture_->reading().passage.first != VerseRef{1, 3} ||
        scripture_->reading().passage.last != VerseRef{1, 3} || scripture_->reading().segments().size() != 1)
        fail(__LINE__);
    browse_bible();
    picker = dynamic_cast<BiblePicker*>(current_page());
    const auto chapter_end = [&](int chapter) {
        return *std::find_if(john_verses.rbegin(), john_verses.rend(), [chapter](VerseRef ref) {
            return ref.chapter == chapter;
        });
    };
    picker->select_book(john);
    picker->select_chapter(john, 3);
    if (picker->first_ != VerseRef{3, 1} || picker->last_ != chapter_end(3))
        fail(__LINE__);
    picker->select_chapter(john, 5);
    if (picker->first_ != VerseRef{3, 1} || picker->last_ != chapter_end(5))
        fail(__LINE__);
    picker->select_book(john);
    picker->select_chapter(john, 5);
    picker->select_chapter(john, 3);
    if (picker->first_ != VerseRef{3, 1} || picker->last_ != chapter_end(5))
        fail(__LINE__);
    picker->select_book(john);
    picker->select_chapter(john, 3);
    picker->select_verse(john, {3, 3});
    picker->select_verse(john, {3, 5});
    if (picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{3, 5})
        fail(__LINE__);
    picker->back(); // Escape steps up inside the picker; history Back leaves the page.
    update_bar();
    if (current_page() != picker || picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{3, 5})
        fail(__LINE__);
    picker->select_chapter(john, 5);
    if (picker->first_ != VerseRef{3, 3} || picker->last_ != chapter_end(5))
        fail(__LINE__);
    picker->select_verse(john, {5, 5});
    const auto selection_title = ui::utf8(passage_label(corpus_, {{"John", {3, 3}, {5, 5}}}));
    if (current_page() != picker || picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{5, 5} ||
        address_->GetLabel() != selection_title ||
        !address_->GetToolTipText().Contains(ui::utf8("gå tillbaka")))
        fail(__LINE__);
    update_bar();
    if (address_->GetLabel() != selection_title)
        fail(__LINE__);
    about();
    if (address_->GetLabel() != "Om Perikop")
        fail(__LINE__);
    back();
    if (current_page() != picker || address_->GetLabel() != selection_title)
        fail(__LINE__);
    picker->select_verse(john, {3, 4});
    if (picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{3, 4})
        fail(__LINE__);
    picker->select_verse(john, {2, 5});
    if (picker->first_ != VerseRef{2, 5} || picker->last_ != VerseRef{3, 3})
        fail(__LINE__);
    picker->select_book(psalms);
    const auto psalm_verses = picker->verses_of(psalms);
    if (picker->range_book_->code != psalms.code || picker->first_ != psalm_verses.front() ||
        picker->last_ != psalm_verses.back())
        fail(__LINE__);
    picker->select_book(john);
    picker->select_chapter(john, 3);
    picker->open_selection();
    if (current_page() || scripture_->reading().passage.first != VerseRef{3, 1} ||
        scripture_->reading().passage.last != chapter_end(3) || scripture_->reading().segments().size() != 1)
        fail(__LINE__);
    // Lägg till stacks selections from several books as the parts of one reading.
    browse_bible();
    picker = dynamic_cast<BiblePicker*>(current_page());
    const auto picker_size = GetSize();
    // In a small window, the unfolded Psalms scroll and nothing is wider than the page.
    picker->show_chapters(psalms);
    SetSize(FromDIP(wxSize(520, 650)));
    Layout();
    root_->Layout();
    picker->update_grid();
    const auto content_rect = picker->contents_->GetRect();
    if (content_rect.GetRight() > picker->grid_->GetClientSize().x ||
        content_rect.GetBottom() > picker->grid_->GetVirtualSize().y ||
        picker->grid_->GetScrollRange(wxVERTICAL) <= picker->grid_->GetScrollThumb(wxVERTICAL))
        fail(__LINE__);
    for (auto* child : picker->contents_->GetChildren())
        if (child->GetRect().GetRight() > picker->contents_->GetClientSize().x)
            fail(__LINE__);
    SetSize(picker_size);
    Layout();
    root_->Layout();
    picker->select_book(john);
    picker->select_chapter(john, 1);
    picker->select_verse(john, {1, 50});
    picker->select_chapter(john, 2);
    picker->select_verse(john, {2, 2});
    if (!picker->first_ || !picker->last_ || *picker->first_ != VerseRef{1, 50} ||
        *picker->last_ != VerseRef{2, 2})
        fail(__LINE__);
    picker->add_selection();
    const auto first_part =
        ui::utf8("Del 1: ") + ui::utf8(passage_label(corpus_, {{"John", {1, 50}, {2, 2}}}));
    if (!picker->stacked() || picker->first_ || address_->GetLabel() != first_part || !address_->IsEnabled())
        fail(__LINE__);
    picker->select_book(psalms);
    picker->select_chapter(psalms, 22);
    picker->select_verse(psalms, {22, 6});
    if (picker->parts().size() != 2 || !address_->GetLabel().StartsWith(ui::utf8("Del 2: ")))
        fail(__LINE__);
    // Rensa empties the stack and keeps the current selection.
    {
        const auto stack = picker->stack_;
        picker->clear_stack();
        if (picker->stacked() || picker->parts().size() != 1)
            fail(__LINE__);
        picker->stack_ = stack;
        picker->update_selection();
    }
    // About is reachable through its menu action without losing the browser's in-progress reading.
    const auto browser_scroll = picker->grid_->GetViewStart();
    const auto browser_first = picker->first_, browser_last = picker->last_;
    wxCommandEvent show_about(wxEVT_MENU, wxID_ABOUT);
    GetEventHandler()->ProcessEvent(show_about);
    if (current_page() == picker || picker->IsShown() || navigation_.history_size() != 1)
        fail(__LINE__);
    const auto* about_page = current_page();
    GetEventHandler()->ProcessEvent(show_about);
    if (current_page() != about_page || navigation_.history_size() != 1)
        fail(__LINE__);
    back();
    if (current_page() != picker || !picker->IsShown() || picker->chapter_ != 22 ||
        picker->first_ != browser_first || picker->last_ != browser_last ||
        picker->grid_->GetViewStart() != browser_scroll || picker->parts().size() != 2 ||
        navigation_.history_size() != 0)
        fail(__LINE__);
    picker->open_selection();
    const auto custom = scripture_->reading().segments();
    if (current_page() || custom.size() != 2 || custom[0].book != "John" ||
        custom[0].first != VerseRef{1, 50} || custom[0].last != VerseRef{2, 2} ||
        custom[1].first != VerseRef{22, 6} || parts_.size() != 2)
        fail(__LINE__);
    select_part(1);
    const auto custom_scroll = scripture_->scroll_position();
    about();
    back();
    if (part_ != 1 || scripture_->scroll_position() != custom_scroll || parts_.size() != 2)
        fail(__LINE__);
    // Day navigation controls rebuild safely; the calendar marks today instead of adding a button.
    show_readings();
    // Both church variants open independently, while the shared Gospel has one button.
    const auto previous_tradition = settings_.tradition;
    const auto previous_calendar = settings_.calendar;
    settings_.tradition = Tradition::Greek;
    settings_.calendar = CalendarStyle::New;
    select_day(parse_date("2026-05-10").value());
    const auto click_row = [](ReadingRow* row) {
        wxMouseEvent down(wxEVT_LEFT_DOWN), up(wxEVT_LEFT_UP);
        down.SetPosition({4, 4});
        up.SetPosition({4, 4});
        row->GetEventHandler()->ProcessEvent(down);
        row->GetEventHandler()->ProcessEvent(up);
        wxTheApp->ProcessPendingEvents();
    };
    const auto openable = std::ranges::count(day_.readings, true, &Reading::can_open) +
                          std::ranges::count_if(day_.variants, [](const auto& variant) {
                              return variant.reading && variant.reading->can_open();
                          });
    // Every reading has one row, then the next plan part and all plans.
    if (std::ranges::count_if(home_rows_, &ReadingRow::opens) != openable + 2 ||
        home_rows_.back()->GetLabel() != ui::utf8("Alla läsplaner"))
        fail(__LINE__);
    // The Liturgy's Epistle and Gospel come first.
    if (home_rows_.empty() || !home_rows_.front()->GetLabel().Contains("Epistel"))
        fail(__LINE__);
    // Daily rows show the same passage checkpoint as plans and resume, including other entry points.
    const auto daily = std::ranges::find_if(day_.readings, [](const Reading& reading) {
        return reading.can_open() && reading.service == "Epistle";
    });
    if (daily != day_.readings.end()) {
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): refresh_day replaces day_.readings.
        const auto reading = *daily;
        const auto key = day_key(reading);
        const auto shared = reading_key(corpus_, reader_reading(reading));
        user_.position(shared, {0, reading.passage.book, "sv1917", reading.passage.first, 37});
        refresh_day();
        if (!home_rows_.front()->GetLabel().Contains(ui::utf8(" · 37 %")))
            fail(__LINE__);
        user_.position(key, {0, reading.passage.book, "sv1917", reading.passage.first, 62});
        refresh_day();
        if (!home_rows_.front()->GetLabel().Contains(ui::utf8(" · 62 %")))
            fail(__LINE__);
        if (!screenshot_path.empty()) {
            readings_->Refresh();
            readings_->Update();
            Update();
            wxBitmap bitmap;
            bitmap.CreateWithLogicalSize(GetClientSize(), GetContentScaleFactor());
            wxClientDC window(this);
            {
                wxMemoryDC image(bitmap);
                image.Blit(0, 0, GetClientSize().x, GetClientSize().y, &window, 0, 0);
            }
            save(bitmap.ConvertToImage(), "-daily-progress");
        }
        user_.complete(key);
        refresh_day();
        if (home_rows_.front()->GetLabel().Contains("%") ||
            !home_rows_.front()->GetLabel().EndsWith(ui::utf8(", läst")))
            fail(__LINE__);
        user_.forget(key);
        user_.reset_position(key);
        user_.reset_position(shared);
        refresh_day();
        if (home_rows_.front()->GetLabel().Contains("%"))
            fail(__LINE__);
    } else
        fail(__LINE__);
    ReadingRow* variant_row = nullptr;
    for (auto* row : home_rows_)
        if (row->GetLabel().StartsWith("Antiokisk"))
            variant_row = row;
    if (variant_row) {
        click_row(variant_row);
        if (!visible_reading_ || visible_reading_->passage.last != VerseRef{11, 30} || !tracked_ ||
            tracked_->key != "day:2026-05-10:Acts 11:19-11:30")
            fail(__LINE__);
    } else
        fail(__LINE__);
    show_readings();
    click_row(home_rows_.back());
    if (current_page_title() != ui::utf8("Läsplaner"))
        fail(__LINE__);
    close_pages();
    tracked_.reset();
    show_readings();
    settings_.tradition = previous_tradition;
    settings_.calendar = previous_calendar;
    select_day(date);
    const auto click_arrow = [this](size_t index) {
        auto* button = date_buttons_[index];
        wxMouseEvent down(wxEVT_LEFT_DOWN), up(wxEVT_LEFT_UP);
        down.SetPosition({4, 4});
        up.SetPosition({4, 4});
        button->GetEventHandler()->ProcessEvent(down);
        button->GetEventHandler()->ProcessEvent(up);
        wxTheApp->ProcessPendingEvents();
    };
    select_day(parse_date("2019-01-01").value());
    if (date_buttons_[0]->IsEnabled() || !date_buttons_[1]->IsEnabled())
        fail(__LINE__);
    navigate(-1);
    if (selected_.date() != parse_date("2019-01-01").value())
        fail(__LINE__);
    pick_date();
    month_->shift_month(-1);
    if (month_->focused() != parse_date("2019-01-01").value())
        fail(__LINE__);
    month_->focus(parse_date("2018-12-31").value());
    if (month_->focused() != parse_date("2019-01-01").value())
        fail(__LINE__);
    pick_date();
    select_day(date);
    click_arrow(1);
    if (selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    click_arrow(0);
    if (selected_.date() != date)
        fail(__LINE__);
    for (auto* child : home_content_->GetChildren())
        if (auto* button = dynamic_cast<TextButton*>(child); button && button->GetLabel() == "Idag")
            fail(__LINE__);
    // The calendar folds out on the start page; a key moves the day and Return chooses it.
    pick_date();
    if (current_page() || !month_ || !month_->IsShown() || month_->focused() != date)
        fail(__LINE__);
    const auto press = [this](int code) {
        wxKeyEvent key(wxEVT_KEY_DOWN);
        key.m_keyCode = code;
        month_->GetEventHandler()->ProcessEvent(key);
        wxTheApp->ProcessPendingEvents();
    };
    const auto calendar_arrow = [this, &fail](int step) {
        const auto area = month_->arrow(step);
        const wxPoint point{area.x + area.width / 2, area.y + area.height / 2};
        if (month_->arrow_at(point) != step)
            fail(__LINE__);
        wxMouseEvent hover(wxEVT_MOTION), click(wxEVT_LEFT_UP);
        hover.SetPosition(point);
        click.SetPosition(point);
        month_->GetEventHandler()->ProcessEvent(hover);
        const auto tooltip = month_->GetToolTipText();
        if (!tooltip.Contains(ui::utf8(std::abs(step) == 12 ? "år" : "månad")))
            fail(__LINE__);
        month_->GetEventHandler()->ProcessEvent(click);
    };
    month_->focus(parse_date("2024-02-29").value());
    calendar_arrow(12);
    if (month_->focused() != parse_date("2025-02-28").value())
        fail(__LINE__);
    calendar_arrow(-12);
    if (month_->focused() != parse_date("2024-02-28").value())
        fail(__LINE__);
    calendar_arrow(1);
    if (month_->focused() != parse_date("2024-03-28").value())
        fail(__LINE__);
    calendar_arrow(-1);
    if (month_->focused() != parse_date("2024-02-28").value())
        fail(__LINE__);
    for (const int group : {0, 1}) {
        const auto area = month_->title(group);
        wxMouseEvent hover(wxEVT_MOTION);
        hover.SetPosition({area.x + area.width / 2, area.y + area.height / 2});
        month_->GetEventHandler()->ProcessEvent(hover);
        if (month_->GetToolTipText() != ui::utf8(group == 0 ? "Välj månad" : "Välj år"))
            fail(__LINE__);
    }
    // The titles open grids of months and years; a cell moves there and shows the days again.
    const auto click_at = [this](const wxRect& area) {
        wxMouseEvent click(wxEVT_LEFT_UP);
        click.SetPosition({area.x + area.width / 2, area.y + area.height / 2});
        month_->GetEventHandler()->ProcessEvent(click);
    };
    month_->focus(parse_date("2024-01-31").value());
    click_at(month_->title(0));
    if (month_->view_ != MonthCalendar::View::Months)
        fail(__LINE__);
    click_at(month_->grid_cell(1));
    if (month_->view_ != MonthCalendar::View::Days || month_->focused() != parse_date("2024-02-29").value())
        fail(__LINE__);
    click_at(month_->title(1));
    if (month_->grid_date(0) != parse_date("2019-02-28").value())
        fail(__LINE__);
    calendar_arrow(12);
    if (month_->focused() != parse_date("2036-02-29").value() ||
        month_->grid_date(0) != parse_date("2031-02-28").value())
        fail(__LINE__);
    click_at(month_->grid_cell(0));
    if (month_->view_ != MonthCalendar::View::Days || month_->focused() != parse_date("2031-02-28").value())
        fail(__LINE__);
    // Paging years keeps the grid's height: October 2038 has six weeks, October 2026 five.
    month_->focus(parse_date("2026-10-14").value());
    click_at(month_->title(1));
    const auto grid_size = month_->GetBestSize();
    calendar_arrow(12);
    if (month_->GetBestSize() != grid_size)
        fail(__LINE__);
    click_at(month_->title(1));
    month_->focus(parse_date("2024-02-28").value());
    wxKeyEvent next_year(wxEVT_KEY_DOWN);
    next_year.m_keyCode = WXK_PAGEDOWN;
    next_year.m_shiftDown = true;
    month_->GetEventHandler()->ProcessEvent(next_year);
    if (month_->focused() != parse_date("2025-02-28").value())
        fail(__LINE__);
    month_->focus(parse_date("2019-01-01").value());
    for (int step : {-1, -12}) {
        const auto area = month_->arrow(step);
        if (month_->arrow_at({area.x + area.width / 2, area.y + area.height / 2}) != 0)
            fail(__LINE__);
        month_->shift_month(step);
    }
    if (month_->focused() != parse_date("2019-01-01").value())
        fail(__LINE__);
    month_->focus(parse_date("9999-12-31").value());
    for (int step : {1, 12}) {
        const auto area = month_->arrow(step);
        if (month_->arrow_at({area.x + area.width / 2, area.y + area.height / 2}) != 0)
            fail(__LINE__);
        month_->shift_month(step);
    }
    if (month_->focused() != parse_date("9999-12-31").value())
        fail(__LINE__);
    month_->focus(date);
    press(WXK_RIGHT);
    press(WXK_RETURN);
    if (month_ || month_open_ || selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    pick_date();
    press(WXK_ESCAPE);
    if (month_ || selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    pick_date();
    press(WXK_HOME);
    const auto today = local_civil_date();
    if (month_->focused() != today || !month_->description(today).Contains("Idag") ||
        !month_->GetLabel().Contains(ui::utf8(date_swedish(today))))
        fail(__LINE__);
    const auto calendar_image = [&](const wxString& suffix, Theme theme) {
        if (screenshot_path.empty())
            return;
        month_->apply(palette(theme));
        month_->Refresh();
        month_->Update();
        wxBitmap bitmap;
        bitmap.CreateWithLogicalSize(month_->GetClientSize(), month_->GetContentScaleFactor());
        wxClientDC window(month_);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, month_->GetClientSize().x, month_->GetClientSize().y, &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), suffix);
    };
    calendar_image("-calendar-light", Theme::Light);
    calendar_image("-calendar-dark", Theme::Dark);
    month_->focus(parse_date("2026-09-30").value());
    calendar_image("-calendar-navigation", Theme::Light);
    month_->focus(parse_date("2019-01-01").value());
    calendar_image("-calendar-first-year", Theme::Light);
    month_->focus(today);
    month_->apply(palette(settings_.theme));
    press(WXK_RETURN);
    if (month_ || selected_.date() != today)
        fail(__LINE__);
    settings_.calendar = CalendarStyle::New;
    settings_.tradition = Tradition::Greek;
    select_day(parse_date("2026-03-25").value());
    for (const auto& label : home_labels_)
        if (label.text.Contains(ui::utf8("Avstå från")) || label.text.Contains(ui::utf8("Vin och olja")) ||
            label.text.Contains(ui::utf8("bebådelse")))
            fail(__LINE__);
    pick_date();
    const auto annunciation = selected_.date();
    if (month_->days_.at(annunciation).marked != std::array{true, true, true} ||
        !month_->description(annunciation).Contains(ui::utf8("Ortros: 1 läsning")) ||
        !month_->description(annunciation).Contains(ui::utf8("Vesper:")) ||
        !month_->description(annunciation).Contains(ui::utf8("Sjätte timmen:")) ||
        month_->description(annunciation).Contains(ui::utf8("bebådelse")) ||
        month_->description(annunciation).Contains(ui::utf8("fastan")))
        fail(__LINE__);
    calendar_image("-calendar-services", Theme::Light);
    // Tooltips follow the actual date under the pointer and describe its service readings.
    const int offset = int(std::chrono::weekday{std::chrono::sys_days{month_->first_shown()}}.c_encoding());
    wxMouseEvent hover(wxEVT_MOTION);
    const auto hovered_cell = month_->cell(offset + 24);
    hover.SetPosition({hovered_cell.x + hovered_cell.width / 2, hovered_cell.y + hovered_cell.height / 2});
    month_->GetEventHandler()->ProcessEvent(hover);
    if (month_->GetToolTipText() != month_->description(annunciation))
        fail(__LINE__);
    month_->focus(parse_date("2026-05-10").value());
    if (!month_->description(month_->focused()).Contains(ui::utf8("Antiokisk, Epistel: 1 läsning")))
        fail(__LINE__);
    month_->focus(parse_date("2026-10-09").value());
    if (month_->days_.at(month_->focused()).marked != std::array{true, true, false} ||
        !month_->description(month_->focused()).Contains(ui::utf8("Vesper: 1 läsning")) ||
        !month_->description(month_->focused()).Contains(ui::utf8("Ortros: 1 läsning")))
        fail(__LINE__);
    month_->focus(parse_date("2026-10-08").value());
    if (month_->days_.at(month_->focused()).marked != std::array{false, false, false} ||
        !month_->description(month_->focused()).Contains(ui::utf8("Epistel: 1 läsning")) ||
        !month_->description(month_->focused()).Contains(ui::utf8("Evangelium: 1 läsning")))
        fail(__LINE__);
    month_->focus(parse_date("2026-04-12").value());
    if (month_->days_.at(month_->focused()).marked != std::array{false, false, false} ||
        month_->description(month_->focused()).Contains("Pascha") || month_->days_.size() != 30)
        fail(__LINE__);
    calendar_image("-calendar-pascha", Theme::Light);
    settings_.calendar = CalendarStyle::Old;
    month_->focus(parse_date("2026-01-07").value());
    if (!month_->description(month_->focused()).Contains("2025-12-25") ||
        month_->days_.at(month_->focused()).marked != std::array{true, false, false} ||
        !month_->description(month_->focused()).Contains("Ortros:") ||
        month_->description(month_->focused()).Contains(ui::utf8("Kristi födelse")))
        fail(__LINE__);
    press(WXK_ESCAPE);
    settings_.calendar = CalendarStyle::New;
    select_day(parse_date("2026-01-05").value());
    pick_date();
    if (month_->days_.at(month_->focused()).marked != std::array{false, true, true} ||
        !month_->description(month_->focused()).Contains(ui::utf8("Stora vattenvälsignelsen:")) ||
        !month_->description(month_->focused()).Contains(ui::utf8("Första timmen")))
        fail(__LINE__);
    press(WXK_ESCAPE);
    select_day(parse_date("2026-10-14").value());
    if (home_labels_.empty() || home_labels_.front().text != ui::utf8("Onsdag i 20:e veckan efter pingst") ||
        !(home_labels_.front().label->GetWindowStyleFlag() & wxALIGN_CENTER_HORIZONTAL))
        fail(__LINE__);
    pick_date();
    const auto date_row = entries_->GetItem(std::size_t{1})->GetRect();
    if (std::abs(date_row.x * 2 + date_row.width - home_content_->GetSize().x) > 2 ||
        std::abs(month_->GetPosition().x * 2 + month_->GetSize().x - home_content_->GetSize().x) > 2)
        fail(__LINE__);
    if (!screenshot_path.empty()) {
        readings_->Refresh();
        readings_->Update();
        wxBitmap bitmap;
        bitmap.CreateWithLogicalSize(readings_->GetClientSize(), readings_->GetContentScaleFactor());
        wxClientDC window(readings_);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, readings_->GetClientSize().x, readings_->GetClientSize().y, &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), "-day-heading");
    }
    press(WXK_ESCAPE);
    settings_.tradition = previous_tradition;
    settings_.calendar = previous_calendar;
    select_day(date);
    open_fresh_psalm();
    pick_date();
    if (!readings_->IsShown() || scripture_->IsShown() || !month_)
        fail(__LINE__);
    pick_date();
    if (month_)
        fail(__LINE__);
    select_day(date);
    if (std::abs(home_content_->GetPosition().x * 2 + home_content_->GetSize().x -
                 readings_->GetClientSize().x) > 2)
        fail(__LINE__);
    const auto home_scroll = readings_->GetViewStart();
    about();
    back();
    if (!readings_->IsShown() || scripture_->IsShown() || back_->IsEnabled() != history_.can_back() ||
        readings_->GetViewStart() != home_scroll)
        fail(__LINE__);
    std::cout << "Embedded pages: previous view, scroll, selection, study, parts, date controls.\n";
    settings_.word_study = false;
    apply_settings(false);
    if (study_->IsShown())
        fail(__LINE__);
    // Exercise playback presentation without model loading or audible output.
    open_fresh_psalm();
    settings_.theme = Theme::Light;
    set_parallel("el");
    apply_settings(false);
    // The same focused control changes to Pause, so Space can toggle it again.
    play_->SetFocus();
    const bool focusable = FindFocus() == play_;
    display_playback({SpeechState::Buffering, {}, 0, 0});
    // Play/Pause and Stop keep their positions while preparing audio.
    if (address_->GetLabel() != ui::utf8("Förbereder uppläsningen…") || !play_->IsEnabled() ||
        play_->GetLabel() != "Pausa" || !stop_->IsEnabled() || !play_->IsShown() || !stop_->IsShown() ||
        (focusable && FindFocus() != play_))
        fail(__LINE__);
    // Playback starts on Scripture and immediately shows the first verse.
    const SpeechCue first_verse{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}};
    playback_.start({});
    display_playback({SpeechState::Playing, first_verse, 0, 0.01});
    if (address_->GetLabel() != ui::utf8("Psaltaren 23:1") || !scripture_->marker_position())
        fail(__LINE__);
    display_playback({SpeechState::Buffering, {}, 0, 0, 0.4});
    if (address_->GetLabel() != ui::utf8("Förbereder uppläsningen… 40 %") || scripture_->marker_position())
        fail(__LINE__);
    SpeechPlayback playing{SpeechState::Playing, SpeechCue{0, 0, "Ps", "sv1917", {23, 3}, {23, 3}}, 0.35,
                           0.4};
    scripture_->playback(playing);
    scripture_->follow_playback();
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position() || !scripture_->follows_playback())
        fail(__LINE__);
    const auto held_marker = scripture_->marker_position();
    const auto held_scroll = scripture_->scroll_position();
    auto paused = playing;
    paused.state = SpeechState::Paused;
    playback_timer_.Start(30);
    display_playback(paused);
    if (playback_timer_.IsRunning())
        fail(__LINE__);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        play_->GetLabel() != ui::utf8("Fortsätt") || !play_->IsEnabled() || !stop_->IsEnabled() ||
        (focusable && FindFocus() != play_))
        fail(__LINE__);
    // Back must preserve the engine's paused state and position; toolbar actions are suspended.
    {
        auto real_speech = std::move(speech_);
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        engine->held = paused;
        speech_ = std::move(held);
        for (int mode = 0; mode < 2; ++mode) {
            if (mode == 0)
                about();
            else
                browse_bible();
            display_playback(paused);
            if (!current_page() || play_->IsEnabled() || stop_->IsEnabled() ||
                playback_.playback().state != SpeechState::Paused)
                fail(__LINE__);
            play_or_pause();
            back();
            if (current_page() || engine->changed || playback_.playback().state != SpeechState::Paused ||
                playback_.playback().progress != paused.progress ||
                scripture_->marker_position() != held_marker ||
                scripture_->scroll_position() != held_scroll || !scripture_->follows_playback())
                fail(__LINE__);
        }
        // New playback cues cannot replace a page while the user is in it.
        engine->held = playing;
        about();
        playback_.forget_location();
        display_playback(playing);
        if (!current_page() || scripture_->IsShown() || playback_.location())
            fail(__LINE__);
        close_page();
        playback_.location(std::pair<size_t, size_t>{0, 0});
        speech_ = std::move(real_speech);
        display_playback(paused);
    }
    auto buffering = playing;
    buffering.state = SpeechState::Buffering;
    display_playback(buffering);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        address_->GetLabel() != ui::utf8("Förbereder fortsättningen…"))
        fail(__LINE__);
    for (int i = 0; i < 120; ++i)
        scripture_->advance_playback(0.016);
    const auto settled = scripture_->render_stats();
    for (int i = 0; i < 120; ++i) {
        scripture_->playback(buffering);
        scripture_->advance_playback(0.016);
    }
    if (scripture_->animating() || scripture_->render_stats().repaint_requests != settled.repaint_requests)
        fail(__LINE__);
    display_playback(playing);
    display_playback(buffering);
    if (playback_.feedback() != SpeechState::Playing)
        fail(__LINE__);
    playback_.update(buffering, PlaybackController::Clock::now() + std::chrono::seconds(1));
    if (playback_.feedback() != SpeechState::Buffering)
        fail(__LINE__);
    scripture_->scroll_by(20);
    if (scripture_->follows_playback())
        fail(__LINE__);
    if (!screenshot_path.empty()) {
        display_playback(playing);
        scripture_->Update();
        wxBitmap bitmap(scripture_->GetClientSize());
        wxClientDC window(scripture_);
        {
            wxMemoryDC image(bitmap);
            image.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &window, 0, 0);
        }
        save(bitmap.ConvertToImage(), "-follow-button");
    }
    scripture_->follow_playback();
    display_playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    settings_.speech_highlight = true;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    const auto highlighted = render();
    save(highlighted, "-playing");
    // Without the highlight, the reader still follows the voice and keeps the margin marker, but draws no
    // tint.
    settings_.speech_highlight = false;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    const auto plain = render();
    if (!scripture_->marker_position() ||
        std::equal(plain.GetData(), plain.GetData() + size_t(plain.GetWidth()) * plain.GetHeight() * 3,
                   highlighted.GetData()))
        fail(__LINE__);
    save(plain, "-playing-plain");
    settings_.speech_highlight = true;
    settings_.theme = Theme::Dark;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    save(render(), "-playing-dark");
    // Reflow keeps the spoken verse, including the other language column.
    settings_.text_zoom = 125;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position())
        fail(__LINE__);
    auto greek = playing;
    greek.cue = SpeechCue{0, 0, "Ps", "grc-lxx", {22, 3}, {22, 3}};
    scripture_->playback(greek);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position())
        fail(__LINE__);
    save(render(), "-playing-greek");
    SetSize(FromDIP(wxSize(520, 650)));
    Layout();
    root_->Layout();
    display_playback(paused);
    for (wxWindow* control :
         {static_cast<wxWindow*>(back_), static_cast<wxWindow*>(play_), static_cast<wxWindow*>(stop_),
          static_cast<wxWindow*>(address_), static_cast<wxWindow*>(panes_.back().second)})
        if (control->IsShown() && control->GetRect().GetRight() > bar_->GetClientSize().x)
            fail(__LINE__);
    save(render(), "-playing-narrow");
    SetSize(original_size);
    Layout();
    root_->Layout();
    settings_.text_zoom = original.text_zoom;
    apply_settings(false);
    scripture_->playback(playing);
    scripture_->follow_playback();
    auto distant = playing;
    distant.cue->verse = distant.cue->last = {24, 10};
    scripture_->playback(distant);
    for (int i = 0; i < 160; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position() || scripture_->scroll_position() <= held_scroll + 100)
        fail(__LINE__);
    display_playback({SpeechState::Stopped, {}, 0, 0});
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() ||
        play_->GetLabel() != (has_reading_position() ? ui::utf8("Fortsätt") : wxString("Lyssna")) ||
        !play_->IsEnabled() || stop_->IsEnabled() != has_reading_position())
        fail(__LINE__);
    // Following, section changes and browsing all use the reader's follow state.
    {
        auto real_speech = std::move(speech_);
        auto held = std::make_unique<NavigationSpeech>();
        auto* engine = held.get();
        speech_ = std::move(held);
        const Reading psalms{
            ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}, {{"Ps", {24, 1}, {24, 10}}}};
        const Reading gospel{ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}};
        playback_.start({psalms, gospel});
        engine->held = {SpeechState::Paused, SpeechCue{0, 1, "Ps", "sv1917", {24, 1}, {24, 1}}, 0, 0.5};
        follow_speech();
        if (part_ != 1 || parts_.size() != 2 || !scripture_->follows_playback() ||
            playback_.location() != std::pair<size_t, size_t>{0, 1} || engine->changed)
            fail(__LINE__);
        // A cue for the next reading cannot take over after a manual scroll.
        scripture_->scroll_by(20);
        engine->held.cue = SpeechCue{1, 0, "John", "sv1917", {1, 1}, {1, 1}};
        display_playback(engine->held);
        if (scripture_->follows_playback() || visible_reading_->passage.book != "Ps")
            fail(__LINE__);
        follow_speech();
        if (part_ != 0 || visible_reading_->passage.book != "John" || !scripture_->follows_playback() ||
            engine->changed)
            fail(__LINE__);
        about();
        engine->held.cue = SpeechCue{0, 1, "Ps", "sv1917", {24, 1}, {24, 1}};
        display_playback(engine->held);
        if (!current_page() || visible_reading_->passage.book != "John")
            fail(__LINE__);
        back();
        if (current_page() || part_ != 1 || visible_reading_->passage.book != "Ps" ||
            !scripture_->follows_playback() || engine->changed)
            fail(__LINE__);
        select_part(0);
        display_playback(engine->held);
        if (part_ != 0 || scripture_->follows_playback())
            fail(__LINE__);
        follow_speech();
        if (part_ != 1 || !scripture_->follows_playback())
            fail(__LINE__);
        show_readings();
        display_playback(engine->held);
        if (scripture_->IsShown() || scripture_->follows_playback())
            fail(__LINE__);
        follow_speech();
        if (!scripture_->IsShown() || part_ != 1 || !scripture_->follows_playback() || engine->changed)
            fail(__LINE__);
        // Leaving an audible reading pauses it; undoing navigation never resumes it.
        for (int mode = 0; mode < 3; ++mode) {
            engine->held.state = SpeechState::Playing;
            engine->changed = false;
            display_playback(engine->held);
            const auto progress = engine->held.progress;
            if (mode == 0)
                about();
            else if (mode == 1)
                browse_bible();
            else
                show_plans();
            if (!engine->changed || engine->held.state != SpeechState::Paused ||
                playback_.playback().state != SpeechState::Paused || playback_timer_.IsRunning() ||
                play_->IsEnabled() || stop_->IsEnabled())
                fail(__LINE__);
            engine->changed = false;
            if (mode == 1)
                address_clicked();
            else
                back();
            if (current_page() || engine->changed || engine->held.state != SpeechState::Paused ||
                engine->held.progress != progress || !play_->IsEnabled())
                fail(__LINE__);
        }
        engine->held.state = SpeechState::Buffering;
        about();
        if (engine->held.state != SpeechState::Paused)
            fail(__LINE__);
        back();
        engine->held.state = SpeechState::Playing;
        show_readings();
        if (engine->held.state != SpeechState::Paused || play_->IsEnabled() || stop_->IsEnabled())
            fail(__LINE__);
        engine->changed = false;
        play_or_pause();
        address_clicked();
        address_clicked();
        if (engine->changed || current_page() || !readings_->IsShown() || scripture_->IsShown() ||
            play_->IsEnabled() || stop_->IsEnabled())
            fail(__LINE__);
        // Choosing a reading stops the previous session and restores its own saved position.
        open_reading(gospel);
        if (!engine->changed || engine->held.state != SpeechState::Stopped ||
            playback_.playback().state != SpeechState::Stopped || visible_reading_->passage.book != "John" ||
            scripture_->marker_position() || !play_->IsEnabled() ||
            stop_->IsEnabled() != has_reading_position() ||
            play_->GetLabel() != (has_reading_position() ? ui::utf8("Fortsätt") : wxString("Lyssna")))
            fail(__LINE__);
        engine->held = {SpeechState::Paused, SpeechCue{0, 0, "Ps", "sv1917", {23, 3}, {23, 3}}, 0.35, 0.4};
        follow_speech();
        browse_bible();
        auto* browser = dynamic_cast<BiblePicker*>(current_page());
        browser->select_book(john);
        browser->select_chapter(john, 1);
        browser->select_verse(john, {1, 2});
        engine->changed = false;
        browser->open_selection();
        if (current_page() || !engine->changed || engine->held.state != SpeechState::Stopped ||
            visible_reading_->passage.first != VerseRef{1, 2})
            fail(__LINE__);
        // Automatic queue transitions continue playback rather than replacing its session.
        engine->held = {SpeechState::Playing, SpeechCue{0, 0, "Ps", "sv1917", {23, 3}, {23, 3}}, 0.35, 0.4};
        engine->changed = false;
        follow_speech();
        engine->held.cue = SpeechCue{1, 0, "John", "sv1917", {1, 1}, {1, 1}};
        display_playback(engine->held);
        if (engine->changed || engine->held.state != SpeechState::Playing ||
            visible_reading_->passage.book != "John" || !scripture_->follows_playback())
            fail(__LINE__);
        speech_ = std::move(real_speech);
        display_playback({SpeechState::Stopped, {}, 0, 0});
    }
    std::cout << "Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_fresh_psalm();
    settings_ = original;
    apply_settings(false);
    if (selected_.date() != date)
        fail(__LINE__);
    if (!smoke_menu())
        fail(__LINE__);
    return ok;
}
} // namespace ortho
