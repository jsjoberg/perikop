#include "speech/portable_speech.hpp"
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/app_icon.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/native_icon.hpp"
#include <clocale>
#include <filesystem>
#include <iostream>
#include <wx/app.h>
#include <wx/fontenum.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/timer.h>
#include <wx/uilocale.h>
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#endif
#ifdef __WXGTK__
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>
#endif
class ReaderApp final : public wxApp {
public:
    bool OnInit() override {
        // Swedish month and weekday names in date controls. This sets only
        // wxWidgets' UI locale; the C locale used for numbers is unchanged.
        wxUILocale::UseLocaleName("sv_SE");
#ifdef __WXGTK__
        // wxGTK leaves the C locale, in which character classes such as wxIsalpha know only ASCII.
        // Hyphenation needs Swedish and Greek letters; numbers keep the C format.
        std::setlocale(LC_CTYPE, "C.UTF-8");
#endif
        // The data directory keeps its pre-Perikop name so settings, voices and caches remain.
        SetAppName("orthodox-reader");
        SetVendorName("orthodox-reader");
        SetAppDisplayName("Perikop");
        wxInitAllImageHandlers();
        bool smoke = false, benchmark = false, reader = false, pronunciation_review = false;
        wxString resource_override, screenshot, speech_probe;
        auto date = ortho::local_civil_date();
        for (int i = 1; i < argc; ++i) {
            const wxString arg = argv[i];
            if (arg == "--smoke-test")
                smoke = true;
            else if (arg == "--render-benchmark")
                benchmark = true;
            else if (arg == "--reader")
                reader = true;
            else if (arg == "--pronunciation-review")
                pronunciation_review = true;
            else if ((arg == "--resources" || arg == "--date" || arg == "--screenshot" ||
                      arg == "--speech-probe") &&
                     i + 1 < argc) {
                const wxString value = argv[++i];
                if (arg == "--resources")
                    resource_override = value;
                if (arg == "--screenshot")
                    screenshot = value;
                if (arg == "--speech-probe")
                    speech_probe = value;
                if (arg == "--date") {
                    auto parsed = ortho::parse_date(value.ToStdString());
                    if (!parsed) {
                        std::cerr << parsed.error() << '\n';
                        return false;
                    }
                    date = *parsed;
                }
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg.ToStdString() << '\n';
                return false;
            }
        }
        try {
            std::vector<std::filesystem::path> candidates;
            if (!resource_override.empty())
                candidates.push_back(ortho::ui::filesystem_path(resource_override));
            else {
                const auto executable =
                    ortho::ui::filesystem_path(wxStandardPaths::Get().GetExecutablePath()).parent_path();
#ifdef __APPLE__
                candidates.push_back(executable.parent_path() / "Resources");
#endif
                candidates.push_back(executable / "resources");
                candidates.push_back(executable.parent_path() / "share/perikop/resources");
                candidates.emplace_back(ORTHO_RESOURCE_INSTALL_PATH);
            }
            std::filesystem::path resources;
            for (const auto& candidate : candidates)
                if (std::filesystem::exists(candidate / "corpus/corpus.db")) {
                    resources = candidate;
                    break;
                }
            if (resources.empty())
                throw std::runtime_error("Bundled resources missing. Rebuild or use --resources PATH.");
            if (!speech_probe.empty()) {
                smoke_exit_ =
                    ortho::render_speech_probe(resources / "voices", ortho::ui::filesystem_path(speech_probe))
                        ? 0
                        : 1;
                return true;
            }
            for (const auto* file : {"Literata-Regular.ttf", "Literata-Italic.ttf", "IBMPlexSans-Regular.ttf",
                                     "IBMPlexSans-Medium.ttf", "NotoSerifHebrew-Regular.ttf"}) {
                const auto file_path = (resources / "fonts" / file).u8string();
#ifdef __APPLE__
                const auto* bytes = reinterpret_cast<const UInt8*>(file_path.c_str());
                CFURLRef url =
                    CFURLCreateFromFileSystemRepresentation(nullptr, bytes, file_path.size(), false);
                CFErrorRef error = nullptr;
                const bool loaded =
                    url && CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, &error);
                if (url)
                    CFRelease(url);
                if (error)
                    CFRelease(error);
#else
                const bool loaded = wxFont::AddPrivateFont(
                    wxString::FromUTF8(reinterpret_cast<const char*>(file_path.c_str())));
#endif
                if (!loaded)
                    throw std::runtime_error(std::string("Cannot load bundled font: ") + file);
            }
#ifdef __WXGTK__
            // wxGTK gives Pango the same font configuration for every private font, so Pango keeps
            // the families it listed after the first one. Make it list them again.
            pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(pango_cairo_font_map_get_default()));
            wxFontEnumerator::InvalidateCache();
#endif
            ortho::load_hyphenation(resources / "hyphenation");
            corpus_ = std::make_unique<ortho::CorpusDb>(resources / "corpus/corpus.db");
            if (smoke || benchmark) {
                test_path_ = std::filesystem::temp_directory_path() /
                             std::filesystem::path(
                                 "perikop-smoke-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                user_ = std::make_unique<ortho::UserDb>(test_path_ / "user.db");
            } else
                user_ = std::make_unique<ortho::UserDb>(
                    ortho::ui::filesystem_path(wxStandardPaths::Get().GetUserLocalDataDir()) / "user.db");
#if wxCHECK_VERSION(3, 3, 0)
            SetAppearance(static_cast<wxApp::Appearance>(user_->load().theme));
#endif
            auto* frame = new ortho::MainFrame(*corpus_, *user_, date, resources);
            wxIcon icon;
            icon.CopyFromBitmap(wxBitmap(ortho::app_icon_image()));
            if (icon.IsOk())
                frame->SetIcon(icon);
            ortho::set_native_app_icon(ortho::app_icon_png());
            SetTopWindow(frame);
            frame->Show();
            if (reader)
                frame->open_psalm();
            if (pronunciation_review)
                frame->CallAfter([frame] {
                    frame->review_pronunciation();
                });
            if (smoke || benchmark) {
                timer_ = std::make_unique<wxTimer>(this);
                Bind(wxEVT_TIMER, [this, frame, screenshot, benchmark](wxTimerEvent&) {
                    try {
                        smoke_exit_ =
                            (benchmark ? frame->render_benchmark() : frame->smoke_test(screenshot)) ? 0 : 1;
                    } catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                        smoke_exit_ = 1;
                    }
                    if (!benchmark)
                        std::cout << (smoke_exit_ == 0 ? "UI smoke passed: fonts, rendering, themes, "
                                                         "parallel modes, stable date.\n"
                                                       : "UI smoke failed.\n");
                    frame->Close(true);
                });
                timer_->StartOnce(300);
            }
            return true;
        } catch (const std::exception& e) {
            if (smoke || benchmark)
                std::cerr << e.what() << '\n';
            else
                wxMessageBox(wxString::FromUTF8(e.what()), "Perikop", wxOK | wxICON_ERROR);
            return false;
        }
    }
    int OnRun() override {
        if (smoke_exit_ >= 0 && !GetTopWindow())
            return smoke_exit_;
        const int status = wxApp::OnRun();
        return smoke_exit_ >= 0 ? smoke_exit_ : status;
    }
    int OnExit() override {
        timer_.reset();
        user_.reset();
        corpus_.reset();
        if (!test_path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(test_path_, error);
        }
        return wxApp::OnExit();
    }

private:
    std::unique_ptr<ortho::CorpusDb> corpus_;
    std::unique_ptr<ortho::UserDb> user_;
    std::unique_ptr<wxTimer> timer_;
    std::filesystem::path test_path_;
    int smoke_exit_ = -1;
};
wxIMPLEMENT_APP(ReaderApp); // NOLINT(bugprone-throwing-static-initialization): wxWidgets entry point
