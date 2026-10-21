#include "storage/content_import.hpp"
#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
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
    };
    paragraph(
        "Reformationsbibeln 2026 kan läggas till från två PDF-filer som du själv hämtar från "
        "Svenska Reformationsbibelsällskapet. Båda testamentena behövs. Texten sparas bara i din profil.");
    const auto link = [&](const char* title, const char* url) {
        auto* control = new wxHyperlinkCtrl(content, wxID_ANY, ui::utf8(title), ui::utf8(url));
        control->SetFont(ui_font());
        entries->Add(control, 0, wxBOTTOM, FromDIP(zoomed(6)));
    };
    link("Hämta bibelböcker från bibel.se",
         "https://bibel.se/wordpress/index.php/2022/01/18/oversatta-bibelbocker/");
    link("Hämta hela Gamla testamentet (PDF)", "https://www.bibel.se/objfiles/PDF_books/SRBhelaGT.pdf");
    link("Hämta hela Nya testamentet (PDF)", "https://www.bibel.se/objfiles/PDF_books/SRBNT-UNnoter.pdf");
    if (corpus_.has_imported())
        paragraph(
            "Reformationsbibeln 2026 är importerad. Aktivera den i källorna ovan och välj dess prioritet.");
    else {
        paragraph(
            "Importen stöder de länkade 2026-filerna. Fotnoter, korshänvisningar och kapitelrubriker "
            "importeras inte. Bibeltexten kan läsas, slås upp och läsas upp som övriga svenska texter.");
        auto* button = ui::button(content, ui::utf8("Importera båda PDF-filerna…"), [this] {
            CallAfter([this] {
                import_content();
            });
        });
        button->SetToolTip(ui::utf8("Välj först Gamla och sedan Nya testamentets PDF från bibel.se"));
        entries->Add(button, 0, wxBOTTOM, FromDIP(zoomed(8)));
    }
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
        corpus_.load_imported(data_ / "content/srb2026.db");
        for (auto* preferences : {&settings_.primary_sources, &settings_.secondary_sources})
            if (std::ranges::find(*preferences, srb_source, &SourcePreference::source) == preferences->end())
                preferences->push_back({srb_source, false});
        clear_preparation();
        apply_settings();
        if (navigation_.current().screen == Screen::Settings)
            fill_settings(static_cast<wxScrolledWindow*>(current_page()));
        wxMessageBox(ui::utf8("Reformationsbibeln 2026 är importerad. Aktivera den i källorna och flytta "
                              "den uppåt om du vill använda den som huvudtext."),
                     ui::utf8("Importen är klar"), wxOK | wxICON_INFORMATION, this);
    } catch (const std::exception& e) {
        wxMessageBox(ui::utf8(e.what()), ui::utf8("PDF-filerna kunde inte importeras"), wxOK | wxICON_ERROR,
                     this);
    }
}
} // namespace ortho
