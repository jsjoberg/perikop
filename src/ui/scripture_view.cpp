#include "ui/scripture_view.hpp"
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <algorithm>
namespace ortho {
namespace { wxString u(const std::string& s){return wxString::FromUTF8(s);} }
ScriptureView::ScriptureView(wxWindow* parent,const CorpusDb& corpus)
    :wxVScrolledWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE|wxWANTS_CHARS),corpus_(corpus) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetName("Kontinuerlig skriftläsare");
    Bind(wxEVT_PAINT,&ScriptureView::paint,this);
    Bind(wxEVT_SIZE,[this](wxSizeEvent& e){invalidate();e.Skip();});
    Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& e){
        if(e.GetKeyCode()==WXK_HOME){ScrollToRow(0);return;}
        if(e.GetKeyCode()==WXK_END && !rows_.empty()){ScrollToRow(rows_.size()-1);return;}
        if(rows_.empty()){e.Skip();return;}
        const auto begin=GetVisibleRowsBegin();
        if(e.GetKeyCode()==WXK_DOWN){ScrollToRow(std::min(begin+1,rows_.size()-1));return;}
        if(e.GetKeyCode()==WXK_UP){ScrollToRow(begin?begin-1:0);return;}
        if(e.GetKeyCode()==WXK_PAGEDOWN){ScrollToRow(std::min(GetVisibleRowsEnd(),rows_.size()-1));return;}
        if(e.GetKeyCode()==WXK_PAGEUP){const auto span=GetVisibleRowsEnd()-begin;ScrollToRow(begin>span?begin-span:0);return;}
        e.Skip();
    });
}
int ScriptureView::columns_count() const { return settings_.parallel.empty()?1:settings_.parallel=="el,en"?3:2; }
int ScriptureView::outside_margin() const {
    const int width=GetClientSize().x;
    if(columns_count()==1)return std::max(32,(width-std::min(FromDIP(680),width-64))/2);
    return std::clamp(width/14,32,105);
}
int ScriptureView::column_width() const {
    const int usable=std::max(120,GetClientSize().x-2*outside_margin());
    const bool stacked=usable/columns_count()<270;
    return stacked?usable-30:(usable-44*(columns_count()-1))/columns_count()-30;
}
void ScriptureView::open(const Reading& reading) {
    reading_=reading; rows_.clear();
    int chapter=0;
    for(auto ref:corpus_.coordinates("sv1917",reading.passage.book)) {
        if(ref.chapter!=chapter){chapter=ref.chapter;rows_.push_back({ref,true});}
        rows_.push_back({ref,false});
    }
    cache_.clear();order_.clear();
    SetRowCount(rows_.size()); RefreshAll(); center_passage(); Refresh();
}
void ScriptureView::center_passage() {
    auto it=std::find_if(rows_.begin(),rows_.end(),[this](auto& r){return !r.heading && r.ref==reading_.passage.first;});
    if(it!=rows_.end()) {
        const auto index=std::size_t(it-rows_.begin());
        const auto begin=index>2?index-2:0;
        const int context_height=index>1?OnGetRowHeight(index-2)+OnGetRowHeight(index-1):0;
        // On a short viewport, start at the chapter heading. Earlier context
        // remains immediately available by scrolling upward.
        const bool fits=context_height+OnGetRowHeight(index)<GetClientSize().y;
        ScrollToRow(fits?begin:(index && rows_[index-1].heading?index-1:index));
    }
}
void ScriptureView::apply(const Settings& settings) {
    settings_=settings; const auto colors=palette(settings.theme);
    SetBackgroundColour(colors.paper);SetForegroundColour(colors.ink);invalidate();
}
void ScriptureView::invalidate() {
    const auto begin=GetVisibleRowsBegin();cache_.clear();order_.clear();
    RefreshAll(); if(!rows_.empty())ScrollToRow(std::min(begin,rows_.size()-1)); Refresh();
}
const ScriptureView::Layout& ScriptureView::row_layout(std::size_t index) const {
    if(auto it=cache_.find(index);it!=cache_.end())return it->second;
    wxClientDC dc(const_cast<ScriptureView*>(this));dc.SetFont(body_font(settings_.font_size));
    Layout layout;
    if(rows_[index].heading)layout.height=FromDIP(124);
    else {
        std::vector<std::string> languages{"sv"};
        if(settings_.parallel=="el"||settings_.parallel=="el,en")languages.push_back("el");
        if(settings_.parallel=="en"||settings_.parallel=="el,en")languages.push_back("en");
        const bool stacked=(GetClientSize().x-2*outside_margin())/columns_count()<270;
        int height=0;
        for(const auto& language:languages) {
            auto verse=corpus_.parallel_verse("sv1917",source_for_language(language,reading_.passage.book),reading_.passage.book,rows_[index].ref);
            Column column{language,verse?verse->ref:rows_[index].ref,verse?verse->last:std::nullopt,layout_paragraph(dc,u(verse?verse->text:verse.error()),std::max(80,column_width())),!verse};
            const int h=column.text.height()+(stacked&&languages.size()>1?FromDIP(24):0);
            height=stacked?height+h:std::max(height,h);
            layout.columns.push_back(std::move(column));
        }
        layout.height=height+FromDIP(22);
    }
    while(cache_.size()>=cache_limit){cache_.erase(order_.front());order_.pop_front();}
    order_.push_back(index);
    return cache_.emplace(index,std::move(layout)).first->second;
}
wxCoord ScriptureView::OnGetRowHeight(std::size_t index) const {
    if(index>=rows_.size())return 1;
    return row_layout(index).height;
}
void ScriptureView::draw(wxDC& dc,wxSize size,std::size_t begin,std::size_t end) const {
    const auto colors=palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));dc.Clear();
    const int margin=outside_margin(), usable=size.x-2*margin;
    const bool stacked=usable/columns_count()<270;
    const int stride=(usable-44*(columns_count()-1))/columns_count()+44;
    int y=FromDIP(12);
    for(std::size_t index=begin;index<end && y<size.y;++index) {
        const auto& row=rows_[index];const auto& layout=row_layout(index);
        if(row.heading) {
            dc.SetTextForeground(colors.muted);dc.SetFont(ui_font(11));
            auto name=u(corpus_.book_name(reading_.passage.book)).Upper();
            const auto extent=dc.GetTextExtent(name);dc.DrawText(name,(size.x-extent.x)/2,y+FromDIP(15));
            dc.SetFont(body_font(28));dc.SetTextForeground(colors.ink);
            const wxString title=wxString::Format("%d",row.ref.chapter);
            dc.DrawText(title,(size.x-dc.GetTextExtent(title).x)/2,y+FromDIP(42));
            dc.SetFont(ui_font(9));dc.SetTextForeground(colors.muted);
            wxString labels="SVENSKA 1917";
            if(settings_.parallel=="el"||settings_.parallel=="el,en") labels+="     ·     ΕΛΛΗΝΙΚΑ";
            if(settings_.parallel=="en"||settings_.parallel=="el,en") labels+="     ·     ENGLISH";
            if(reading_.passage.book=="Ps" && !settings_.parallel.empty() && settings_.parallel!="en") {
                const auto map=corpus_.alignment("sv1917","grc-ot-fixture",{reading_.passage.book,row.ref,row.ref});
                if(map)labels+=wxString::Format("     ·     MT %d / LXX %d",row.ref.chapter,map->to.first.chapter);
            }
            dc.DrawText(labels,(size.x-dc.GetTextExtent(labels).x)/2,y+FromDIP(93));
        } else {
            if(columns_count()>1&&!stacked) {
                dc.SetPen(wxPen(colors.rule,1));
                for(int c=1;c<columns_count();++c){const int gutter=margin+c*stride-FromDIP(24);dc.DrawLine(gutter,y,gutter,y+layout.height);}
            }
            if(reading_.passage.contains(row.ref)) {
                dc.SetPen(wxPen(colors.accent,FromDIP(3)));dc.DrawLine(margin-FromDIP(17),y,margin-FromDIP(17),y+layout.height-FromDIP(5));
            }
            int stacked_y=y;
            for(std::size_t c=0;c<layout.columns.size();++c) {
                const auto& column=layout.columns[c];
                const int x=margin+(stacked?0:int(c)*stride);
                int text_y=stacked?stacked_y:y;
                if(stacked&&layout.columns.size()>1) {
                    dc.SetFont(ui_font(8));dc.SetTextForeground(colors.muted);
                    dc.DrawText(column.language=="sv"?"SVENSKA":column.language=="el"?"ΕΛΛΗΝΙΚΑ":"ENGLISH",x+FromDIP(30),text_y);
                    text_y+=FromDIP(24);
                }
                dc.SetFont(ui_font(9));dc.SetTextForeground(colors.muted);
                dc.DrawText(column.last?wxString::Format("%d–%d",column.ref.verse,column.last->verse):wxString::Format("%d",column.ref.verse),x,text_y+FromDIP(6));
                dc.SetFont(body_font(settings_.font_size));dc.SetTextForeground(column.missing?colors.muted:colors.ink);
                for(const auto& line:column.text.lines){dc.DrawText(line,x+FromDIP(30),text_y);text_y+=column.text.line_height;}
                if(stacked)stacked_y=text_y;
            }
        }
        y+=layout.height;
    }

}
void ScriptureView::paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);draw(dc,GetClientSize(),GetVisibleRowsBegin(),GetVisibleRowsEnd());
}
void ScriptureView::render_to(wxDC& dc,wxSize size) { draw(dc,size,GetVisibleRowsBegin(),rows_.size()); }
}
