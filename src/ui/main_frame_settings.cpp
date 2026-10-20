#include "storage/database.hpp"
#include "ui/controls.hpp"
#include "ui/main_frame.hpp"
#include <algorithm>
#include <wx/checkbox.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
namespace ortho {
namespace {
class SourceStack final : public wxPanel {
public:
    SourceStack(wxWindow* parent, std::vector<SourcePreference>& preferences,
                const std::vector<Source>& catalog, bool primary, std::function<void()> changed)
        : wxPanel(parent), preferences_(preferences), catalog_(catalog), primary_(primary),
          changed_(std::move(changed)) {
        rebuild();
    }

private:
    std::vector<SourcePreference>& preferences_;
    std::vector<Source> catalog_;
    bool primary_;
    std::function<void()> changed_;
    void rebuild() {
        DestroyChildren();
        auto* stack = new wxBoxSizer(wxVERTICAL);
        const auto enabled = std::ranges::count(preferences_, true, &SourcePreference::enabled);
        for (std::size_t i = 0; i < preferences_.size(); ++i) {
            const auto& preference = preferences_[i];
            const auto found = std::ranges::find(catalog_, preference.source, &Source::code);
            const auto name = ui::utf8(found == catalog_.end() ? preference.source : found->name);
            auto* row = new wxBoxSizer(wxHORIZONTAL);
            auto* check = new wxCheckBox(this, wxID_ANY, name);
            check->SetFont(ui_font());
            check->SetValue(preference.enabled);
            check->SetName(ui::utf8(primary_ ? "Huvudtext, " : "Andra texten, ") + name);
            check->SetToolTip(ui::utf8("Aktivera eller inaktivera denna källa i prioriteringsordningen."));
            check->Enable(found != catalog_.end() && (!primary_ || !preference.enabled || enabled > 1));
            check->Bind(wxEVT_CHECKBOX, [this, i](wxCommandEvent& event) {
                preferences_[i].enabled = event.IsChecked();
                changed_();
                CallAfter([this] {
                    rebuild();
                });
            });
            row->Add(check, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(zoomed(12)));
            for (const int direction : {-1, 1}) {
                auto* move = ui::button(this, ui::utf8(direction < 0 ? "↑" : "↓"), [this, i, direction] {
                    CallAfter([this, i, direction] {
                        const auto destination = direction < 0 ? i - 1 : i + 1;
                        std::swap(preferences_[i], preferences_[destination]);
                        changed_();
                        rebuild();
                        if (auto* item = GetSizer()
                                             ->GetItem(destination)
                                             ->GetSizer()
                                             ->GetItem(std::size_t{0})
                                             ->GetWindow())
                            item->SetFocus();
                    });
                });
                const auto label = ui::utf8(direction < 0 ? "Flytta upp " : "Flytta ner ") + name;
                move->SetName(label);
                move->SetToolTip(label);
                move->Enable(direction < 0 ? i > 0 : i + 1 < preferences_.size());
                row->Add(move, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(zoomed(4)));
            }
            stack->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(zoomed(5)));
        }
        SetSizer(stack, true);
        SetMinSize(stack->GetMinSize());
        Layout();
        if (GetParent())
            GetParent()->Layout();
        for (auto* child : GetChildren()) {
            child->SetBackgroundColour(GetBackgroundColour());
            child->SetForegroundColour(GetForegroundColour());
        }
    }
};
// The chosen colour scheme's light and dark variants as small reader pages, so both can be judged at once.
class SchemePreview final : public wxWindow {
public:
    explicit SchemePreview(wxWindow* parent) : wxWindow(parent, wxID_ANY) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(zoomed(wxSize(2 * page_width + gap, page_height + 22))));
        Bind(wxEVT_PAINT, &SchemePreview::paint, this);
    }
    bool AcceptsFocus() const override {
        return false;
    }

