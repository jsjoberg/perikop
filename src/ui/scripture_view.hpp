#pragma once
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/theme.hpp"
#include "speech/speech.hpp"
#include <wx/panel.h>
#include <wx/timer.h>
#include <memory>
#include <map>
#include <list>
namespace ortho {
class ScriptureView final : public wxPanel {
public:
    ScriptureView(wxWindow*,const CorpusDb&);
    void open(const Reading&);
    void open_section(std::size_t index);
    void apply(const Settings&);
    void center_passage();
    const Reading& reading() const { return reading_; }
    std::size_t cached_rows() const { return cache_.size(); }
    void render_to(wxDC&,wxSize size);
    void scroll_by(double pixels);
    double scroll_position() const { return offset_; }
    void playback(const SpeechPlayback&);
    void follow_playback(bool follow=true);
    bool follows_playback() const {return following_;}
    void on_release_follow(std::function<void()> callback){release_follow_=std::move(callback);}
    void advance_playback(double seconds);
    std::optional<double> marker_position() const {return guide_?std::optional<double>{guide_y_}:std::nullopt;}
    // Verses marked by dragging in the left pane, as a passage in its edition.
    std::optional<Passage> selection() const;
    const std::string& base_source() const {return base_source_;}
    void clear_selection();
    void select_verses(VerseRef first,VerseRef last);
    void on_selection(std::function<void()> callback){selection_changed_=std::move(callback);}
    // The left-pane verse under a point in client coordinates.
    std::optional<VerseRef> verse_at(wxPoint) const;
    ~ScriptureView() override;
private:
    struct Row { VerseRef ref; bool heading; std::vector<VerseRef> verses; VerseRef last; };
    struct Column { std::string language,source; TextLayout text; bool missing=false; std::vector<bool> prescribed; std::vector<std::pair<VerseRef,VerseRef>> verses; };
    struct Layout { std::vector<Column> columns; int height=0; };
    void set_position(double);
    void rebuild_positions();
    void prepare_visible();
    std::size_t first_visible() const;
    void scrollbar();
    const Layout& row_layout(std::size_t row) const;
    void paint(wxPaintEvent&);
    void draw(wxDC&,wxSize,std::size_t begin,std::size_t end,double top) const;
    void invalidate();
    void release_follow();
    void locate_playback();
    struct GuideSpan {size_t line;double first,last,weight;};
    struct Guide {size_t row,column,line;double y;int height;std::vector<GuideSpan> spans;};
    std::optional<Guide> guide_;
    SpeechPlayback playback_;
    std::optional<SpeechCue> located_cue_;
    std::optional<size_t> speech_row_;
    double guide_y_=0,guide_alpha_=0,follow_target_=0;
    bool following_=false;
    std::function<void()> release_follow_,selection_changed_;
    std::optional<VerseRef> drag_anchor_;
    std::optional<std::pair<VerseRef,VerseRef>> selection_;
    bool dragged_=false;
    wxTimer follow_timer_;
    std::vector<std::string> languages() const;
    int columns_count() const;
    int outside_margin() const;
    int column_width() const;
    std::vector<int> heights_;
    std::vector<double> positions_;
    double offset_=0,target_=0;
    int layout_width_=0;
    wxTimer wheel_timer_;
    void* native_scroll_=nullptr;
    const CorpusDb& corpus_;
    Reading reading_{ReadingKind::MorningPsalm,{"Ps",{23,1},{23,6}},"Psalm 23"};
    Settings settings_;
    Passage displayed_{"Ps",{23,1},{23,6}};
    std::string base_source_="sv1917";
    std::vector<Row> rows_;
    mutable std::map<std::size_t,Layout> cache_;
    mutable std::list<std::size_t> order_;
    static constexpr std::size_t cache_limit=192;
};
}
