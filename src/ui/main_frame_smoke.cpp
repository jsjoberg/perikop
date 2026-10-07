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
    bool ok = corpus_.read_only() && wxFontEnumerator::IsValidFacename("Literata") &&
              wxFontEnumerator::IsValidFacename("IBM Plex Sans") &&
              wxFontEnumerator::IsValidFacename("Noto Serif Hebrew") &&
              wxFontEnumerator::IsValidFacename("Noto Sans Math");
    // Native drawing must use the same shaped widths as paragraph fitting.
    wxClientDC metrics(scripture_);
    metrics.SetFont(body_font(19));
    for (const auto& [language, text] : std::vector<std::pair<std::string, wxString>>{
             {"sv", "I begynnelsen skapade Gud himmel och jord. Och Gud såg att det var gott. Detta är en "
                    "längre text för att kontrollera styckets jämna radbrytning och mellanrum."},
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
                ok = false;
            if (line.justified) {
                justified = true;
                if (std::abs(line.width - 280) > 0.01)
                    ok = false;
            }
            if (i + 1 == layout.lines.size() && line.justified)
                ok = false;
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
            ok = false;
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
                ok = false;
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
            ok = false;
        std::cout << "Paragraph oracle: " << verified
                  << " globally optimal fits; differs from greedy=" << beats_greedy << "\n";
        const std::vector<TextFragment> spans = {{"I begynnelsen skapade Gud himmel och jord.", "1", 0},
                                                 {"Och Gud såg att det var gott.", "2", 1}};
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
            ok = false;
    }
    if (hyphenation_points("begynnelsen", "sv").empty() || hyphenation_points("beginning", "en").empty() ||
        hyphenation_points(wxString::FromUTF8("ἀρχιερεύς"), "el").empty())
        ok = false;
    if (!hyphenation_points("project", "en").empty())
        ok = false;
    const auto optical = layout_paragraph(metrics, wxString::FromUTF8("“Guds ord.”"), 280, "sv");
    if (optical.lines.front().left_protrusion <= 0 || optical.lines.back().right_protrusion <= 0)
        ok = false;
    settings_.theme = Theme::Light;
    settings_.parallel = "el";
    apply_settings(false);
    scripture_->center_passage();
    const double start = scripture_->scroll_position();
    scripture_->scroll_by(0.375);
    if (std::abs(scripture_->scroll_position() - start - 0.375) > 0.001)
        ok = false;
    scripture_->scroll_by(-0.375);
    const auto viewport = scripture_->GetClientSize();
    wxBitmap before(viewport.x, viewport.y), after(viewport.x, viewport.y);
    {
        wxMemoryDC dc(before);
        scripture_->render_to(dc, viewport);
    }
    scripture_->scroll_by(17);
    if (std::abs(scripture_->scroll_position() - start - 17) > 0.001)
        ok = false;
    {
        wxMemoryDC dc(after);
        scripture_->render_to(dc, viewport);
    }
    const auto first = before.ConvertToImage(), second = after.ConvertToImage();
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
        ok = false;
    for (auto theme : {Theme::Light, Theme::Dark, Theme::System})
        for (auto mode : {"", "el", "en"}) {
            settings_.theme = theme;
            settings_.parallel = mode;
            apply_settings(false);
            scripture_->center_passage();
            const auto size = scripture_->GetClientSize();
            if (size.x < 1 || size.y < 1) {
                ok = false;
                continue;
            }
            wxBitmap bitmap(size.x, size.y);
            wxMemoryDC dc(bitmap);
            scripture_->render_to(dc, size);
            dc.SelectObject(wxNullBitmap);
            auto image = bitmap.ConvertToImage();
            const auto colors = palette(settings_.theme);
            std::size_t changed = 0;
            for (int y = 0; y < image.GetHeight(); ++y)
                for (int x = 0; x < image.GetWidth(); ++x)
                    if (image.GetRed(x, y) != colors.paper.Red() ||
                        image.GetGreen(x, y) != colors.paper.Green() ||
                        image.GetBlue(x, y) != colors.paper.Blue())
                        ++changed;
            if (changed < 200 || scripture_->cached_rows() > 192)
                ok = false;
            if (!screenshot_path.empty() && std::string(mode) == "el" &&
                (theme == Theme::Light || theme == Theme::Dark)) {
                const wxSize page_size(size.x, 1100);
                wxBitmap page(page_size.x, page_size.y);
                wxMemoryDC page_dc(page);
                scripture_->render_to(page_dc, page_size);
                page_dc.SelectObject(wxNullBitmap);
                const wxString output =
                    theme == Theme::Light ? screenshot_path : screenshot_path.BeforeLast('.') + "-dark.png";
                ok = page.ConvertToImage().SaveFile(output, wxBITMAP_TYPE_PNG) && ok;
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
        ok = false;
    else {
        wxBitmap bitmap(narrow_size.x, narrow_size.y);
        wxMemoryDC dc(bitmap);
        scripture_->render_to(dc, narrow_size);
        dc.SelectObject(wxNullBitmap);
        if (!screenshot_path.empty())
            ok = bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.') + "-narrow.png",
                                                  wxBITMAP_TYPE_PNG) &&
                 ok;
    }
    SetSize(original_size);
    Layout();
    // The day page keeps the toolbar, with the reader's controls dimmed.
    show_readings();
    for (auto* child : readings_->GetChildren())
        if (auto* control = dynamic_cast<wxButton*>(child); control && control->GetLabel().Contains("Lyssna"))
            ok = false;
    if (!bar_->IsShown() || play_->IsEnabled() || back_->IsEnabled() || panes_.front().second->IsEnabled() ||
        address_->GetLabel() != ui::utf8(date_swedish(selected_.date())))
        ok = false;
    // Right-column symbols toggle, and the Visa menu follows them.
    toggle_pane("el");
    if (settings_.parallel != "el" || settings_.word_study)
        ok = false;
    toggle_pane("study");
    if (!settings_.parallel.empty() || !settings_.word_study || !GetMenuBar()->IsChecked(study_item_))
        ok = false;
    toggle_pane("study");
    if (settings_.word_study || !GetMenuBar()->IsChecked(right_item_))
        ok = false;
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
            ok = false;
        const auto size = scripture_->GetClientSize();
        wxBitmap bitmap(size.x, size.y);
        wxMemoryDC dc(bitmap);
        scripture_->render_to(dc, size);
        dc.SelectObject(wxNullBitmap);
        if (!screenshot_path.empty())
            ok = bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.') + "-" +
                                                      ui::utf8(reading.passage.book) + ".png",
                                                  wxBITMAP_TYPE_PNG) &&
                 ok;
    }
    open_reading({ReadingKind::Epistle, {"1Cor", {4, 9}, {4, 16}}, "Första Korintierbrevet 4:9–16"});
    if (!screenshot_path.empty()) {
        const auto size = scripture_->GetClientSize();
        wxBitmap bitmap(size.x, size.y);
        wxMemoryDC dc(bitmap);
        scripture_->render_to(dc, size);
        dc.SelectObject(wxNullBitmap);
        ok = bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.') + "-prose.png",
                                              wxBITMAP_TYPE_PNG) &&
             ok;
    }
    // Marked verses replace the reading as what Lyssna reads.
    open_psalm();
    scripture_->select_verses({23, 4}, {23, 2});
    const auto marked = scripture_->selection();
    if (!marked || marked->first != VerseRef{23, 2} || marked->last != VerseRef{23, 4} ||
        play_->GetLabel() != "Läs markering")
        ok = false;
    scripture_->clear_selection();
    if (play_->GetLabel() != "Lyssna")
        ok = false;
    // The left pane's language chooses the edition and its numbering: LXX Psalm 22 is Psalm 23.
    settings_.primary = "el";
    apply_settings(false);
    open_psalm();
    if (scripture_->base_source() != "grc-lxx" || scripture_->reading().passage.first.chapter != 22)
        ok = false;
    settings_.primary = "sv";
    apply_settings(false);
    // Ordstudium replaces the right pane; a word shows Dalin and the verse's Strong's entries.
    settings_.word_study = true;
    apply_settings(false);
    open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 5}}, "Johannesevangeliet 1:1–5"});
    if (!study_->IsShown() || scripture_->base_source() != "sv1917")
        ok = false;
    study_->show({"John", "begynnelsen", {1, 1}, 0}, scripture_->base_source(), scripture_->frame());
    {
        const auto lines = study_->text();
        const auto has = [&](const wxString& part) {
            return std::any_of(lines.begin(), lines.end(), [&](const wxString& line) {
                return line.Contains(part);
            });
        };
        if (!has("DALIN") || !has("begynnelse") || !has("G746"))
            ok = false;
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
            ok = false;
        // Long articles begin with a bounded preview; the complete text stays in the database.
        if (std::any_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.length() > 520;
            }))
            ok = false;
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
            ok = false;
    }
    study_->show({"John", "Kristi", {1, 17}, 0}, scripture_->base_source(), scripture_->frame());
    {
        const auto lines = study_->text();
        if (std::none_of(lines.begin(), lines.end(), [](const wxString& line) {
                return line.Contains("Messias");
            }))
            ok = false;
    }
    settings_.word_study = false;
    apply_settings(false);
    if (study_->IsShown())
        ok = false;
    // Exercise playback presentation without model loading or audible output.
    open_psalm();
    settings_.theme = Theme::Light;
    settings_.parallel = "el";
    apply_settings(false);
    display_playback({SpeechState::Loading, {}, 0, 0});
    if (address_->GetLabel() != "Laddar rösten…" || play_->GetLabel() != "Pausa" || !stop_->IsShown())
        ok = false;
    // During the introduction the address field shows what the voice says.
    speech_introductions_ = {ui::utf8("Läsning ur Psaltaren, kapitel 23, vers 1 till 6.")};
    display_playback({SpeechState::Playing, SpeechCue{0, 0, "Ps", "", {23, 1}, {23, 6}, true}, 0, 0.01});
    if (address_->GetLabel() != speech_introductions_.front())
        ok = false;
    display_playback({SpeechState::Buffering, {}, 0, 0, 0.4});
    if (address_->GetLabel() != "Förbereder uppläsningen… 40 %" || scripture_->marker_position())
        ok = false;
    SpeechPlayback playing{SpeechState::Playing, SpeechCue{0, 0, "Ps", "sv1917", {23, 3}, {23, 3}, false},
                           0.35, 0.4};
    following_audio_ = true;
    scripture_->playback(playing);
    scripture_->follow_playback();
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position() || !scripture_->follows_playback())
        ok = false;
    const auto held_marker = scripture_->marker_position();
    const auto held_scroll = scripture_->scroll_position();
    auto paused = playing;
    paused.state = SpeechState::Paused;
    display_playback(paused);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        play_->GetLabel() != "Fortsätt" || !stop_->IsShown())
        ok = false;
    auto buffering = playing;
    buffering.state = SpeechState::Buffering;
    display_playback(buffering);
    for (int i = 0; i < 30; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() != held_marker || scripture_->scroll_position() != held_scroll ||
        address_->GetLabel() != "Förbereder fortsättningen…")
        ok = false;
    display_playback(playing);
    display_playback(buffering);
    if (feedback_state_ != SpeechState::Playing)
        ok = false;
    buffering_since_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    display_playback(buffering);
    if (feedback_state_ != SpeechState::Buffering)
        ok = false;
    scripture_->scroll_by(20);
    if (scripture_->follows_playback() || following_audio_)
        ok = false;
    following_audio_ = true;
    display_playback(playing);
    scripture_->follow_playback();
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    const auto save_playback = [&](const wxString& suffix) {
        if (screenshot_path.empty())
            return;
        const auto size = scripture_->GetClientSize();
        wxBitmap bitmap(size.x, size.y);
        wxMemoryDC dc(bitmap);
        scripture_->render_to(dc, size);
        dc.SelectObject(wxNullBitmap);
        ok = bitmap.ConvertToImage().SaveFile(screenshot_path.BeforeLast('.') + suffix + ".png",
                                              wxBITMAP_TYPE_PNG) &&
             ok;
    };
    save_playback("-playing");
    settings_.theme = Theme::Dark;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    save_playback("-playing-dark");
    // Reflow keeps the spoken verse, including the other language column.
    settings_.font_size = 24;
    apply_settings(false);
    scripture_->playback(playing);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position())
        ok = false;
    auto greek = playing;
    greek.cue->source = "grc-lxx";
    greek.cue->verse = greek.cue->last = {22, 3};
    scripture_->playback(greek);
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (!scripture_->marker_position())
        ok = false;
    save_playback("-playing-greek");
    SetSize(FromDIP(wxSize(520, 650)));
    Layout();
    root_->Layout();
    display_playback(paused);
    for (wxWindow* control :
         {static_cast<wxWindow*>(back_), static_cast<wxWindow*>(play_), static_cast<wxWindow*>(stop_),
          static_cast<wxWindow*>(address_), static_cast<wxWindow*>(panes_.back().second)})
        if (control->IsShown() && control->GetRect().GetRight() > bar_->GetClientSize().x)
            ok = false;
    save_playback("-playing-narrow");
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
        ok = false;
    display_playback({SpeechState::Stopped, {}, 0, 0});
    for (int i = 0; i < 90; ++i)
        scripture_->advance_playback(0.016);
    if (scripture_->marker_position() || play_->GetLabel() != "Lyssna" || stop_->IsShown())
        ok = false;
    std::cout << "Playback presentation: marker, pause, buffering, manual scroll, follow, stop.\n";
    open_psalm();
    settings_ = original;
    apply_settings(false);
    ok = ok && selected_.date() == date;
    return ok;
}
} // namespace ortho
