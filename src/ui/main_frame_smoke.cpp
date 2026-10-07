#include "core/reading_plan.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <wx/app.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/fontenum.h>
#include <wx/graphics.h>
#include <wx/menu.h>
namespace ortho {
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
    if (!corpus_.read_only() || !wxFontEnumerator::IsValidFacename("Literata") ||
        !wxFontEnumerator::IsValidFacename("IBM Plex Sans") ||
        !wxFontEnumerator::IsValidFacename("Noto Serif Hebrew") ||
        !wxFontEnumerator::IsValidFacename("Noto Sans Math"))
        fail(__LINE__);
    // The reader as drawn in a size, and a copy saved beside the screenshot.
    const auto render = [this](wxSize size = {}) {
        if (size == wxSize{})
            size = scripture_->GetClientSize();
        wxBitmap bitmap(size.x, size.y);
        {
            wxMemoryDC dc(bitmap);
            scripture_->render_to(dc, size);
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
        gc->SetFont(metrics.GetFont(), *wxBLACK);
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
    if (hyphenation_points("begynnelsen", "sv").empty() || hyphenation_points("beginning", "en").empty() ||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"), "el").empty())
        fail(__LINE__);
    if (!hyphenation_points("project", "en").empty())
        fail(__LINE__);
    const auto optical = layout_paragraph(metrics, wxString::FromUTF8("“Guds ord.”"), 280, "sv");
    if (optical.lines.front().left_protrusion <= 0 || optical.lines.back().right_protrusion <= 0)
        fail(__LINE__);
    settings_.theme = Theme::Light;
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
    for (auto* child : readings_->GetChildren())
        if (auto* control = dynamic_cast<wxButton*>(child); control && control->GetLabel().Contains("Lyssna"))
            fail(__LINE__);
    if (!bar_->IsShown() || play_->IsEnabled() || pause_->IsEnabled() || stop_->IsEnabled() ||
        back_->IsEnabled() || panes_.front().second->IsEnabled() ||
        address_->GetLabel() != ui::utf8("Gå till bibelställe…"))
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
    // Right-column symbols toggle, and the Visa menu follows them.
    toggle_pane("el");
    if (settings_.parallel != "el" || settings_.word_study)
        fail(__LINE__);
    toggle_pane("study");
    if (!settings_.parallel.empty() || !settings_.word_study || !GetMenuBar()->IsChecked(study_item_))
        fail(__LINE__);
    toggle_pane("study");
    if (settings_.word_study || !GetMenuBar()->IsChecked(right_item_))
        fail(__LINE__);
    settings_.parallel = "";
    apply_settings(false);
    for (auto reading : std::vector<Reading>{
             {ReadingKind::OldTestament,
              {"Gen", {31, 50, "a"}, {31, 50, "a"}},
              "Första Moseboken 31:50a",
              {},
              "el"},
             {ReadingKind::OldTestament, {"4Macc", {8, 29}, {8, 29}}, "Fjärde Mackabeerboken 8:29", {}, "en"},
             {ReadingKind::OldTestament, {"2Esd", {1, 1}, {1, 1}}, "Andra Esdrasboken 1:1"}}) {
        open_reading(reading);
        scripture_->center_passage();
        if (scripture_->cached_rows() == 0)
            fail(__LINE__);
        save(render(), "-" + ui::utf8(reading.passage.book));
    }
    open_reading({ReadingKind::Epistle, {"1Cor", {4, 9}, {4, 16}}, "Första Korintierbrevet 4:9–16"});
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
    const auto marked = scripture_->selection();
    if (!marked || marked->first != VerseRef{23, 2} || marked->last != VerseRef{23, 4} ||
        play_->GetLabel() != ui::utf8("Läs markering"))
        fail(__LINE__);
    scripture_->clear_selection();
    if (play_->GetLabel() != "Lyssna")
        fail(__LINE__);
    // The left pane's language chooses the edition and its numbering: LXX Psalm 22 is Psalm 23.
    settings_.primary = "el";
    apply_settings(false);
    open_psalm();
    if (scripture_->base_source() != "grc-lxx" || scripture_->reading().passage.first.chapter != 22)
        fail(__LINE__);
    settings_.primary = "sv";
    apply_settings(false);
    // Ordstudium replaces the right pane; a word shows Dalin and the verse's Strong's entries.
    settings_.word_study = true;
    apply_settings(false);
    open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}, "Johannesevangeliet 1:1–5"});
    if (!study_->IsShown() || scripture_->base_source() != "sv1917")
        fail(__LINE__);
    study_->show({"John", "begynnelsen", {1, 1}, 0}, scripture_->base_source(), scripture_->frame());
    {
        const auto lines = study_->text();
        const auto has = [&](const wxString& part) {
            return std::any_of(lines.begin(), lines.end(), [&](const wxString& line) {
                return line.Contains(part);
            });
        };
        if (!has("DALIN") || !has("begynnelse") || !has("G746"))
            fail(__LINE__);
    }
    study_->show({"John", "Jesu", {1, 17}, 0}, scripture_->base_source(), scripture_->frame());
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
    study_->show({"John", "Kristi", {1, 17}, 0}, scripture_->base_source(), scripture_->frame());
    {
        const auto lines = study_->text();
        if (std::none_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.Contains("Messias");
            }))
            fail(__LINE__);
    }
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
    // During the introduction the address field shows what the voice says.
    const std::string introduction = "Läsning ur Psaltaren, kapitel 23, vers 1 till 6.";
    const SpeechCue introduction_cue{0, 0, "Ps", "", {23, 1}, {23, 6}, true};
    read_aloud_.start({}, {{introduction, introduction, "sv", introduction_cue}});
    display_playback({SpeechState::Playing, introduction_cue, 0, 0.01});
    if (address_->GetLabel() != ui::utf8(introduction))
        fail(__LINE__);
    display_playback({SpeechState::Buffering, {}, 0, 0, 0.4});
    if (address_->GetLabel() != ui::utf8("Förbereder uppläsningen… 40 %") || scripture_->marker_position())
        fail(__LINE__);
    SpeechPlayback playing{SpeechState::Playing, SpeechCue{0, 0, "Ps", "sv1917", {23, 3}, {23, 3}, false},
                           0.35, 0.4};
    following_audio_ = true;
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
    display_playback(paused);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        play_->GetLabel() != ui::utf8("Fortsätt") || !play_->IsEnabled() || pause_->IsEnabled() ||
        !stop_->IsEnabled())
        fail(__LINE__);
    auto buffering = playing;
    buffering.state = SpeechState::Buffering;
    display_playback(buffering);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        address_->GetLabel() != ui::utf8("Förbereder fortsättningen…"))
        fail(__LINE__);
    display_playback(playing);
    display_playback(buffering);
    if (read_aloud_.feedback() != SpeechState::Playing)
        fail(__LINE__);
    read_aloud_.update(buffering, ReadAloud::Clock::now() + std::chrono::seconds(1));
    if (read_aloud_.feedback() != SpeechState::Buffering)
        fail(__LINE__);
    scripture_->scroll_by(20);
    if (scripture_->follows_playback() || following_audio_)
        fail(__LINE__);
    following_audio_ = true;
    display_playback(playing);
    scripture_->follow_playback();
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
        std::equal(plain.GetData(), plain.GetData() + plain.GetWidth() * plain.GetHeight() * 3,
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
    greek.cue->source = "grc-lxx";
    greek.cue->verse = greek.cue->last = {22, 3};
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
    std::cout << "Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_psalm();
    settings_ = original;
    apply_settings(false);
    if (selected_.date() != date)
        fail(__LINE__);
    return ok;
}
} // namespace ortho
