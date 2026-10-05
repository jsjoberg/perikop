#pragma once
#include <wx/dc.h>
#include <filesystem>
#include <string>
#include <vector>
namespace ortho {
struct TextRun { wxString text; double x=0, width=0; };
struct TextLine { std::vector<TextRun> runs; double width=0, space=0, left_protrusion=0, right_protrusion=0; bool justified=false, hyphenated=false; };
struct TextLayout {
    std::vector<TextLine> lines;
    int line_height=0;
    int height() const { return int(lines.size())*line_height; }
};
// TeX's total-paragraph fit and Liang patterns; wxDC retains native word shaping.
void load_hyphenation(const std::filesystem::path& directory);
std::vector<int> hyphenation_points(const wxString&,const std::string& language);
TextLayout layout_paragraph(wxDC&,const wxString&,int width,const std::string& language="sv");
void draw_paragraph(wxDC&,const TextLayout&,int x,int y);
}
