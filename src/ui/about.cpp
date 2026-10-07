#include "ui/about.hpp"
#include "perikop_version.hpp"
#include "ui/controls.hpp"
#include <wx/dialog.h>
#include <wx/hyperlink.h>
#include <wx/image.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/utils.h>
namespace ortho {
namespace {
struct Link {
    Link(const char* address, const char* text = nullptr) : url(address), label(text) {}
    const char* url;
    // Shown instead of the address, for addresses that say nothing to a reader.
    const char* label;
};
struct Credit {
    const char* name;
    const char* detail;
    std::vector<Link> links;
};
struct Section {
    const char* title;
    std::vector<Credit> credits;
};
// THIRD-PARTY-NOTICES.md is the complete record; this is its reader-facing summary.
const std::vector<Section>& sections() {
    static const std::vector<Section> all = {
        {"Bibeltexter",
         {{"Svenska Bibeln 1917", "Projekt Runeberg. Allmän egendom.", {"https://runeberg.org/bibeln/"}},
          {"Apokryferna 1921",
           "Projekt Runeberg och Scrollmapper Bible Databases (MIT).",
           {"https://runeberg.org/apokryf/",
            "https://github.com/scrollmapper/bible_databases/tree/master/sources/sv/Swe1917"}},
          {"Septuaginta, Brentons grekiska text",
           "eBible.org. Allmän egendom enligt utgivaren.",
           {"https://ebible.org/grcbrent/copyright.htm"}},
          {"Grekiska Nya testamentet, Patriarkatets text 1904",
           "eBible.org. Allmän egendom enligt utgivaren.",
           {"https://ebible.org/grcbyz/copyright.htm"}},
          {"King James Version",
           "Scrollmapper Bible Databases. Allmän egendom i många länder.",
           {"https://github.com/scrollmapper/bible_databases/tree/master/sources/en/KJV"}},
          {"World English Bible",
           "eBible.org. Allmän egendom; namnet är ett varumärke.",
           {"https://ebible.org/bible/details.php?id=eng-web"}}}},
        {"Läsordning",
         {{"Orthocal",
           "Brian Glass. Perikoptabeller och kalenderregler. MIT.",
           {"https://github.com/brianglass/orthocal-python"}},
          {"Antiokiska ärkestiftet i Nordamerika",
           "Bibelhänvisningarna i den officiella läsordningen för 2026.",
           {{"https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/"
             "Liturgical%20Chart%20for%202026%20English.pdf",
             "Liturgical Chart for 2026 (PDF)"}}}}},
        {"Ordstudium",
         {{"Dalins ordbok",
           "Språkbanken Text, Göteborgs universitet. CC BY 4.0.",
           {"https://spraakbanken.gu.se/resurser/dalin"}},
          {"Erik Nyström, Biblisk ordbok för hemmet och skolan (1896)",
           "Digitaliserad av Projekt Runeberg. Allmän egendom.",
           {"https://runeberg.org/biblobok/"}},
          {"TBESG, Strongs grekiska lexikon",
           "STEP Bible, Tyndale House Cambridge. CC BY 4.0.",
           {"https://github.com/STEPBible/STEPBible-Data"}},
          {"NST uttalslexikon för svenska",
           "Nasjonalbiblioteket, Språkbanken. CC0 1.0.",
           {"https://www.nb.no/sprakbanken/"}}}},
        {"Uppläsning",
         {{"Kokoro, svenska rösterna Alice och Björn",
           "Svenska Kokoro-vikter, röster och uttalsmodell från kokoro-sv. Apache 2.0.",
           {"https://huggingface.co/Joakim/kokoro-sv-voices", "https://github.com/joakimeriksson/kokoro-sv"}},
          {"ONNX Runtime", "Microsoft. MIT.", {"https://github.com/microsoft/onnxruntime"}},
          {"miniaudio", "David Reid. Allmän egendom.", {"https://github.com/mackron/miniaudio"}},
          {"Sonic", "Bill Cox. Apache 2.0.", {"https://github.com/waywardgeek/sonic"}}}},
        {"Typografi",
         {{"Literata", "TypeTogether för Google. SIL OFL 1.1.", {"https://github.com/googlefonts/literata"}},
          {"IBM Plex Sans", "IBM och Bold Monday. SIL OFL 1.1.", {"https://github.com/IBM/plex"}},
          {"Noto Serif Hebrew och Noto Sans Math",
           "Google. SIL OFL 1.1.",
           {"https://github.com/notofonts/noto-fonts"}},
          {"Avstavningsmönster",
           "Jan Michael Rynning (svenska, LPPL), Dimitrios Filippou (grekiska, MIT) och "
           "Gerard D.C. Kuiken (engelska).",
           {"https://github.com/hyphenation/tex-hyphen"}},
          {"Justif", "Förebild för styckesättningen.", {"https://github.com/lyallcooper/justif"}}}},
        {"Programbibliotek",
         {{"wxWidgets", "wxWindows Library Licence.", {"https://www.wxwidgets.org"}},
          {"SQLite", "Allmän egendom.", {"https://sqlite.org"}},
          {"libpng, zlib, PCRE2 och NanoSVG",
           "Ingår i wxWidgets, med egna fria licenser.",
           {"http://www.libpng.org", "https://zlib.net", "https://github.com/PCRE2Project/pcre2",
            "https://github.com/memononen/nanosvg"}},
          {"libjpeg", "Programmet bygger delvis på arbete av Independent JPEG Group.", {"https://ijg.org"}}}},
    };
    return all;
}
wxString path_text(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return wxString::FromUTF8(reinterpret_cast<const char*>(text.c_str()));
}
wxString link_label(wxString url) {
    for (const auto* scheme : {"https://", "http://"})
        if (url.StartsWith(scheme, &url))
            break;
    if (url.EndsWith("/"))
        url.RemoveLast();
    // Long links read better without their query or deep path.
    return url.length() > 56 ? url.Left(55) + ui::utf8("…") : url;
}
} // namespace

void show_about(wxWindow* parent, Theme theme, const std::filesystem::path& resources) {
    wxDialog dialog(parent, wxID_ANY, "Om Perikop", wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    const auto colors = palette(theme);
    auto* page =
        new wxScrolledWindow(&dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetScrollRate(0, dialog.FromDIP(12));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const int pad = dialog.FromDIP(32), width = dialog.FromDIP(480);
    std::vector<wxWindow*> muted;
    std::vector<wxHyperlinkCtrl*> links;
    const auto text = [&](const wxString& value, const wxFont& font, int below, bool quiet = false,
                          int flags = 0) {
        auto* label = new wxStaticText(page, wxID_ANY, value);
        label->SetFont(font);
        label->Wrap(width);
        sizer->Add(label, 0, wxLEFT | wxRIGHT | flags, pad);
        sizer->AddSpacer(dialog.FromDIP(below));
        if (quiet)
            muted.push_back(label);
    };
    sizer->AddSpacer(dialog.FromDIP(28));
    wxImage icon;
    if (icon.LoadFile(path_text(resources / "icons/orthodox-cross.png"), wxBITMAP_TYPE_PNG)) {
        const int side = dialog.FromDIP(72);
        auto* image =
            new wxStaticBitmap(page, wxID_ANY, wxBitmap(icon.Rescale(side, side, wxIMAGE_QUALITY_HIGH)));
        sizer->Add(image, 0, wxALIGN_CENTER_HORIZONTAL);
        sizer->AddSpacer(dialog.FromDIP(12));
    }
    text("Perikop", body_font(28), 2, false, wxALIGN_CENTER_HORIZONTAL);
    text("Version " PERIKOP_VERSION, ui_font(10), 18, true, wxALIGN_CENTER_HORIZONTAL);
    text(ui::utf8("Dagens bibelläsningar enligt den antiokiska ortodoxa kyrkans läsordning i Nordamerika, "
                  "med Svenska Bibeln 1917, grekisk och engelsk parallelltext, ordstudium och svensk "
                  "uppläsning. Allt fungerar utan nätverksanslutning."),
         ui_font(11), 8);
    text(ui::utf8("Perikop är fri programvara under MIT-licensen. Programmet bygger på andras generösa "
                  "arbete; tack till alla nedan. Varje källa har sina egna villkor."),
         ui_font(11), 4, true);
    for (const auto& section : sections()) {
        sizer->AddSpacer(dialog.FromDIP(18));
        text(ui::utf8(section.title).Upper(), ui_font(9), 6, true);
        for (const auto& credit : section.credits) {
            auto name = ui_font(11);
            name.SetWeight(wxFONTWEIGHT_MEDIUM);
            text(ui::utf8(credit.name), name, 1);
            text(ui::utf8(credit.detail), ui_font(10), 2, true);
            for (const auto& [url, label] : credit.links) {
                auto* link =
                    new wxHyperlinkCtrl(page, wxID_ANY, label ? ui::utf8(label) : link_label(url), url);
                link->SetFont(ui_font(10));
                link->SetToolTip(url);
                sizer->Add(link, 0, wxLEFT | wxRIGHT, pad);
                links.push_back(link);
            }
            sizer->AddSpacer(dialog.FromDIP(10));
        }
    }
    sizer->AddSpacer(dialog.FromDIP(8));
    text(ui::utf8("De fullständiga licenstexterna följer med programmet."), ui_font(10), 24, true);
    page->SetSizer(sizer);
    page->FitInside();
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(page, 1, wxEXPAND);
    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    const auto licenses = path_text(resources / "licenses");
    buttons->Add(ui::button(&dialog, "Visa licenstexter",
                            [licenses] {
                                wxLaunchDefaultApplication(licenses);
                            }),
                 0, wxALIGN_CENTER_VERTICAL);
    buttons->AddStretchSpacer();
    auto* close = ui::button(&dialog, ui::utf8("Stäng"), [&dialog] {
        dialog.EndModal(wxID_OK);
    });
    close->SetDefault();
    buttons->Add(close, 0, wxALIGN_CENTER_VERTICAL);
    outer->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(14));
    dialog.SetSizer(outer);
    dialog.SetSize(dialog.FromDIP(wxSize(480 + 2 * 32 + 24, 640)));
    dialog.SetMinSize(dialog.FromDIP(wxSize(420, 360)));
    ui::recolor(&dialog, colors);
    for (auto* label : muted)
        label->SetForegroundColour(colors.muted);
    for (auto* link : links) {
        link->SetNormalColour(colors.accent);
        link->SetVisitedColour(colors.accent);
        link->SetHoverColour(colors.ink);
    }
    dialog.SetEscapeId(wxID_OK);
    dialog.CenterOnParent();
    dialog.ShowModal();
}
} // namespace ortho
