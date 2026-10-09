#pragma once
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <wx/dc.h>
#include <wx/graphics.h>
namespace ortho {
struct TextRun {
    wxString text;
    double x = 0, width = 0;
    int tag = -1;
    bool marker = false;
};
struct TextFragment {
    wxString text, label;
    int tag = -1;
};
struct TextLine {
    std::vector<TextRun> runs;
    double width = 0, space = 0, left_protrusion = 0, right_protrusion = 0;
    bool justified = false, hyphenated = false;
};
struct TextLayout {
    std::vector<TextLine> lines;
    int line_height = 0;
    double demerits = 0;
    int height() const {
        return int(lines.size()) * line_height;
    }
};
// TeX's total-paragraph fit and Liang patterns; wxDC retains native word shaping.
void load_hyphenation(const std::filesystem::path& directory);
std::vector<int> hyphenation_points(const wxString&, const std::string& language);
TextLayout layout_paragraph(wxDC&, const wxString&, int width, const std::string& language = "sv");
TextLayout layout_paragraph(wxDC&, const std::vector<TextFragment>&, int width,
                            const std::string& language = "sv");
// colour may return another colour for a run, such as muted text.
void draw_paragraph(wxDC&, const TextLayout&, int x, int y,
                    const std::function<std::optional<wxColour>(const TextRun&)>& colour = {});
// Draw a line range into a reusable context. DPI comes from the destination,
// so offscreen text has the same metrics as text drawn into a native window.
void draw_paragraph(wxGraphicsContext&, const TextLayout&, const wxFont&, const wxRealPoint& dpi,
                    const wxColour& ink, int x, int y, std::size_t first, std::size_t end,
                    const std::function<std::optional<wxColour>(const TextRun&)>& colour = {});
} // namespace ortho
