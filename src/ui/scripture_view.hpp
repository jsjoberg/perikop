#pragma once
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/theme.hpp"
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
    ~ScriptureView() override;
private:
    struct Row { VerseRef ref; bool heading; };
    struct Column { std::string language; VerseRef ref; std::optional<VerseRef> last; TextLayout text; bool missing=false; };
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