private:
    static constexpr int page_width = 210, page_height = 172, gap = 14;
    void paint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc)
            return;
        const double unit = FromDIP(zoomed(100)) / 100.0;
        const auto& names = scheme_names[std::size_t(color_scheme)];
        for (const bool dark : {false, true}) {
            gc->PushState();
            gc->Translate(dark ? (page_width + gap) * unit : 0, 0);
            gc->SetFont(ui_font(9), GetForegroundColour());
            gc->DrawText(ui::utf8(dark ? names.dark : names.light), 0, 0);
            gc->Translate(0, 20 * unit);
            page(*gc, palette(color_scheme, dark), unit);
            gc->PopState();
        }
    }
    static void page(wxGraphicsContext& gc, const Palette& colors, double unit) {
        double width = 0, height = 0;
        const auto text = [&](const wxString& line, const wxFont& font, const wxColour& colour, double x,
                              double y) {
            gc.SetFont(font, colour);
            gc.DrawText(line, x * unit, y * unit);
            gc.GetTextExtent(line, &width, &height);
            return x + width / unit;
        };
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colors.rule).Width(unit)));
        gc.SetBrush(wxBrush(colors.paper));
        gc.DrawRoundedRectangle(0, 0, page_width * unit, page_height * unit, 6 * unit);
        gc.SetPen(*wxTRANSPARENT_PEN);
        // The address field, resting and hovered.
        gc.SetBrush(wxBrush(colors.surface));
        gc.DrawRoundedRectangle(46 * unit, 9 * unit, 118 * unit, 18 * unit, 5 * unit);
        gc.SetBrush(wxBrush(colors.overlay));
        gc.DrawRoundedRectangle(14 * unit, 9 * unit, 22 * unit, 18 * unit, 5 * unit);
        text(ui::utf8("Joh 1:1–5"), ui_font(7.5), colors.ink, 82, 12);
        gc.SetBrush(wxBrush(colors.rule));
        gc.DrawRectangle(0, 35 * unit, page_width * unit, unit);
        text("JOHANNES", ui_font(6.5), colors.muted, 82, 44);
        text("1", body_font(13), colors.ink, 100, 54);
        // The prescribed-reading bar, a selected phrase, and verse numbers in the rubric colour.
        gc.SetBrush(wxBrush(colors.accent));
        gc.DrawRoundedRectangle(14 * unit, 80 * unit, 2.5 * unit, 47 * unit, unit);
        const auto body = body_font(8.5), number = body_font(6);
        double x = text("1", number, colors.verse, 24, 80);
        text(ui::utf8("I begynnelsen var Ordet,"), body, colors.ink, x + 2, 82);
        x = text(ui::utf8("och Ordet var "), body, colors.ink, 24, 97);
        gc.GetTextExtent(ui::utf8("hos Gud,"), &width, &height);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(colors.selection));
        gc.DrawRoundedRectangle((x - 3) * unit, 96 * unit, width + 6 * unit, height + 2 * unit, 3 * unit);
        text(ui::utf8("hos Gud,"), body, colors.ink, x, 97);
        x = text("2", number, colors.verse, 24, 110);
        x = text(ui::utf8("och Ordet var "), body, colors.ink, x + 2, 112);
        text("Gud.", body, colors.accent, x, 112);
        text(ui::utf8("Ordet finns inte i texten."), body, colors.muted, 24, 127);
        // The calendar's today ring and service markers.
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colors.today).Width(1.4 * unit)));
        gc.DrawRoundedRectangle(24 * unit, 148 * unit, 18 * unit, 14 * unit, 3 * unit);
        gc.SetPen(*wxTRANSPARENT_PEN);
        for (std::size_t i = 0; i < colors.services.size(); ++i) {
            gc.SetBrush(wxBrush(colors.services[i]));
            gc.DrawEllipse((54 + 12 * double(i)) * unit, 152 * unit, 6 * unit, 6 * unit);
        }
    }
};
} // namespace
void MainFrame::show_settings() {
    if (navigation_.current().screen == Screen::Settings)
        return;
    auto* page =
        new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetScrollRate(0, FromDIP(zoomed(12)));
    fill_settings(page);
    show_page(page, Screen::Settings, ui::utf8("Inställningar"));
}
void MainFrame::fill_settings(wxScrolledWindow* page) {
    // Native radio buttons and a check box, so screen readers and arrow keys work as everywhere else.
    page->DestroyChildren();
    auto* content = new wxPanel(page);
    auto* entries = new wxBoxSizer(wxVERTICAL);
    content->SetSizer(entries);
    const auto sources = corpus_.sources();
    const auto source_stack = [&](const char* heading, std::vector<SourcePreference>& preferences,
                                  bool primary) {
        entries->Add(ui::label(content, ui::utf8(heading), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        entries->Add(new SourceStack(content, preferences, sources, primary,
                                     [this, primary] {
                                         if (primary)
                                             stop_speech(false);
                                         apply_settings();
                                     }),
                     0, wxEXPAND);
        entries->AddSpacer(FromDIP(zoomed(14)));
    };
    source_stack("HUVUDTEXT – KÄLLOR I PRIORITETSORDNING", settings_.primary_sources, true);
    source_stack("DELAD LÄSNING – ANDRA TEXTENS KÄLLOR", settings_.secondary_sources, false);
    entries->Add(
        ui::label(content, ui::utf8("Den första aktiverade källan med text i kapitlet används."), 10));
    entries->Add(ui::label(
        content, ui::utf8("Andra texten väljer en annan källa. Uppläsning finns bara för svenska."), 10));
    entries->AddSpacer(FromDIP(zoomed(22)));
    const auto choices = [&](const char* heading, std::initializer_list<const char*> names, int chosen,
                             const std::function<void(int)>& choose) {
        entries->Add(ui::label(content, ui::utf8(heading), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
        int i = 0;
        for (const auto* name : names) {
            auto* radio = new wxRadioButton(content, wxID_ANY, ui::utf8(name), wxDefaultPosition,
                                            wxDefaultSize, i ? 0 : wxRB_GROUP);
            radio->SetFont(ui_font());
            radio->SetValue(i == chosen);
            radio->Bind(wxEVT_RADIOBUTTON, [choose, i](wxCommandEvent&) {
                choose(i);
            });
            entries->Add(radio, 0, wxBOTTOM, FromDIP(zoomed(6)));
            ++i;
        }
        entries->AddSpacer(FromDIP(zoomed(18)));
    };
    // The scheme list beside a preview of its light and dark variants.
    entries->Add(ui::label(content, ui::utf8("FÄRGSCHEMA"), 10), 0, wxTOP | wxBOTTOM, FromDIP(zoomed(8)));
    auto* schemes = new wxBoxSizer(wxVERTICAL);
    auto* preview = new SchemePreview(content);
    for (std::size_t i = 0; i < scheme_names.size(); ++i) {
        auto* radio = new wxRadioButton(content, wxID_ANY, ui::utf8(scheme_names[i].name), wxDefaultPosition,
                                        wxDefaultSize, i ? 0 : wxRB_GROUP);
        radio->SetFont(ui_font());
        radio->SetValue(ColorScheme(i) == settings_.color_scheme);
        radio->Bind(wxEVT_RADIOBUTTON, [this, i](wxCommandEvent&) {
            settings_.color_scheme = ColorScheme(i);
            apply_settings();
        });
        schemes->Add(radio, 0, wxBOTTOM, FromDIP(zoomed(6)));
    }
    auto* scheme_row = new wxBoxSizer(wxHORIZONTAL);
    scheme_row->Add(schemes, 0, wxRIGHT, FromDIP(zoomed(28)));
    scheme_row->Add(preview);
    entries->Add(scheme_row);
    entries->AddSpacer(FromDIP(zoomed(18)));
    choices("TEMA", {"Systemets tema", "Ljust tema", "Mörkt tema"}, int(settings_.theme), [this](int i) {
        settings_.theme = static_cast<Theme>(i);
        apply_settings();
    });
    const auto day_changed = [this] {
        clear_preparation();
        refresh_day();
        apply_settings();
    };
    choices("KALENDER", {"Gregoriansk / reviderad juliansk", "Juliansk"},
            settings_.calendar == CalendarStyle::Old, [this, day_changed](int i) {
                settings_.calendar = i ? CalendarStyle::Old : CalendarStyle::New;
                day_changed();
            });
    choices("LÄSORDNING", {"Grekiska läsordningen", "Slaviska läsordningen"},
            settings_.tradition == Tradition::Slavic, [this, day_changed](int i) {
                settings_.tradition = i ? Tradition::Slavic : Tradition::Greek;
                day_changed();
            });
    // Read-aloud is Swedish only.
    static constexpr const char* voices[] = {"alice", "bjorn", "enkel"};
    choices("RÖST FÖR UPPLÄSNING", {"Alice", "Björn", "Enkel röst (robotlik, snabb)"},
            settings_.speech_voice == "bjorn"   ? 1
            : settings_.speech_voice == "enkel" ? 2
                                                : 0,
            [this](int i) {
                clear_preparation();
                settings_.speech_voice = voices[i];
                speech_->set_voice(settings_.speech_voice);
                apply_settings();
            });
    auto* highlight = new wxCheckBox(content, wxID_ANY, ui::utf8("Markera texten som läses upp"));
    highlight->SetFont(ui_font());
    highlight->SetValue(settings_.speech_highlight);
    highlight->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        settings_.speech_highlight = event.IsChecked();
        apply_settings();
    });
    entries->Add(highlight);
    entries->AddSpacer(FromDIP(zoomed(18)));
    choices("UPPDATERINGSKANAL", {"Utgåvor (rekommenderas)", "Nattbyggen (experimentella)"},
            settings_.update_channel == UpdateChannel::Nightly, [this](int i) {
                settings_.update_channel = i ? UpdateChannel::Nightly : UpdateChannel::Releases;
                apply_settings();
                // An offer from the other channel no longer applies.
                updater_->reset();
            });
    entries->Add(ui::label(content, ui::utf8("Valet startar ingen sökning eller hämtning.")));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(content, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, FromDIP(zoomed(40)));
    page->SetSizer(outer);
    ui::recolor(page, palette(settings_.theme));
    page->FitInside();
    page->Layout();
}
} // namespace ortho
