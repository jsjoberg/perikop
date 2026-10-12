#include "core/reading_display.hpp"
#include "core/reading_plan.hpp"
#include "storage/sqlite.hpp"
#include "ui/app_icon.hpp"
#include "ui/bible_picker.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/month_calendar.hpp"
#include "ui/scripture_view.hpp"
#include "ui/study_panel.hpp"
#include "ui/toolbar.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <wx/access.h>
#include <wx/app.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/fontenum.h>
#include <wx/graphics.h>
#include <wx/menu.h>
namespace ortho {
namespace {
// Navigation tests need a held playback position without model loading or sound output.
class NavigationSpeech final : public SpeechEngine {
public:
    SpeechPlayback held;
    bool changed = false;
    std::vector<std::function<void(std::string)>> lookups;
    void speak(const SpeechUtterance&) override {
        changed = true;
    }
    void pause() override {
        changed = true;
    }
    void resume() override {
        changed = true;
    }
    void stop() override {
        changed = true;
    }
    SpeechPlayback playback() const override {
        return held;
    }
    void pronunciation_async(const std::string&, std::function<void(std::string)> deliver) override {
        lookups.push_back(std::move(deliver));
    }
};
} // namespace
bool MainFrame::smoke_test(const wxString& screenshot_path) {
    const auto original = settings_;
    const auto date = selected_.date();
    open_psalm();
    Layout();
    bool ok = true;
    // Names the failed check, since the checks otherwise fail silently.
    const auto fail = [&ok](int line) {
        std::cerr << "Smoke check failed: main_frame_smoke.cpp:" << line << '\n';
        ok = false;
    };
    // Optional pronunciation failures preserve the profile and leave reading usable.
    for (const bool future : {false, true}) {
        const auto data = data_ / (future ? "future-pronunciation" : "corrupt-pronunciation");
        UserDb profile(data / "user.db");
        auto settings = profile.load();
        settings.speech_voice = "bjorn";
        profile.save(settings);
        profile.complete("preserved-progress");
        const auto path = data / "pronunciation-review.db";
        if (future) {
            {
                PronunciationReviewDb review(path);
                review.save({"RecoverySentinel", "corrected", "preserved correction", "future schema"});
            }
            auto db = storage::open(path, SQLITE_OPEN_READWRITE);
            storage::exec(db.get(), "PRAGMA user_version=99");
        } else
            std::ofstream(path) << "preserve corrupt pronunciation bytes";
        const auto contents = [&] {
            std::ifstream file(path, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(file)), {});
        };
        const auto before = contents();
        {
            auto candidate = std::make_unique<MainFrame>(corpus_, profile, date, resources_, data, true);
            candidate->open_psalm();
            if (candidate->pronunciation_review_ || !candidate->speech_ ||
                candidate->scripture_->reading().passage.book != "Ps" ||
                candidate->speech_lexicon("sv").size() != corpus_.pronunciations("sv").size())
                fail(__LINE__);
        }
        if (contents() != before || profile.load().speech_voice != "bjorn" ||
            !profile.completed().contains("preserved-progress"))
            fail(__LINE__);
    }
    if (!corpus_.read_only())
        fail(__LINE__);
    // The ⋮ menu is always there. Windows and Linux hide the menu bar until Alt; the checks below use it.
    if (!menu_ || !menu_->IsShown() || !menu_bar_)
        fail(__LINE__);
#ifndef __WXOSX__
    if (GetMenuBar())
        fail(__LINE__);
    show_menu_bar(true);
#endif
    if (GetMenuBar() != menu_bar_)
        fail(__LINE__);
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
                    enumerate(end + 1, fitness,
                              cost + std::pow(10 + badness, 2) +
                                  (std::abs(previous - fitness) > 1 ? 10000 : 0));
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
                layout_paragraph(metrics, std::vector<TextFragment>{{ui::utf8("MMMM שלום Λόγος"), "7", 0}},
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
    if (hyphenation_points("begynnelsen", "sv").empty() || hyphenation_points("beginning", "en").empty() ||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"), "el").empty())
        fail(__LINE__);
    if (!hyphenation_points("project", "en").empty())
        fail(__LINE__);
    const auto optical = layout_paragraph(metrics, wxString::FromUTF8("“Guds ord.”"), 280, "sv");
    if (optical.lines.front().left_protrusion <= 0 || optical.lines.back().right_protrusion <= 0)
        fail(__LINE__);
    settings_.theme = Theme::Light;
    settings_.parallel.clear();
    settings_.word_study = false;
    settings_.font_size = 19;
    apply_settings(false);
    scripture_->center_passage();
    {
        // A wide single column must retain its reading measure in font points.
        // The old 680-DIP cap fitted about 25 em on Windows versus 34 em on macOS.
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
        const double em = 19.0 * metrics.GetPPI().x / 72.0;
        // Allow the page margins when the CI display cannot fit the full column.
        const double minimum = std::min(32 * em, double(image.GetWidth() - scripture_->FromDIP(110)));
        if (last - first + 1 < minimum)
            fail(__LINE__);
        save(image, "-single");
    }
    settings_.parallel = "el";
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
            settings_.parallel = mode;
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
    settings_.parallel = "en";
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
    for (auto* child : home_content_->GetChildren())
        if (auto* control = dynamic_cast<wxButton*>(child); control && control->GetLabel().Contains("Lyssna"))
            fail(__LINE__);
    if (!bar_->IsShown() || play_->IsEnabled() || pause_->IsEnabled() || stop_->IsEnabled() ||
        back_->IsEnabled() || panes_.front().second->IsEnabled() ||
        address_->GetLabel() != ui::utf8("Dagens läsningar"))
        fail(__LINE__);
    // Repeated layout keeps long plan explanations wrapped in the centered column.
    layout_home();
    layout_home();
    for (const auto& [label, text, beside] : home_labels_)
        if (text.length() > 100 && !label->GetLabel().Contains("\n"))
            fail(__LINE__);
    // The start page offers the three reading plans; a marked part shows as read.
    if (plan_tiles_.size() != 74)
        fail(__LINE__);
    user_.complete(plan_key(reading_plans().front(), 0));
    refresh_day();
    if (!plan_tiles_.front()->GetToolTipText().EndsWith(ui::utf8("läst")) ||
        plan_tiles_[1]->GetToolTipText().EndsWith(ui::utf8("läst")))
        fail(__LINE__);
    user_.forget("plan:");
    refresh_day();
    if (plan_tiles_.front()->GetToolTipText().EndsWith(ui::utf8("läst")))
        fail(__LINE__);
    // All plan parts have native button semantics and a descriptive accessible name.
    for (auto* tile : plan_tiles_)
        if (!tile->AcceptsFocus() || !tile->GetName().Contains("Del ") || !tile->GetName().Contains(" · "))
            fail(__LINE__);
#if wxUSE_ACCESSIBILITY
    for (auto* tile : plan_tiles_) {
        wxString name;
        wxAccRole role = wxROLE_NONE;
        auto* accessible = tile->GetAccessible();
        if (!accessible || accessible->GetName(wxACC_SELF, &name) != wxACC_OK ||
            accessible->GetRole(wxACC_SELF, &role) != wxACC_OK || name != tile->GetName() ||
            role != wxROLE_SYSTEM_PUSHBUTTON)
            fail(__LINE__);
    }
#endif
    wxCommandEvent open_part(wxEVT_BUTTON, plan_tiles_.back()->GetId());
    plan_tiles_.back()->GetEventHandler()->ProcessEvent(open_part);
    if (!scripture_->IsShown() || !tracked_)
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
    toggle_pane("el");
    if (settings_.parallel != "el" || settings_.word_study)
        fail(__LINE__);
    toggle_pane("study");
    if (!settings_.parallel.empty() || !settings_.word_study ||
        !menu_bar_->IsChecked(menu_bar_->FindMenuItem("Visa", "Svenska och ordstudium")))
        fail(__LINE__);
    toggle_pane("study");
    if (settings_.word_study || !menu_bar_->IsChecked(menu_bar_->FindMenuItem("Visa", "Bara svenska")))
        fail(__LINE__);
    // The left column is always Swedish, so no symbol offers Swedish beside it.
    if (std::ranges::any_of(panes_, [](const auto& pane) {
            return pane.first == "sv";
        }))
        fail(__LINE__);
    // The text size returns to its default.
    {
        const auto size = settings_.font_size;
        settings_.font_size = 26;
        wxCommandEvent normal(wxEVT_MENU, menu_bar_->FindMenuItem("Visa", "Normal textstorlek"));
        ProcessWindowEvent(normal);
        if (settings_.font_size != Settings{}.font_size)
            fail(__LINE__);
        settings_.font_size = size;
        apply_settings(false);
    }
    settings_.parallel = "";
    apply_settings(false);
    for (const auto& reading :
         std::vector<Reading>{{ReadingKind::OldTestament, {"Gen", {31, 50, "a"}, {31, 50, "a"}}, {}, "el"},
                              {ReadingKind::OldTestament, {"4Macc", {8, 29}, {8, 29}}, {}, "en"},
                              {ReadingKind::OldTestament, {"1Esd", {1, 1}, {1, 1}}}}) {
        open_reading(reading);
        scripture_->center_passage();
        if (scripture_->cached_rows() == 0)
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
    open_psalm();
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
    // Ordstudium replaces the right pane; a Swedish word shows Dalin and Nyström.
    settings_.word_study = true;
    apply_settings(false);
    open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}});
    if (!study_->IsShown() || scripture_->base_source() != "sv1917")
        fail(__LINE__);
    study_->show({"John", "begynnelsen", {1, 1}, 0}, scripture_->base_source());
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
        auto panel = std::make_unique<StudyPanel>(root_, corpus_, study_db_.get(), deferred,
                                                  [this](const auto& language) {
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
            std::vector<wxButton*> buttons;
            for (auto* child : panel->GetChildren()) {
                auto* button = wxDynamicCast(child, wxButton);
                if (button && (button->GetLabel() == "SFB15" || button->GetLabel() == "SKB" ||
                               button->GetLabel() == "SRB" || button->GetLabel() == "B2026"))
                    buttons.push_back(button);
            }
            return buttons;
        };
        if (source_buttons().size() != 4 || !has("Joh 1:1"))
            fail(__LINE__);
        panel->show({"Tob", "Tobit", {1, 1}, 0}, "sv1917");
        const auto apocrypha_buttons = source_buttons();
        if (apocrypha_buttons.size() != 4 ||
            std::count_if(apocrypha_buttons.begin(), apocrypha_buttons.end(), [](auto* button) {
                return button->IsEnabled();
            }) != 1)
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
        if (!has("DALIN") || !has("begynnelse") || has("STRONG"))
            fail(__LINE__);
    }
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
        auto* more = wxDynamicCast(child, wxButton);
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
        if (!page_ || scripture_->IsShown() || study_->IsShown() || !back_->IsEnabled() ||
            play_->IsEnabled() || pause_->IsEnabled() || stop_->IsEnabled() ||
            address_->IsEnabled() == (mode == 1) ||
            page_->GetClientSize().y < root_->GetClientSize().y - bar_->GetSize().y - FromDIP(4))
            fail(__LINE__);
        for (size_t i = 0; i < GetMenuBar()->GetMenuCount(); ++i) {
#ifdef __WXOSX__
            const bool available = false;
#else
            const bool available = GetMenuBar()->GetMenu(i)->FindItem(wxID_ABOUT) != nullptr;
#endif
            if (GetMenuBar()->IsEnabledTop(i) != available)
                fail(__LINE__);
        }
        if (!GetMenuBar()->IsEnabled(wxID_ABOUT))
            fail(__LINE__);
        back();
        for (size_t i = 0; i < GetMenuBar()->GetMenuCount(); ++i)
            if (!GetMenuBar()->IsEnabledTop(i))
                fail(__LINE__);
        const auto restored = scripture_->selections();
        if (page_ || !scripture_->IsShown() || !study_->IsShown() ||
            scripture_->scroll_position() != saved_scroll || study_content() != saved_study ||
            restored.size() != saved_selection.size() ||
            restored.front().first != saved_selection.front().first || !tracked_ ||
            tracked_->key != "smoke:preserved" ||
            visible_reading_->passage.book != saved_reading->passage.book ||
            visible_reading_->passage.first != saved_reading->passage.first)
            fail(__LINE__);
    }
    about();
    auto* saved_about = page_;
    address_clicked();
    if (!dynamic_cast<BiblePicker*>(page_) || saved_about->IsShown() || page_history_.size() != 1)
        fail(__LINE__);
    back();
    if (page_ != saved_about || !page_->IsShown() || !address_->IsEnabled())
        fail(__LINE__);
    back();
    tracked_.reset();
    scripture_->clear_selection();
    // The normal browser prepares one range and opens it only after confirmation.
    browse_bible();
    auto* picker = dynamic_cast<BiblePicker*>(page_);
    const auto& canon = corpus_.canon();
    const auto john = *std::find_if(canon.begin(), canon.end(), [](const auto& book) {
        return book.code == "John";
    });
    const auto psalms = *std::find_if(canon.begin(), canon.end(), [](const auto& book) {
        return book.code == "Ps";
    });
    if (picker->stacked() || !picker->parts().empty() || address_->GetLabel() != "Bibel" ||
        address_->IsEnabled())
        fail(__LINE__);
    picker->open_selection();
    if (page_ != picker)
        fail(__LINE__);
    picker->select_book(john);
    const auto john_verses = picker->verses_of(john);
    if (picker->first_ != john_verses.front() || picker->last_ != john_verses.back() ||
        picker->parts().size() != 1 || page_ != picker ||
        address_->GetLabel() !=
            ui::utf8(passage_label(corpus_, {{"John", john_verses.front(), john_verses.back()}})))
        fail(__LINE__);
    picker->select_chapter(john, 1);
    // John's chapters and verses unfold beneath its row, above the books that follow it.
    const auto chapters = picker->cells_->unfolded(1), verses = picker->cells_->unfolded(2);
    if (chapters.IsEmpty() || verses.IsEmpty() || !chapters.Contains(verses) ||
        chapters.GetBottom() + FromDIP(30) > picker->cells_->GetSize().y)
        fail(__LINE__);
    picker->select_verse(john, {1, 3});
    if (page_ != picker || picker->first_ != VerseRef{1, 3} || picker->last_ != VerseRef{1, 3})
        fail(__LINE__);
    picker->open_selection();
    if (page_ || scripture_->reading().passage.first != VerseRef{1, 3} ||
        scripture_->reading().passage.last != VerseRef{1, 3} || scripture_->reading().segments().size() != 1)
        fail(__LINE__);
    browse_bible();
    picker = dynamic_cast<BiblePicker*>(page_);
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
    back();
    if (page_ != picker || picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{3, 5})
        fail(__LINE__);
    picker->select_chapter(john, 5);
    if (picker->first_ != VerseRef{3, 3} || picker->last_ != chapter_end(5))
        fail(__LINE__);
    picker->select_verse(john, {5, 5});
    const auto selection_title = ui::utf8(passage_label(corpus_, {{"John", {3, 3}, {5, 5}}}));
    if (page_ != picker || picker->first_ != VerseRef{3, 3} || picker->last_ != VerseRef{5, 5} ||
        address_->GetLabel() != selection_title || address_->GetToolTipText() != selection_title)
        fail(__LINE__);
    update_bar();
    if (address_->GetLabel() != selection_title)
        fail(__LINE__);
    about();
    if (address_->GetLabel() != "Om Perikop")
        fail(__LINE__);
    back();
    if (page_ != picker || address_->GetLabel() != selection_title)
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
    if (page_ || scripture_->reading().passage.first != VerseRef{3, 1} ||
        scripture_->reading().passage.last != chapter_end(3) || scripture_->reading().segments().size() != 1)
        fail(__LINE__);
    // Lägg till stacks selections from several books as the parts of one reading.
    browse_bible();
    picker = dynamic_cast<BiblePicker*>(page_);
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
        ui::utf8("Del 1 · ") + ui::utf8(passage_label(corpus_, {{"John", {1, 50}, {2, 2}}}));
    if (!picker->stacked() || picker->first_ || address_->GetLabel() != first_part || !address_->IsEnabled())
        fail(__LINE__);
    picker->select_book(psalms);
    picker->select_chapter(psalms, 22);
    picker->select_verse(psalms, {22, 6});
    if (picker->parts().size() != 2 || !address_->GetLabel().StartsWith(ui::utf8("Del 2 · ")))
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
    if (page_ == picker || picker->IsShown() || page_history_.size() != 1)
        fail(__LINE__);
    const auto* about_page = page_;
    GetEventHandler()->ProcessEvent(show_about);
    if (page_ != about_page || page_history_.size() != 1)
        fail(__LINE__);
    back();
    if (page_ != picker || !picker->IsShown() || picker->chapter_ != 22 || picker->first_ != browser_first ||
        picker->last_ != browser_last || picker->grid_->GetViewStart() != browser_scroll ||
        picker->parts().size() != 2 || !page_history_.empty())
        fail(__LINE__);
    picker->open_selection();
    const auto custom = scripture_->reading().segments();
    if (page_ || custom.size() != 2 || custom[0].book != "John" || custom[0].first != VerseRef{1, 50} ||
        custom[0].last != VerseRef{2, 2} || custom[1].first != VerseRef{22, 6} || parts_.size() != 2)
        fail(__LINE__);
    select_part(1);
    const auto custom_scroll = scripture_->scroll_position();
    about();
    back();
    if (part_ != 1 || scripture_->scroll_position() != custom_scroll || parts_.size() != 2)
        fail(__LINE__);
    // Day navigation controls rebuild safely; today remains available even on the current day.
    show_readings();
    // Both church variants open independently, while the shared Gospel has one button.
    const auto previous_tradition = settings_.tradition;
    const auto previous_calendar = settings_.calendar;
    settings_.tradition = Tradition::Greek;
    settings_.calendar = CalendarStyle::New;
    select_day(parse_date("2026-05-10").value());
    std::vector<wxButton*> reading_buttons;
    for (auto* child : home_content_->GetChildren())
        if (auto* button = wxDynamicCast(child, wxButton); button && button->GetLabel() == ui::utf8("Läs"))
            reading_buttons.push_back(button);
    const auto openable = std::ranges::count(day_.readings, true, &Reading::can_open) +
                          std::ranges::count_if(day_.variants, [](const auto& variant) {
                              return variant.reading && variant.reading->can_open();
                          });
    if (reading_buttons.size() != std::size_t(openable))
        fail(__LINE__);
    wxButton* variant_button = nullptr;
    if (!day_.variants.empty() && day_.variants[0].reading) {
        const auto variant_title = ui::utf8(
            passage_label(corpus_, corpus_.localize(in_swedish(*day_.variants[0].reading)).segments()));
        for (const auto& label : home_labels_)
            if (label.text.StartsWith(variant_title))
                if (auto* row = label.label->GetContainingSizer(); row && row->GetItemCount() == 2)
                    variant_button = wxDynamicCast(row->GetItem(std::size_t(1))->GetWindow(), wxButton);
    }
    if (variant_button) {
        wxCommandEvent click(wxEVT_BUTTON, variant_button->GetId());
        variant_button->GetEventHandler()->ProcessEvent(click);
        if (!visible_reading_ || visible_reading_->passage.last != VerseRef{11, 30} || !tracked_ ||
            tracked_->key != "day:2026-05-10:Acts 11:19-11:30")
            fail(__LINE__);
    } else
        fail(__LINE__);
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
    click_arrow(1);
    if (selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    click_arrow(0);
    if (selected_.date() != date)
        fail(__LINE__);
    for (auto* child : home_content_->GetChildren())
        if (auto* button = wxDynamicCast(child, wxButton); button && button->GetLabel() == "Idag") {
            if (!button->IsEnabled())
                fail(__LINE__);
            wxCommandEvent click(wxEVT_BUTTON, button->GetId());
            button->GetEventHandler()->ProcessEvent(click);
            break;
        }
    wxTheApp->ProcessPendingEvents();
    if (selected_.date() != local_civil_date())
        fail(__LINE__);
    select_day(date);
    // The calendar folds out on the start page; a key moves the day and Return chooses it.
    pick_date();
    if (page_ || !month_ || !month_->IsShown() || month_->focused() != date)
        fail(__LINE__);
    const auto press = [this](int code) {
        wxKeyEvent key(wxEVT_KEY_DOWN);
        key.m_keyCode = code;
        month_->GetEventHandler()->ProcessEvent(key);
        wxTheApp->ProcessPendingEvents();
    };
    press(WXK_RIGHT);
    press(WXK_RETURN);
    if (month_ || month_open_ || selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    pick_date();
    press(WXK_ESCAPE);
    if (month_ || selected_.date() != shift_date(date, 1))
        fail(__LINE__);
    open_psalm();
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
    if (!readings_->IsShown() || scripture_->IsShown() || back_->IsEnabled() ||
        readings_->GetViewStart() != home_scroll)
        fail(__LINE__);
    std::cout << "Embedded pages: previous view, scroll, selection, study, parts, date controls.\n";
    settings_.word_study = false;
    apply_settings(false);
    if (study_->IsShown())
        fail(__LINE__);
    // Exercise playback presentation without model loading or audible output.
    open_psalm();
    settings_.theme = Theme::Light;
    settings_.parallel = "el";
    apply_settings(false);
    display_playback({SpeechState::Loading, {}, 0, 0});
    // Play, pause and stop stay in place; only what applies is enabled.
    if (address_->GetLabel() != ui::utf8("Laddar rösten…") || play_->IsEnabled() || !pause_->IsEnabled() ||
        !stop_->IsEnabled() || !play_->IsShown() || !stop_->IsShown())
        fail(__LINE__);
    // Playback starts on Scripture and immediately shows the first verse.
    const SpeechCue first_verse{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}};
    read_aloud_.start({});
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
        play_->GetLabel() != ui::utf8("Fortsätt") || !play_->IsEnabled() || pause_->IsEnabled() ||
        !stop_->IsEnabled())
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
            if (!page_ || play_->IsEnabled() || pause_->IsEnabled() || stop_->IsEnabled() ||
                read_aloud_.playback().state != SpeechState::Paused)
                fail(__LINE__);
            play_or_pause();
            back();
            if (page_ || engine->changed || read_aloud_.playback().state != SpeechState::Paused ||
                read_aloud_.playback().progress != paused.progress ||
                scripture_->marker_position() != held_marker ||
                scripture_->scroll_position() != held_scroll || !scripture_->follows_playback())
                fail(__LINE__);
        }
        // New playback cues cannot replace a page while the user is in it.
        engine->held = playing;
        about();
        speech_view_.reset();
        display_playback(playing);
        if (!page_ || scripture_->IsShown() || speech_view_)
            fail(__LINE__);
        close_page();
        speech_view_ = std::pair<size_t, size_t>{0, 0};
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
    if (read_aloud_.feedback() != SpeechState::Playing)
        fail(__LINE__);
    read_aloud_.update(buffering, ReadAloud::Clock::now() + std::chrono::seconds(1));
    if (read_aloud_.feedback() != SpeechState::Buffering)
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
    settings_.font_size = 24;
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
    for (wxWindow* control : {static_cast<wxWindow*>(back_), static_cast<wxWindow*>(play_),
                              static_cast<wxWindow*>(pause_), static_cast<wxWindow*>(stop_),
                              static_cast<wxWindow*>(address_), static_cast<wxWindow*>(panes_.back().second)})
        if (control->IsShown() && control->GetRect().GetRight() > bar_->GetClientSize().x)
            fail(__LINE__);
    save(render(), "-playing-narrow");
    SetSize(original_size);
    Layout();
    root_->Layout();
    settings_.font_size = original.font_size;
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
    if (scripture_->marker_position() || play_->GetLabel() != "Lyssna" || !play_->IsEnabled() ||
        pause_->IsEnabled() || stop_->IsEnabled())
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
        read_aloud_.start({psalms, gospel});
        engine->held = {SpeechState::Paused, SpeechCue{0, 1, "Ps", "sv1917", {24, 1}, {24, 1}}, 0, 0.5};
        follow_speech();
        if (part_ != 1 || parts_.size() != 2 || !scripture_->follows_playback() ||
            speech_view_ != std::pair<size_t, size_t>{0, 1} || engine->changed)
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
        if (!page_ || visible_reading_->passage.book != "John")
            fail(__LINE__);
        back();
        if (page_ || part_ != 1 || visible_reading_->passage.book != "Ps" ||
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
        speech_ = std::move(real_speech);
        display_playback({SpeechState::Stopped, {}, 0, 0});
    }
    std::cout << "Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_psalm();
    settings_ = original;
    apply_settings(false);
    if (selected_.date() != date)
        fail(__LINE__);
    return ok;
}
} // namespace ortho
