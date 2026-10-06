#include "ui/scripture_view.hpp"
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <algorithm>
#include <cmath>
#ifdef __APPLE__
#include "ui/native_scroll.hpp"
#endif
namespace ortho {
namespace { wxString u(const std::string& s){return wxString::FromUTF8(s);} }
ScriptureView::ScriptureView(wxWindow* parent,const CorpusDb& corpus)
    :wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE|wxWANTS_CHARS|wxVSCROLL),wheel_timer_(this),corpus_(corpus) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);SetName("Kontinuerlig skriftläsare");
    Bind(wxEVT_PAINT,&ScriptureView::paint,this);
    Bind(wxEVT_SIZE,[this](wxSizeEvent& e){if(layout_width_!=GetClientSize().x)invalidate();scrollbar();e.Skip();});
    Bind(wxEVT_MOUSEWHEEL,[this](wxMouseEvent& e){
        if(e.GetWheelAxis()!=wxMOUSE_WHEEL_VERTICAL){e.Skip();return;}
#ifdef __APPLE__
        scroll_by(-e.GetWheelRotation()); // fallback; Cocoa monitor preserves fractional deltas
#else
        const double amount=-double(e.GetWheelRotation())/std::max(1,e.GetWheelDelta())*
            (e.IsPageScroll()?GetClientSize().y*0.85:std::max(1,e.GetLinesPerAction())*FromDIP(30));
        target_=std::clamp(target_+amount,0.0,std::max(0.0,positions_.empty()?0.0:positions_.back()-GetClientSize().y));
        wheel_timer_.Start(16);
#endif
    });
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){
        const double distance=target_-offset_;
        if(std::abs(distance)<0.5){set_position(target_);wheel_timer_.Stop();}
        else set_position(offset_+distance*0.3);
    });
    const auto scroll_event=[this](wxScrollWinEvent& e){
        wheel_timer_.Stop();
        if(e.GetEventType()==wxEVT_SCROLLWIN_LINEUP)scroll_by(-FromDIP(30));
        else if(e.GetEventType()==wxEVT_SCROLLWIN_LINEDOWN)scroll_by(FromDIP(30));
        else if(e.GetEventType()==wxEVT_SCROLLWIN_PAGEUP)scroll_by(-GetClientSize().y*0.85);
        else if(e.GetEventType()==wxEVT_SCROLLWIN_PAGEDOWN)scroll_by(GetClientSize().y*0.85);
        else {set_position(e.GetPosition());target_=offset_;}
    };
    for(const auto type:{wxEVT_SCROLLWIN_LINEUP,wxEVT_SCROLLWIN_LINEDOWN,wxEVT_SCROLLWIN_PAGEUP,wxEVT_SCROLLWIN_PAGEDOWN,wxEVT_SCROLLWIN_THUMBTRACK,wxEVT_SCROLLWIN_THUMBRELEASE,wxEVT_SCROLLWIN_TOP,wxEVT_SCROLLWIN_BOTTOM})Bind(type,scroll_event);
    Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& e){
        const int key=e.GetKeyCode();
        if(key==WXK_HOME){set_position(0);return;}
        if(key==WXK_END){set_position(positions_.empty()?0:positions_.back());return;}
        if(key==WXK_DOWN){scroll_by(FromDIP(30));return;}
        if(key==WXK_UP){scroll_by(-FromDIP(30));return;}
        if(key==WXK_PAGEDOWN){scroll_by(GetClientSize().y*0.85);return;}
        if(key==WXK_PAGEUP){scroll_by(-GetClientSize().y*0.85);return;}
        e.Skip();
    });
#ifdef __APPLE__
    native_scroll_=install_native_scroll(this,[this](double pixels){scroll_by(pixels);});
