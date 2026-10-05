#pragma once
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/theme.hpp"
#include <wx/vscroll.h>
#include <map>
#include <list>
namespace ortho {
class ScriptureView final : public wxVScrolledWindow {
public:
    ScriptureView(wxWindow*,const CorpusDb&);
    void open(const Reading&);
    void apply(const Settings&);
    void center_passage();
    const Reading& reading() const { return reading_; }
    std::size_t cached_rows() const { return cache_.size(); }
    void render_to(wxDC&,wxSize size);
private:
    struct Row { VerseRef ref; bool heading; };
    struct Column { std::string language; VerseRef ref; std::optional<VerseRef> last; TextLayout text; bool missing=false; };
    struct Layout { std::vector<Column> columns; int height=0; };
    wxCoord OnGetRowHeight(std::size_t row) const override;
    const Layout& row_layout(std::size_t row) const;
    void paint(wxPaintEvent&);
    void draw(wxDC&,wxSize,std::size_t begin,std::size_t end) const;
    void invalidate();
    int columns_count() const;
    int outside_margin() const;
    int column_width() const;
    const CorpusDb& corpus_;
    Reading reading_{ReadingKind::MorningPsalm,{"Ps",{23,1},{23,6}},"Psalm 23"};
    Settings settings_;
    std::vector<Row> rows_;
    mutable std::map<std::size_t,Layout> cache_;
    mutable std::list<std::size_t> order_;
    static constexpr std::size_t cache_limit=192;
};
}
