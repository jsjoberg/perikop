#include "storage/content_import.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <wx/filedlg.h>
#include <wx/hyperlink.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>

namespace ortho {
void MainFrame::add_content_loader(wxWindow* content, wxBoxSizer* entries) {
    entries->Add(ui::label(content, ui::utf8("LOKALT BIBELINNEHÅLL"), 10), 0, wxBOTTOM, FromDIP(zoomed(8)));
    const auto paragraph = [&](const char* text) {
        auto* label = ui::label(content, ui::utf8(text));
        label->Wrap(FromDIP(zoomed(440)));
        entries->Add(label, 0, wxBOTTOM, FromDIP(zoomed(8)));
        return label;
    };
    paragraph("Reformationsbibeln 2026 kan läggas till från två PDF-filer som du själv hämtar från "
              "Svenska Reformationsbibelsällskapet. Texten sparas bara i din profil.");
    auto* page =
        new wxHyperlinkCtrl(content, wxID_ANY, ui::utf8("Öppna Nerladdningsbara bibelböcker på bibel.se"),
                            "https://bibel.se/wordpress/index.php/2022/01/18/oversatta-bibelbocker/");
    page->SetFont(ui_font());
    entries->Add(page, 0, wxBOTTOM, FromDIP(zoomed(8)));
    paragraph("Hämta där SRB Hela GT och SRB NT med UN noter och importera dem sedan här. "
              "Avsnittsrubriker visas separat från verserna. Fotnoter och korshänvisningar importeras inte.");
    auto* button = ui::button(content, ui::utf8("Importera båda PDF-filerna…"), [this] {
        CallAfter([this] {
            import_content();
        });
    });
    button->SetToolTip(ui::utf8("Välj först Gamla och sedan Nya testamentets PDF från bibel.se"));
    entries->Add(button, 0, wxBOTTOM, FromDIP(zoomed(8)));
    auto* remove = ui::button(content, ui::utf8("Ta bort importerad text…"), [this] {
        CallAfter([this] {
            remove_content();
        });
    });
    remove->Enable(corpus_.has_imported());
    entries->Add(remove, 0, wxBOTTOM, FromDIP(zoomed(8)));
    paragraph(corpus_.has_imported()
                  ? "Reformationsbibeln 2026 är importerad. Aktivera den under Böcker och textkällor "
                    "och välj dess prioritet."
                  : "Reformationsbibeln 2026 är inte importerad.");
    if (corpus_.has_imported())
        paragraph(corpus_.import_is_current()
                      ? "Din importerade text är uppdaterad. Du behöver inte importera PDF-filerna igen."
                      : "En förbättrad import är tillgänglig. Du kan importera samma två PDF-filer igen "
                        "när det passar dig.")
            ->SetName("Importstatus");
    entries->AddSpacer(FromDIP(zoomed(22)));
}
void MainFrame::import_content() {
    wxFileDialog old_testament(this, ui::utf8("Välj SRBhelaGT.pdf – Gamla testamentet"), {}, "SRBhelaGT.pdf",
                               "PDF (*.pdf)|*.pdf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (old_testament.ShowModal() != wxID_OK)
        return;
    wxFileDialog new_testament(this, ui::utf8("Välj SRBNT-UNnoter.pdf – Nya testamentet"), {},
                               "SRBNT-UNnoter.pdf", "PDF (*.pdf)|*.pdf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (new_testament.ShowModal() != wxID_OK)
        return;
    try {
        {
            wxProgressDialog progress(ui::utf8("Importerar Reformationsbibeln 2026"),
                                      ui::utf8("Kontrollerar filerna och läser in bibeltexten…"), 100, this,
                                      wxPD_APP_MODAL | wxPD_AUTO_HIDE);
            int last = -1;
            import_reformationsbibeln(
                ui::filesystem_path(old_testament.GetPath()), ui::filesystem_path(new_testament.GetPath()),
                resources_ / "content/srb2026.recipe", data_ / "content/srb2026.db", [&](int value) {
                    if (value != last) {
                        progress.Update(value);
                        last = value;
                    }
                });
        }
        stop_speech(false);
        corpus_.load_imported(data_ / "content/srb2026.db");
        for (auto* preferences : {&settings_.primary_sources, &settings_.secondary_sources})
            if (std::ranges::find(*preferences, srb_source, &SourcePreference::source) == preferences->end())
                preferences->push_back({srb_source, false});
        apply_settings();
        // A replacement keeps the edition list but changes its cached text and headings.
        scripture_->apply(settings_, true);
        if (navigation_.current().screen == Screen::Settings)
            fill_settings(static_cast<wxScrolledWindow*>(current_page()));
        wxMessageBox(ui::utf8("Reformationsbibeln 2026 är importerad. Aktivera den under Böcker och "
                              "textkällor och flytta den uppåt om du vill använda den som huvudtext."),
                     ui::utf8("Importen är klar"), wxOK | wxICON_INFORMATION, this);
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8("PDF-filerna kunde inte importeras"), wxOK | wxICON_ERROR,
                     this);
    }
}
void MainFrame::remove_content() {
    if (!corpus_.has_imported())
        return;
    if (wxMessageBox(ui::utf8("Ta bort den importerade Reformationsbibeln 2026? "
                              "Dina PDF-filer, bokmärken och läsframsteg sparas."),
                     ui::utf8("Ta bort importerad text"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                     this) != wxYES)
        return;
    try {
        save_reading_position();
        stop_speech(false);
        const bool primary_removed = std::ranges::any_of(settings_.primary_sources, [](const auto& source) {
            return source.source == srb_source && source.enabled;
        });
        corpus_.remove_imported();
        for (auto* preferences : {&settings_.primary_sources, &settings_.secondary_sources})
            std::erase_if(*preferences, [](const auto& preference) {
                return preference.source == srb_source;
            });
        if (primary_removed || std::ranges::none_of(settings_.primary_sources, &SourcePreference::enabled)) {
            const auto fallback =
                std::ranges::find(settings_.primary_sources, "sv1917", &SourcePreference::source);
            if (fallback == settings_.primary_sources.end())
                settings_.primary_sources.push_back({"sv1917", true});
            else if (!fallback->enabled) {
                // Preserve the priority of existing editions; Swedish fills their remaining gaps.
                settings_.primary_sources.erase(fallback);
                settings_.primary_sources.push_back({"sv1917", true});
            }
        }
        apply_settings();
        if (navigation_.current().screen == Screen::Settings)
            fill_settings(static_cast<wxScrolledWindow*>(current_page()));
    } catch (const std::exception& error) {
        wxMessageBox(ui::utf8(error.what()), ui::utf8("Importen kunde inte tas bort"), wxOK | wxICON_ERROR,
                     this);
    }
}
} // namespace ortho