#endif
}
ScriptureView::~ScriptureView(){wheel_timer_.Stop();
#ifdef __APPLE__
    remove_native_scroll(native_scroll_);
#endif
}
void ScriptureView::rebuild_positions() {
    positions_.assign(heights_.size()+1,0);
    for(std::size_t i=0;i<heights_.size();++i)positions_[i+1]=positions_[i]+heights_[i];
}
std::size_t ScriptureView::first_visible() const {
    if(rows_.empty())return 0;
    return std::min(rows_.size()-1,std::size_t(std::upper_bound(positions_.begin(),positions_.end(),offset_)-positions_.begin()-1));
}
void ScriptureView::prepare_visible() {
    if(rows_.empty())return;
    const auto anchor=first_visible();const double within=offset_-positions_[anchor];
    const auto begin=anchor>8?anchor-8:0;
    double covered=0;bool changed=false;
    for(std::size_t i=begin;i<rows_.size()&&(i<=anchor||covered<GetClientSize().y*2+600);++i) {
        const int height=row_layout(i).height;
        changed|=heights_[i]!=height;heights_[i]=height;
        if(i>=anchor)covered+=height;
    }
    if(changed){rebuild_positions();const double adjustment=positions_[anchor]+within-offset_;offset_+=adjustment;target_+=adjustment;}
}
void ScriptureView::scrollbar() {
    const int page=std::max(1,GetClientSize().y);
    const int total=positions_.empty()?page:std::max(page,int(std::ceil(positions_.back())));
    SetScrollbar(wxVERTICAL,int(std::lround(offset_)),page,total,true);
}
void ScriptureView::set_position(double value) {
    prepare_visible();
    offset_=std::clamp(value,0.0,std::max(0.0,positions_.empty()?0.0:positions_.back()-GetClientSize().y));
    prepare_visible();offset_=std::clamp(offset_,0.0,std::max(0.0,positions_.empty()?0.0:positions_.back()-GetClientSize().y));scrollbar();Refresh(false);
}
void ScriptureView::scroll_by(double pixels){set_position(offset_+pixels);target_=offset_;}
std::vector<std::string> ScriptureView::languages() const {
    std::vector<std::string> result{base_source_=="sv1917"?"sv":base_source_.starts_with("en-")?"en":"el"};
    const auto add=[&](const std::string& language){if(std::find(result.begin(),result.end(),language)==result.end())result.push_back(language);};
    if(settings_.parallel=="el,en"){add("sv");add("el");add("en");}
    else if(!settings_.parallel.empty())add(settings_.parallel);
    return result;
}
int ScriptureView::columns_count() const { return int(languages().size()); }
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
void ScriptureView::open(const Reading& reading) { reading_=reading;open_section(0); }
void ScriptureView::open_section(std::size_t index) {
    const auto segments=reading_.segments();if(index>=segments.size())return;
    displayed_=segments[index];rows_.clear();
    base_source_=source_for_language(reading_.base_language,displayed_.book);
    if(!reading_.source_override.empty())base_source_=reading_.source_override;
    if(corpus_.coordinates(base_source_,displayed_.book).empty())for(const std::string language:{"el","en"}) {
        const auto source=source_for_language(language,displayed_.book);
        if(!corpus_.coordinates(source,displayed_.book).empty()){base_source_=source;break;}
    }
    int chapter=0;
    for(auto ref:corpus_.coordinates(base_source_,displayed_.book)) {
        if(ref.chapter!=chapter){chapter=ref.chapter;rows_.push_back({ref,true});}
        rows_.push_back({ref,false});
    }
    cache_.clear();order_.clear();offset_=target_=0;wheel_timer_.Stop();
    heights_.clear();
    for(const auto& row:rows_)heights_.push_back(row.heading?FromDIP(124):FromDIP(150));
    rebuild_positions();center_passage();Refresh(false);
}
void ScriptureView::center_passage() {
    prepare_visible();
    auto it=std::find_if(rows_.begin(),rows_.end(),[this](auto& r){
        if(r.heading || r.ref.chapter!=displayed_.first.chapter || r.ref>displayed_.first)return false;
        const auto text=corpus_.verse(base_source_,displayed_.book,r.ref);
        return text && displayed_.first<=text->last.value_or(text->ref);
    });
    if(it==rows_.end())return;
    wheel_timer_.Stop();const auto index=std::size_t(it-rows_.begin());
    const auto begin=index>2?index-2:0;
    for(auto i=begin;i<=index;++i)heights_[i]=row_layout(i).height;
    rebuild_positions();
    const bool fits=positions_[index+1]-positions_[begin]<GetClientSize().y;
    const auto start=fits?begin:(index&&rows_[index-1].heading?index-1:index);
    set_position(positions_[start]);target_=offset_;
}
void ScriptureView::apply(const Settings& settings) {
    settings_=settings; const auto colors=palette(settings.theme);
    SetBackgroundColour(colors.paper);SetForegroundColour(colors.ink);invalidate();
}
void ScriptureView::invalidate() {
    const auto anchor=first_visible();
    const double fraction=rows_.empty()?0:(offset_-positions_[anchor])/std::max(1,heights_[anchor]);
    cache_.clear();order_.clear();layout_width_=GetClientSize().x;
    for(std::size_t i=0;i<rows_.size();++i)heights_[i]=rows_[i].heading?FromDIP(124):FromDIP(150);
    rebuild_positions();
    if(!rows_.empty()){heights_[anchor]=row_layout(anchor).height;rebuild_positions();offset_=positions_[anchor]+fraction*heights_[anchor];target_=offset_;}
    prepare_visible();scrollbar();Refresh(false);
}
const ScriptureView::Layout& ScriptureView::row_layout(std::size_t index) const {
    if(auto it=cache_.find(index);it!=cache_.end())return it->second;
    wxClientDC dc(const_cast<ScriptureView*>(this));dc.SetFont(body_font(settings_.font_size));
    Layout layout;
    if(rows_[index].heading)layout.height=FromDIP(124);
    else {
        const auto selected_languages=languages();
        const bool stacked=(GetClientSize().x-2*outside_margin())/columns_count()<270;
        int height=0;
        for(const auto& language:selected_languages) {
            const auto source=language==selected_languages.front()?base_source_:source_for_language(language,displayed_.book);
            auto verse=corpus_.parallel_verse(base_source_,source,displayed_.book,rows_[index].ref);
            Column column{language,verse?verse->ref:rows_[index].ref,verse?verse->last:std::nullopt,layout_paragraph(dc,u(verse?verse->text:verse.error()),std::max(80,column_width()),language),!verse};
            const int h=column.text.height()+(stacked&&selected_languages.size()>1?FromDIP(24):0);
            height=stacked?height+h:std::max(height,h);
            layout.columns.push_back(std::move(column));
        }
        layout.height=height+FromDIP(22);
    }
    while(cache_.size()>=cache_limit){cache_.erase(order_.front());order_.pop_front();}
    order_.push_back(index);
    return cache_.emplace(index,std::move(layout)).first->second;
}
void ScriptureView::draw(wxDC& dc,wxSize size,std::size_t begin,std::size_t end,double top) const {
    const auto colors=palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));dc.Clear();
    const int margin=outside_margin(), usable=size.x-2*margin;
    const bool stacked=usable/columns_count()<270;
    const int stride=(usable-44*(columns_count()-1))/columns_count()+44;
    int y=int(std::lround(top))+FromDIP(12);
    for(std::size_t index=begin;index<end && y<size.y;++index) {
        const auto& row=rows_[index];const auto& layout=row_layout(index);
        if(row.heading) {
            dc.SetTextForeground(colors.muted);dc.SetFont(ui_font(11));
            auto name=u(corpus_.book_name(displayed_.book)).Upper();
            const auto extent=dc.GetTextExtent(name);dc.DrawText(name,(size.x-extent.x)/2,y+FromDIP(15));
            dc.SetFont(body_font(28));dc.SetTextForeground(colors.ink);
            const wxString title=wxString::Format("%d",row.ref.chapter);
            dc.DrawText(title,(size.x-dc.GetTextExtent(title).x)/2,y+FromDIP(42));
            dc.SetFont(ui_font(9));dc.SetTextForeground(colors.muted);
            wxString labels;
            for(const auto& language:languages()) {
                if(!labels.empty())labels+="     ·     ";
                const auto source=language==languages().front()?base_source_:source_for_language(language,displayed_.book);
                labels+=language=="sv"?"SVENSKA 1917":language=="en"?(source=="en-web"?"WORLD ENGLISH BIBLE":"KING JAMES"):"ΕΛΛΗΝΙΚΑ";
            }
            if(displayed_.book=="Ps" && !settings_.parallel.empty() && settings_.parallel!="en") {
                const auto map=corpus_.alignment(base_source_,"grc-lxx",{displayed_.book,row.ref,row.ref});
                if(map&&!map->to.book.empty())labels+=wxString::Format("     ·     MT %d / LXX %d",row.ref.chapter,map->to.first.chapter);
            }
            dc.DrawText(labels,(size.x-dc.GetTextExtent(labels).x)/2,y+FromDIP(93));
        } else {
            if(columns_count()>1&&!stacked) {
                dc.SetPen(wxPen(colors.rule,1));
                for(int c=1;c<columns_count();++c){const int gutter=margin+c*stride-FromDIP(24);dc.DrawLine(gutter,y,gutter,y+layout.height);}
            }
            const auto last=layout.columns.empty()?row.ref:layout.columns.front().last.value_or(row.ref);
            const auto segments=reading_.segments();
            const bool prescribed=std::any_of(segments.begin(),segments.end(),[&](const auto& p){return p.book==displayed_.book&&p.first<=last&&row.ref<=p.last;});
            if(prescribed) {
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
                wxString number=wxString::Format("%d",column.ref.verse)+u(column.ref.suffix);
                if(column.last)number+="–"+wxString::Format("%d",column.last->verse)+u(column.last->suffix);
                dc.DrawText(number,x,text_y+FromDIP(6));
                dc.SetFont(body_font(settings_.font_size));dc.SetTextForeground(column.missing?colors.muted:colors.ink);
                draw_paragraph(dc,column.text,x+FromDIP(30),text_y);text_y+=column.text.height();
                if(stacked)stacked_y=text_y;
            }
        }
        y+=layout.height;
    }

}
void ScriptureView::paint(wxPaintEvent&) {
    prepare_visible();scrollbar();wxAutoBufferedPaintDC dc(this);
    const auto begin=first_visible();draw(dc,GetClientSize(),begin,rows_.size(),positions_.empty()?0:positions_[begin]-offset_);
}
void ScriptureView::render_to(wxDC& dc,wxSize size) { prepare_visible();const auto begin=first_visible();draw(dc,size,begin,rows_.size(),positions_.empty()?0:positions_[begin]-offset_); }
}
