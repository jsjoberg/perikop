#pragma once
#include <wx/dc.h>
#include <vector>
namespace ortho {
// wxDC uses the platform's Unicode shaping and measures/draws the same font.
// Paragraph-wide justification and language hyphenation remain future work.
struct TextLayout { std::vector<wxString> lines; int line_height=0; int height() const { return int(lines.size())*line_height; } };
TextLayout layout_paragraph(wxDC&,const wxString&,int width);
}
