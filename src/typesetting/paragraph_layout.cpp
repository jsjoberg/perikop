#include "typesetting/paragraph_layout.hpp"
#include <wx/tokenzr.h>
#include <algorithm>
namespace ortho {
TextLayout layout_paragraph(wxDC& dc,const wxString& text,int width) {
    TextLayout result;
    result.line_height=std::max(1, int(dc.GetTextExtent("Ågjἄ").GetHeight()*1.48));
    wxStringTokenizer words(text, " \t\r\n");
    wxString line;
    while(words.HasMoreTokens()) {
        auto word=words.GetNextToken();
        auto candidate=line.empty()?word:line+" "+word;
        if(!line.empty() && dc.GetTextExtent(candidate).GetWidth()>width) { result.lines.push_back(line);line=word; }
        else line=candidate;
    }
    if(!line.empty())result.lines.push_back(line);
    if(result.lines.empty())result.lines.push_back("");
    return result;
}
}
