#include "ui/scripture_view.hpp"
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#ifdef __APPLE__
#include "ui/native_scroll.hpp"
#endif
namespace ortho {
namespace { wxString u(const std::string& s){return wxString::FromUTF8(s);} }
ScriptureView::ScriptureView(wxWindow* parent,const CorpusDb& corpus)
    :wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE|wxWANTS_CHARS|wxVSCROLL),follow_timer_(this),wheel_timer_(this),corpus_(corpus) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);SetName("Kontinuerlig skriftläsare");
    Bind(wxEVT_PAINT,&ScriptureView::paint,this);
    Bind(wxEVT_SIZE,[this](wxSizeEvent& e){if(layout_width_!=GetClientSize().x)invalidate();scrollbar();e.Skip();});
    Bind(wxEVT_MOUSEWHEEL,[this](wxMouseEvent& e){
        if(e.GetWheelAxis()!=wxMOUSE_WHEEL_VERTICAL){e.Skip();return;}
        release_follow();
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
    },wheel_timer_.GetId());
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){advance_playback(0.016);},follow_timer_.GetId());
    const auto scroll_event=[this](wxScrollWinEvent& e){
        release_follow();
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
        if(key==WXK_HOME){release_follow();set_position(0);return;}
        if(key==WXK_END){release_follow();set_position(positions_.empty()?0:positions_.back());return;}
        if(key==WXK_DOWN){scroll_by(FromDIP(30));return;}
        if(key==WXK_UP){scroll_by(-FromDIP(30));return;}
        if(key==WXK_PAGEDOWN){scroll_by(GetClientSize().y*0.85);return;}
        if(key==WXK_PAGEUP){scroll_by(-GetClientSize().y*0.85);return;}
        e.Skip();
    });
    // Dragging across verses marks them; a plain click clears the mark.
    Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& e){
        SetFocus();drag_anchor_=verse_at(e.GetPosition());dragged_=false;
        if(drag_anchor_&&!HasCapture())CaptureMouse();
    });
    Bind(wxEVT_MOTION,[this](wxMouseEvent& e){
        if(!drag_anchor_||!e.LeftIsDown())return;
        const auto verse=verse_at(e.GetPosition());
        if(!verse||(!dragged_&&*verse==*drag_anchor_&&!selection_))return;
        dragged_=true;select_verses(*drag_anchor_,*verse);
    });
    Bind(wxEVT_LEFT_UP,[this](wxMouseEvent& e){
        if(HasCapture())ReleaseMouse();
        if(drag_anchor_&&!dragged_) {
            if(selection_)clear_selection();
            else if(word_clicked_)if(auto word=word_at(e.GetPosition()))word_clicked_(*word);
        }
        drag_anchor_.reset();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST,[this](wxMouseCaptureLostEvent&){drag_anchor_.reset();});
#ifdef __APPLE__
    native_scroll_=install_native_scroll(this,[this](double pixels){scroll_by(pixels);});
#endif
}
std::optional<Passage> ScriptureView::selection() const {
    if(!selection_)return std::nullopt;
    const auto last=corpus_.verse(frame_,displayed_.book,selection_->second);
    return Passage{displayed_.book,selection_->first,last?last->last.value_or(selection_->second):selection_->second};
}
void ScriptureView::select_verses(VerseRef first,VerseRef last) {
    if(last<first)std::swap(first,last);
    if(selection_==std::pair{first,last})return;
    selection_=std::pair{first,last};Refresh(false);
    if(selection_changed_)selection_changed_();
}
void ScriptureView::clear_selection() {
    if(!selection_)return;
    selection_.reset();Refresh(false);
    if(selection_changed_)selection_changed_();
}
std::optional<VerseRef> ScriptureView::verse_at(wxPoint point) const {
    const auto found=hit(point);
    if(!found||found->run<0)return std::nullopt;
    const auto& column=*found->column;
    return column.verses[column.text.lines[found->line].runs[found->run].tag].first;
}
namespace {
// Letters of a run, without punctuation, quotes, digits or a hyphenation mark.
wxString word_letters(const wxString& text) {
    wxString result;
    for(const auto c:text) {
        const auto code=c.GetValue();
        const bool ascii_letter=(code>='a'&&code<='z')||(code>='A'&&code<='Z');
        const bool other=code>=0xc0&&code!=0xd7&&code!=0xf7&&code!=0x2013&&code!=0x2014&&code!=0x2018&&code!=0x2019&&
            code!=0x201c&&code!=0x201d&&code!=0x2026&&code!=0x0387&&code!=0x037e&&code!=0x00ab&&code!=0x00bb;
        if(ascii_letter||other)result+=c;
    }
    return result;
}
}
std::vector<ScriptureView::WordRuns> ScriptureView::words(const Column& column) {
    std::vector<WordRuns> result;bool joining=false;
    for(std::size_t line=0;line<column.text.lines.size();++line) {
        const auto& runs=column.text.lines[line].runs;
        for(std::size_t run=0;run<runs.size();++run) {
            if(runs[run].marker||runs[run].tag<0)continue;
            const auto letters=word_letters(runs[run].text);
            if(joining&&!result.empty()){result.back().text+=letters;result.back().runs.emplace_back(line,run);joining=false;continue;}
            joining=false;
            if(letters.empty())continue;
            result.push_back({runs[run].tag,letters,{{line,run}}});
        }
        joining=column.text.lines[line].hyphenated&&!result.empty();
    }
    return result;
}
std::optional<std::pair<ScriptureView::WordRuns,int>> ScriptureView::word_of(const Column& column,int tag,std::size_t line,int run) const {
    std::map<wxString,int> seen;
    for(auto& word:words(column)) {
        if(word.tag!=tag)continue;
        const int occurrence=seen[word.text.Lower()]++;
        for(const auto& [l,r]:word.runs)if(l==line&&int(r)==run)return std::pair{std::move(word),occurrence};
    }
    return std::nullopt;
}
std::optional<ScriptureView::Word> ScriptureView::word_at(wxPoint point) const {
    const auto found=hit(point);
    if(!found||found->run<0||found->distance>FromDIP(4))return std::nullopt;
    const auto& column=*found->column;const int tag=column.text.lines[found->line].runs[found->run].tag;
    const auto word=word_of(column,tag,found->line,found->run);
    if(!word)return std::nullopt;
    const auto utf8=word->first.text.ToUTF8();
    return Word{displayed_.book,std::string(utf8.data(),utf8.length()),column.verses[tag].first,word->second};
}
void ScriptureView::highlight_word(std::optional<Word> word) {
    highlighted_=std::move(word);Refresh(false);
}
std::optional<ScriptureView::Hit> ScriptureView::hit(wxPoint point) const {
    if(rows_.empty()||positions_.empty())return std::nullopt;
    // Rows are drawn from positions_ shifted by the scroll offset and a top inset.
    const double y=point.y+offset_-FromDIP(12);
    auto it=std::upper_bound(positions_.begin(),positions_.end(),y);
    if(it==positions_.begin())return std::nullopt;
    const auto index=std::size_t(it-positions_.begin()-1);
    if(index>=rows_.size()||rows_[index].heading)return std::nullopt;
    const auto& layout=row_layout(index);
    if(layout.columns.empty())return std::nullopt;
    const int margin=outside_margin(),usable=GetClientSize().x-2*margin;
    const bool stacked=usable/columns_count()<270;
    const int stride=(usable-44*(columns_count()-1))/columns_count()+44;
    if(!stacked&&columns_count()>1&&point.x>=margin+stride-FromDIP(24))return std::nullopt;
    const auto& column=layout.columns.front();
    double top=positions_[index];
    if(stacked&&layout.columns.size()>1)top+=FromDIP(24);
    if(y<top||y>=top+column.text.height())return std::nullopt;
    const auto line=std::min(column.text.lines.size()-1,std::size_t((y-top)/column.text.line_height));
    const double x=point.x-margin-FromDIP(30);
    // The run under the pointer, or the nearest one on that line.
    int nearest=-1;double best=std::numeric_limits<double>::max();
    const auto& runs=column.text.lines[line].runs;
    for(std::size_t i=0;i<runs.size();++i) {
        const auto& run=runs[i];
        if(run.tag<0||size_t(run.tag)>=column.verses.size()||column.hebrew[run.tag])continue;
        const double distance=x<run.x?run.x-x:x>run.x+run.width?x-run.x-run.width:0;
        if(distance<best){best=distance;nearest=int(i);}
    }
    return Hit{index,&column,line,nearest,best};
}
ScriptureView::~ScriptureView(){wheel_timer_.Stop();follow_timer_.Stop();
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
void ScriptureView::scroll_by(double pixels){release_follow();set_position(offset_+pixels);target_=offset_;}
void ScriptureView::release_follow() {
    if(!following_)return;
    following_=false;follow_target_=offset_;
    if(release_follow_)release_follow_();
}
void ScriptureView::follow_playback(bool follow) {
    following_=follow;follow_target_=offset_;
    if(follow){
        wheel_timer_.Stop();locate_playback();
        if(guide_) {
            follow_target_=guide_->y-GetClientSize().y*0.38;
            if(playback_.state==SpeechState::Paused)set_position(follow_target_);
        }
        if(!follow_timer_.IsRunning())follow_timer_.Start(16);
    }
}
void ScriptureView::playback(const SpeechPlayback& playback) {
    const bool was_visible=bool(guide_);
    const bool cue_changed=playback_.cue!=playback.cue;
    playback_=playback;
    const bool active=playback.state==SpeechState::Playing||playback.state==SpeechState::Paused||playback.state==SpeechState::Buffering||playback.state==SpeechState::Loading;
    if(active) {
        locate_playback();
        if(guide_&&(!was_visible||(cue_changed&&playback.state==SpeechState::Paused)))guide_y_=guide_->y;
    }
    if((guide_||guide_alpha_>0)&&!follow_timer_.IsRunning())follow_timer_.Start(16);
    if(guide_||was_visible)Refresh(false);
}
void ScriptureView::locate_playback() {
    if(!playback_.cue||playback_.cue->introduction||playback_.cue->book!=displayed_.book){guide_.reset();return;}
    const auto& cue=*playback_.cue;
    if(located_cue_!=playback_.cue) {
        located_cue_=playback_.cue;speech_row_.reset();
        VerseRef first=cue.verse,last=cue.last;
        if(cue.source!=frame_) {
            const auto mapped=corpus_.map_passage(cue.source,frame_,{cue.book,first,last});
            if(mapped.empty()){guide_.reset();return;}
            first=mapped.front().first;last=mapped.front().last;
        }
        speech_range_={first,last};
        for(size_t i=0;i<rows_.size();++i)if(!rows_[i].heading&&rows_[i].ref<=last&&first<=rows_[i].last){speech_row_=i;break;}
    }
    if(!speech_row_){guide_.reset();return;}
    const auto index=*speech_row_;
    const auto anchor=first_visible();const double old=positions_[anchor];
    bool changed=false;
    for(size_t i=index>2?index-2:0;i<=std::min(index+2,rows_.size()-1);++i) {
        const auto height=row_layout(i).height;changed|=heights_[i]!=height;heights_[i]=height;
    }
    if(changed){rebuild_positions();const auto adjustment=positions_[anchor]-old;offset_+=adjustment;target_+=adjustment;follow_target_+=adjustment;guide_y_+=adjustment;}
    const auto& layout=row_layout(index);
    const bool stacked=(GetClientSize().x-2*outside_margin())/columns_count()<270;
    double y=positions_[index]+FromDIP(12);
    for(size_t c=0;c<layout.columns.size();++c) {
        const auto& column=layout.columns[c];
        if(stacked&&layout.columns.size()>1)y+=FromDIP(24);
        if(c==0) {
            Guide guide{index,c,0,y,column.text.line_height,{}};
            double weight=0;
            for(size_t line=0;line<column.text.lines.size();++line) {
                GuideSpan span{line,std::numeric_limits<double>::max(),0,0};
                for(const auto& run:column.text.lines[line].runs) {
                    if(run.tag<0||size_t(run.tag)>=column.verses.size()||column.hebrew[run.tag])continue;
                    const auto& [first,last]=column.verses[run.tag];
                    if(speech_range_.first>last||speech_range_.second<first)continue;
                    span.first=std::min(span.first,run.x);span.last=std::max(span.last,run.x+run.width);
                    if(!run.marker)span.weight+=speech_text_weight(run.text.ToStdString(wxConvUTF8));
                }
                if(span.last>span.first){weight+=span.weight;guide.spans.push_back(span);}
            }
            if(guide.spans.empty()){guide_.reset();return;}
            double remaining=std::clamp(playback_.verse_progress,0.0,1.0)*weight;
            guide.line=guide.spans.back().line;
            for(const auto& span:guide.spans){if(remaining<span.weight){guide.line=span.line;break;}remaining-=span.weight;}
            guide.y=y+(guide.line+0.5)*guide.height;
            guide_=std::move(guide);return;
        }
        if(stacked)y+=column.text.height();
    }
    guide_.reset();
}
void ScriptureView::advance_playback(double seconds) {
    const bool active=playback_.state==SpeechState::Playing||playback_.state==SpeechState::Paused||playback_.state==SpeechState::Buffering||playback_.state==SpeechState::Loading;
    const auto ease=[seconds](double rate){return 1-std::exp(-std::clamp(seconds,0.0,0.1)*rate);};
    if(active)locate_playback();
    guide_alpha_+=((active&&guide_?1.0:0.0)-guide_alpha_)*ease(14);
    // The marker and page hold still during pause and a genuine audio underrun.
    if(guide_&&playback_.state==SpeechState::Playing) {
        guide_y_+=(guide_->y-guide_y_)*ease(16);
        if(following_) {
            const double height=GetClientSize().y,screen=guide_->y-offset_;
            if(screen>height*0.56||screen<height*0.22)follow_target_=guide_->y-height*0.38;
            const double limit=std::max(0.0,positions_.back()-height);
            follow_target_=std::clamp(follow_target_,0.0,limit);
            if(std::abs(follow_target_-offset_)>0.1){set_position(offset_+(follow_target_-offset_)*ease(8));target_=offset_;}
        }
    }
    if(guide_alpha_<0.01&&(!active||!guide_)){guide_.reset();guide_alpha_=0;follow_timer_.Stop();}
    Refresh(false);
}
std::vector<std::string> ScriptureView::languages() const {
    std::vector<std::string> result{base_source_.starts_with("sv")?"sv":base_source_.starts_with("en-")?"en":"el"};
    const auto add=[&](const std::string& language){if(std::find(result.begin(),result.end(),language)==result.end())result.push_back(language);};
    if(!settings_.parallel.empty())add(settings_.parallel);
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
    displayed_=segments[index];rows_.clear();located_cue_.reset();highlighted_.reset();speech_row_.reset();guide_.reset();guide_alpha_=0;orphans_.clear();
    if(selection_){selection_.reset();if(selection_changed_)selection_changed_();}
    // The Septuagint frames the Old Testament: its books, order and numbers.
    frame_=reading_.source_override.empty()?frame_source(reading_.base_language,displayed_.book):reading_.source_override;
    if(corpus_.coordinates(frame_,displayed_.book).empty())for(const std::string language:{"sv","el","en"}) {
        const auto source=source_for_language(language,displayed_.book);
        if(!corpus_.coordinates(source,displayed_.book).empty()){frame_=source;break;}
    }
    canon_=canon_book(displayed_.book,displayed_.first.chapter);
    base_source_=reading_.source_override.empty()?source_for_language(reading_.base_language,canon_code()):reading_.source_override;
    // Paragraphs follow the left pane's edition, placed at its framing verses.
    std::set<VerseRef> boundaries;
    if(base_source_==frame_){for(auto ref:corpus_.paragraph_starts(frame_,displayed_.book))boundaries.insert(ref);}
    else for(auto ref:corpus_.paragraph_starts(base_source_,canon_code()))
        for(const auto& [book,target]:corpus_.counterparts(base_source_,frame_,canon_code(),ref))if(book==displayed_.book){boundaries.insert(target);break;}
    int chapter=0;
    const bool stanza=displayed_.book=="Ps"||displayed_.book=="Ps151"||displayed_.book=="Prov"||displayed_.book=="Song"||displayed_.book=="Lam";
    for(auto ref:corpus_.coordinates(frame_,displayed_.book)) {
        if(canon_&&(ref.chapter<canon_->first_chapter||ref.chapter>canon_->last_chapter))continue;
        const bool new_chapter=ref.chapter!=chapter;
        if(new_chapter){chapter=ref.chapter;rows_.push_back({ref,true,{},ref});}
        if(new_chapter||stanza||boundaries.contains(ref))rows_.push_back({ref,false,{},ref});
        rows_.back().verses.push_back(ref);
        const auto verse=corpus_.verse(frame_,displayed_.book,ref);
        rows_.back().last=verse?verse->last.value_or(ref):ref;
    }
    cache_.clear();order_.clear();offset_=target_=0;wheel_timer_.Stop();
    heights_.clear();
    for(const auto& row:rows_)heights_.push_back(row.heading?FromDIP(124):FromDIP(150));
    rebuild_positions();center_passage();Refresh(false);
}
std::string ScriptureView::source_of(const std::string& language) const {
    return language==languages().front()?base_source_:source_for_language(language,canon_code());
}
const ScriptureView::Orphans& ScriptureView::orphans(const std::string& source) const {
    if(auto it=orphans_.find(source);it!=orphans_.end())return it->second;
    auto& result=orphans_[source];
    if(source==frame_||frame_!="grc-lxx")return result;
    // The edition's books that this framing book draws on, such as Swedish Nehemiah.
    std::set<std::string> books;
    for(const auto& row:rows_)if(!row.heading)for(const auto& ref:row.verses)
        for(const auto& [book,target]:corpus_.counterparts(frame_,source,displayed_.book,ref)){(void)target;books.insert(book);}
    for(const auto& book:books) {
        std::optional<VerseRef> anchor;std::vector<VerseRef> pending;
        for(const auto& ref:corpus_.coordinates(source,book)) {
            const auto targets=corpus_.counterparts(source,frame_,book,ref);
            if(targets.empty()) {
                if(anchor)result[*anchor].emplace_back(book,ref);else pending.push_back(ref);
                continue;
            }
            const auto here=std::find_if(targets.begin(),targets.end(),[&](const auto& target){
                return target.first==displayed_.book&&(!canon_||(canon_->first_chapter<=target.second.chapter&&target.second.chapter<=canon_->last_chapter));});
            if(here==targets.end())continue;
            anchor=here->second;
            for(const auto& waiting:pending)result[*anchor].emplace_back(book,waiting);
            pending.clear();
        }
    }
    return result;
}
void ScriptureView::center_passage() {
    prepare_visible();
    auto it=std::find_if(rows_.begin(),rows_.end(),[this](auto& r){
        return !r.heading && r.ref<=displayed_.first && displayed_.first<=r.last;
    });
    if(it==rows_.end())return;
    wheel_timer_.Stop();const auto index=std::size_t(it-rows_.begin());
    const auto begin=index>2?index-2:0;
    for(auto i=begin;i<=index;++i)heights_[i]=row_layout(i).height;
    rebuild_positions();
    const bool fits=positions_[index+1]-positions_[begin]<GetClientSize().y;
    const auto start=fits?begin:(index&&rows_[index-1].heading?index-1:index);
    double position=positions_[start];
    const auto& paragraph=row_layout(index);
    const auto verse_it=std::find_if(rows_[index].verses.begin(),rows_[index].verses.end(),[&](auto ref){auto v=corpus_.verse(frame_,displayed_.book,ref);return ref<=displayed_.first && v && displayed_.first<=v->last.value_or(ref);});
    const int tag=int(verse_it-rows_[index].verses.begin());
    if(!paragraph.columns.empty()) {
        const auto& text=paragraph.columns.front().text;
        for(std::size_t line=0;line<text.lines.size();++line)if(std::any_of(text.lines[line].runs.begin(),text.lines[line].runs.end(),[&](const auto& run){return run.tag==tag;})) {
            if(line>2)position=positions_[index]+(line-2)*text.line_height;
            break;
        }
    }
    set_position(position);target_=offset_;
}
void ScriptureView::apply(const Settings& settings) {
    settings_=settings; const auto colors=palette(settings.theme);
    SetBackgroundColour(colors.paper);SetForegroundColour(colors.ink);invalidate();
}
void ScriptureView::invalidate() {
    const double marker_screen=guide_y_-offset_;
    const auto anchor=first_visible();
    const double fraction=rows_.empty()?0:(offset_-positions_[anchor])/std::max(1,heights_[anchor]);
    cache_.clear();order_.clear();layout_width_=GetClientSize().x;
    for(std::size_t i=0;i<rows_.size();++i)heights_[i]=rows_[i].heading?FromDIP(124):FromDIP(150);
    rebuild_positions();
    if(!rows_.empty()){heights_[anchor]=row_layout(anchor).height;rebuild_positions();offset_=positions_[anchor]+fraction*heights_[anchor];target_=offset_;}
    prepare_visible();scrollbar();Refresh(false);
    guide_y_=offset_+marker_screen;follow_target_=offset_;
    if(playback_.state==SpeechState::Paused||playback_.state==SpeechState::Buffering){locate_playback();if(guide_)guide_y_=guide_->y;}
}
const ScriptureView::Layout& ScriptureView::row_layout(std::size_t index) const {
    if(auto it=cache_.find(index);it!=cache_.end())return it->second;
    wxClientDC dc(const_cast<ScriptureView*>(this));dc.SetFont(body_font(settings_.font_size));
    Layout layout;
    if(rows_[index].heading)layout.height=FromDIP(124);
    else {
        const auto selected_languages=languages();
        const bool stacked=(GetClientSize().x-2*outside_margin())/columns_count()<270;
        const int offset=canon_?canon_->offset():0;
        const auto segments=reading_.segments();
        int height=0;
        for(const auto& language:selected_languages) {
            const auto source=source_of(language);
            Column column;column.language=language;column.source=source;
            std::vector<TextFragment> fragments;
            std::optional<VerseRef> previous;
            const auto add=[&](const wxString& text,const wxString& label,VerseRef first,VerseRef last,bool faint,bool hebrew) {
                column.verses.emplace_back(first,last);column.faint.push_back(faint);column.hebrew.push_back(hebrew);
                column.prescribed.push_back(!hebrew&&std::any_of(segments.begin(),segments.end(),[&](const auto& p){return p.book==displayed_.book&&p.first<=last&&first<=p.last;}));
                fragments.push_back({text,label,int(column.verses.size()-1)});
            };
            for(const auto& ref:rows_[index].verses) {
                const auto framing=corpus_.verse(frame_,displayed_.book,ref);
                const auto last=framing?framing->last.value_or(ref):ref;
                auto verse=corpus_.parallel_verse(frame_,source,displayed_.book,ref);
                // One verse of this edition can span several framing verses.
                if((verse&&previous&&verse->ref==*previous)||(!verse&&verse.error()=="Ingår i föregående vers"&&!column.verses.empty())) {
                    column.verses.back().second=last;continue;
                }
                if(verse)previous=verse->ref;
                // The Septuagint's number, and this edition's own where it differs.
                wxString number=wxString::Format("%d",ref.verse)+u(ref.suffix);
                if(verse&&source!=frame_) {
                    const auto& own=verse->ref;
                    if(own.chapter!=ref.chapter-offset||own.verse!=ref.verse||own.suffix!=ref.suffix) {
                        wxString mark=(own.chapter!=ref.chapter-offset?wxString::Format("%d:",own.chapter):wxString())+wxString::Format("%d",own.verse)+u(own.suffix);
                        if(verse->last)mark+="–"+wxString::Format("%d",verse->last->verse)+u(verse->last->suffix);
                        number+=" ("+mark+")";
                    }
                }
                add(u(verse?verse->text:verse.error()),number,ref,last,!verse,false);
                if(source!=frame_)if(auto extra=orphans(source).find(ref);extra!=orphans(source).end())
                    for(const auto& [book,own]:extra->second)if(const auto text=corpus_.verse(source,book,own))
                        add(u(text->text),wxString::Format("hebr. %d:%d",own.chapter,own.verse)+u(own.suffix),ref,ref,true,true);
            }
            column.text=layout_paragraph(dc,fragments,std::max(80,column_width()),language);
            const int h=column.text.height()+(stacked&&selected_languages.size()>1?FromDIP(24):0);
            height=stacked?height+h:std::max(height,h);
            layout.columns.push_back(std::move(column));
        }
        layout.height=height+FromDIP(14);
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
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsRenderer::GetDefaultRenderer()->CreateContextFromUnknownDC(dc));
    int y=int(std::lround(top))+FromDIP(12);
    for(std::size_t index=begin;index<end && y<size.y;++index) {
        const auto& row=rows_[index];const auto& layout=row_layout(index);
        if(row.heading) {
            dc.SetTextForeground(colors.muted);dc.SetFont(ui_font(11));
            auto name=u(corpus_.book_name(canon_code())).Upper();
            const auto extent=dc.GetTextExtent(name);dc.DrawText(name,(size.x-extent.x)/2,y+FromDIP(15));
            dc.SetFont(body_font(28));dc.SetTextForeground(colors.ink);
            const wxString title=wxString::Format("%d",row.ref.chapter-(canon_?canon_->offset():0));
            dc.DrawText(title,(size.x-dc.GetTextExtent(title).x)/2,y+FromDIP(42));
            dc.SetFont(ui_font(9));dc.SetTextForeground(colors.muted);
            wxString labels;
            for(const auto& language:languages()) {
                if(!labels.empty())labels+="     ·     ";
                const auto source=source_of(language);
                labels+=language=="sv"?"SVENSKA 1917":language=="en"?(source=="en-web"?"WORLD ENGLISH BIBLE":"KING JAMES"):"ΕΛΛΗΝΙΚΑ";
            }
            dc.DrawText(labels,(size.x-dc.GetTextExtent(labels).x)/2,y+FromDIP(93));
        } else {
            if(columns_count()>1&&!stacked) {
                dc.SetPen(wxPen(colors.rule,1));
                for(int c=1;c<columns_count();++c){const int gutter=margin+c*stride-FromDIP(24);dc.DrawLine(gutter,y,gutter,y+layout.height);}
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
                dc.SetFont(body_font(settings_.font_size));dc.SetTextForeground(colors.ink);
                if(gc&&guide_&&guide_->row==index&&guide_->column==c&&guide_alpha_>0.01) {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    for(const auto& span:guide_->spans) {
                        const double center=positions_[index]+text_y-y+FromDIP(12)+(span.line+0.5)*column.text.line_height;
                        const double emphasis=std::clamp(1-std::abs(center-guide_y_)/column.text.line_height,0.0,1.0);
                        gc->SetBrush(wxBrush(wxColour(colors.accent.Red(),colors.accent.Green(),colors.accent.Blue(),int((16+20*emphasis)*guide_alpha_))));
                        gc->DrawRoundedRectangle(x+FromDIP(30)+span.first-FromDIP(5),text_y+span.line*column.text.line_height+FromDIP(1),span.last-span.first+FromDIP(10),column.text.line_height-FromDIP(2),FromDIP(4));
                    }
                    const double marker_y=guide_y_-offset_-guide_->height*0.34;
                    gc->SetBrush(wxBrush(wxColour(colors.accent.Red(),colors.accent.Green(),colors.accent.Blue(),int(230*guide_alpha_))));
                    gc->DrawRoundedRectangle(x+FromDIP(16),marker_y,FromDIP(4),guide_->height*0.68,FromDIP(2));
                }
                if(gc&&c==0&&selection_) {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(wxColour(colors.accent.Red(),colors.accent.Green(),colors.accent.Blue(),56)));
                    for(std::size_t line=0;line<column.text.lines.size();++line) {
                        double first=std::numeric_limits<double>::max(),last=0;
                        for(const auto& run:column.text.lines[line].runs) {
                            if(run.tag<0||size_t(run.tag)>=column.verses.size()||column.hebrew[run.tag])continue;
                            const auto& verse=column.verses[run.tag].first;
                            if(verse<selection_->first||selection_->second<verse)continue;
                            first=std::min(first,run.x);last=std::max(last,run.x+run.width);
                        }
                        if(last>first)gc->DrawRectangle(x+FromDIP(30)+first-FromDIP(3),text_y+line*column.text.line_height,last-first+FromDIP(6),column.text.line_height);
                    }
                }
                // The word shown in the Ordstudium panel.
                if(gc&&c==0&&highlighted_&&highlighted_->book==displayed_.book) {
                    const auto target=wxString::FromUTF8(highlighted_->text).Lower();
                    std::map<wxString,int> seen;
                    for(const auto& word:words(column)) {
                        if(size_t(word.tag)>=column.verses.size()||column.verses[word.tag].first!=highlighted_->verse)continue;
                        const auto lower=word.text.Lower();const int occurrence=seen[lower]++;
                        if(lower!=target||occurrence!=highlighted_->occurrence)continue;
                        gc->SetPen(*wxTRANSPARENT_PEN);
                        gc->SetBrush(wxBrush(wxColour(colors.accent.Red(),colors.accent.Green(),colors.accent.Blue(),72)));
                        for(const auto& [line,index]:word.runs) {
                            const auto& run=column.text.lines[line].runs[index];
                            gc->DrawRoundedRectangle(x+FromDIP(30)+run.x-FromDIP(3),text_y+line*column.text.line_height+FromDIP(2),run.width+FromDIP(6),column.text.line_height-FromDIP(4),FromDIP(4));
                        }
                    }
                }
                draw_paragraph(dc,column.text,x+FromDIP(30),text_y,[&](const TextRun& run)->std::optional<wxColour>{
                    if(run.tag>=0&&size_t(run.tag)<column.faint.size()&&column.faint[run.tag])return colors.muted;
                    return std::nullopt;
                });
                // Mark only the lines containing the prescribed verse range.
                if(c==0) {
                    dc.SetPen(wxPen(colors.accent,FromDIP(3)));
                    for(std::size_t line=0;line<column.text.lines.size();++line) {
                        const auto& runs=column.text.lines[line].runs;
                        if(std::any_of(runs.begin(),runs.end(),[&](const auto& run){return run.tag>=0&&std::size_t(run.tag)<column.prescribed.size()&&column.prescribed[run.tag];})) {
                            const int ly=text_y+int(line)*column.text.line_height;
                            dc.DrawLine(margin-FromDIP(17),ly,margin-FromDIP(17),ly+column.text.line_height);
                        }
                    }
                }
                text_y+=column.text.height();
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
