#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/theme.hpp"
#include <array>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <wx/panel.h>
#include <wx/timer.h>
namespace ortho {
class ScriptureView final : public wxPanel {
public:
    ScriptureView(wxWindow*, const CorpusDb&);
    bool Show(bool show = true) override;
    void open(const Reading&);
    void open_section(std::size_t index);
    void apply(const Settings&);
    void center_passage();
    const Reading& reading() const {
        return reading_;
    }
    std::size_t cached_rows() const {
        return cache_.size();
    }
    void render_to(wxDC&, wxSize size, bool cache_text = true);
    struct RenderStats {
        std::size_t layout_builds = 0, tile_builds = 0, tile_hits = 0, repaint_requests = 0;
        std::size_t tile_bytes = 0;
        double fragment_ms = 0, typesetting_ms = 0;
    };
    RenderStats render_stats() const {
        return render_stats_;
    }
    bool animating() const {
        return follow_timer_.IsRunning();
    }
    void scroll_by(double pixels);
    void advance_scroll();
    double scroll_position() const {
        return offset_;
    }
    void restore_scroll(double position) {
        wheel_timer_.Stop();
        set_position(position);
        target_ = offset_;
    }
    VerseRef current_verse() const;
    void show_verse(VerseRef);
    void playback(const SpeechPlayback&);
    void follow_playback(bool follow = true);
    bool follows_playback() const {
        return following_;
    }
    void on_release_follow(std::function<void()> callback) {
        release_follow_ = std::move(callback);
    }
    void advance_playback(double seconds);
    std::optional<double> marker_position() const {
        return guide_ ? std::optional<double>{guide_y_} : std::nullopt;
    }
    // Verses marked by dragging in the left pane, as a passage in its edition.
    std::vector<Passage> selections() const;
    std::optional<Reading> marked_reading() const;
    void on_bookmark(std::function<void(const Reading&)> callback) {
        bookmarked_ = std::move(callback);
    }
    void on_listen(std::function<void(const Reading&)> callback) {
        listen_ = std::move(callback);
    }
    bool verse_selected(VerseRef) const;
    // The left pane's edition, and the edition whose numbering frames the text.
    std::string base_source() const;
    std::string primary_language() const;
    std::vector<std::string> sources_for_chapter(int chapter) const;
    // Overrides belong only to this open passage and chapter, never to saved preferences.
    void chapter_source(int chapter, std::size_t pane, const std::string& source);
    std::string chapter_source(int chapter, std::size_t pane) const;
    void on_source_change(std::function<void(bool primary)> callback) {
        source_changed_ = std::move(callback);
    }
    const std::string& frame() const {
        return frame_;
    }
    int compare_refs(VerseRef a, VerseRef b) const {
        return corpus_.compare(frame_, displayed_.book, a, b);
    }
    bool within_refs(VerseRef ref, VerseRef first, VerseRef last) const {
        return compare_refs(first, ref) <= 0 && compare_refs(ref, last) <= 0;
    }
    bool overlap_refs(VerseRef first, VerseRef last, VerseRef other_first, VerseRef other_last) const {
        return compare_refs(first, other_last) <= 0 && compare_refs(other_first, last) <= 0;
    }
    void clear_selection();
    void select_verses(VerseRef first, VerseRef last, bool append = false);
    void on_selection(std::function<void()> callback) {
        selection_changed_ = std::move(callback);
    }
    // The framing verse under a point in either Scripture column.
    std::optional<VerseRef> verse_at(wxPoint) const;
    std::optional<Passage> highlighted_verse() const;
    // A left-pane word: its framing book and verse, and which occurrence of
    // the same word in that verse it is, counting from 0.
    struct Word {
        std::string book, text;
        VerseRef verse;
        int occurrence = 0;
        bool operator==(const Word&) const = default;
    };
    std::optional<Word> word_at(wxPoint) const;
    // A word-study click; an empty word closes the lookup.
    void on_word(std::function<void(const std::optional<Word>&)> callback) {
        word_clicked_ = std::move(callback);
    }
    void highlight_word(std::optional<Word>);
    const std::optional<Word>& highlighted_word() const {
        return highlighted_;
    }
    // A translucent button floating at the top centre of the text; an empty
    // label removes it. Unless always shown, it appears only while the
    // passage is scrolled out of view.
    void return_button(const wxString& label, bool always);
    void on_return(std::function<void()> callback) {
        returned_ = std::move(callback);
    }
    // Whether the end of the open section's passage has been on screen.
    bool end_seen() const {
        return end_seen_;
    }
    ~ScriptureView() override;

private:
    struct Row {
        VerseRef ref;
        bool heading;
        std::vector<VerseRef> verses;
        VerseRef last;
    };
    // Per tag: the framing verses it shows, and whether it is muted text,
    // either missing from this edition or only in the Hebrew text.
    struct Column {
        std::string language, source;
        TextLayout text;
        std::vector<bool> prescribed, faint, hebrew;
        std::vector<std::pair<VerseRef, VerseRef>> verses;
        // Whether a run's tag is a verse of the main text, not Hebrew-only text.
        bool main_text(int tag) const {
            return tag >= 0 && std::size_t(tag) < verses.size() && !hebrew[tag];
        }
    };
    struct Layout {
        std::vector<Column> columns;
        int height = 0;
        std::list<std::size_t>::iterator recency;
    };
    // The runs of one word, which a hyphenated line break can split in two.
    struct WordRuns {
        int tag;
        wxString text;
        std::vector<std::pair<std::size_t, std::size_t>> runs;
    };
    static std::vector<WordRuns> words(const Column&);
    struct Hit {
        std::size_t row;
        const Column* column;
        std::size_t column_index;
        std::size_t line;
        int run;
        double distance;
    };
    std::optional<Hit> hit(wxPoint) const;
    void click_text(wxPoint);
    void rebuild_verse_links();
    void position_verse_links();
    std::optional<std::pair<WordRuns, int>> word_of(const Column&, int tag, std::size_t line, int run) const;
    void set_position(double);
    void navigate_to(double);
    double content_height() const;
    double max_offset() const;
    void rebuild_positions();
    void prepare_visible();
    void prefetch_layout();
    void schedule_prefetch();
    std::size_t first_visible() const;
    void scrollbar();
    const Layout& row_layout(std::size_t row) const;
    void paint(wxPaintEvent&);
    bool passage_in_view() const;
    void draw_return(wxDC&);
    void hover_return(bool);
    void draw(wxDC&, wxSize, std::size_t begin, std::size_t end, double top, bool cache_text = true) const;
    void draw_text(wxGraphicsContext&, wxDC&, const Column&, std::size_t row, std::size_t column, int x,
                   int y, int bottom) const;
    void clear_tiles() const;
    void request_repaint();
    void invalidate();
    void start_animation();
    void release_follow();
    void locate_playback();
    struct GuideSpan {
        size_t line;
        double first, last, weight;
    };
    struct Guide {
        size_t row, column, line;
        double y;
        int height;
        std::vector<GuideSpan> spans;
    };
    std::optional<Guide> guide_;
    std::size_t layout_epoch_ = 0, guide_epoch_ = 0;
    SpeechPlayback playback_;
    std::optional<SpeechCue> located_cue_;
    std::optional<size_t> speech_row_;
    std::pair<VerseRef, VerseRef> speech_range_; // the spoken verses in framing numbers
    double guide_y_ = 0, guide_alpha_ = 0, follow_target_ = 0;
    bool following_ = false;
    std::function<void()> release_follow_, selection_changed_;
    std::function<void(const std::optional<Word>&)> word_clicked_;
    std::function<void()> returned_;
    wxString return_label_;
    wxRect return_rect_; // empty while the button is hidden
    bool return_always_ = false, return_hover_ = false, return_pressed_ = false;
    bool end_seen_ = false;
    std::optional<Word> highlighted_;
    std::optional<std::pair<VerseRef, VerseRef>> highlighted_verse_;
    std::size_t highlighted_column_ = 0;
    wxPanel* verse_links_ = nullptr;
    std::optional<VerseRef> drag_anchor_;
    bool drag_selects_ = true;
    wxPoint press_point_;
    std::vector<std::pair<VerseRef, VerseRef>> selections_, drag_ranges_;
    bool dragged_ = false, append_drag_ = false;
    wxTimer follow_timer_;
    std::chrono::steady_clock::time_point animation_tick_;
    int columns_count() const;
    int outside_margin() const;
    int column_width() const;
    // The width of the lowercase alphabet in the body font, the unit for reading measures.
    double alphabet_width() const;
    // Columns one above another, when side by side they would be too narrow.
    bool stacked_columns() const;
    // The distance from one side-by-side column to the next.
    int column_stride() const;
    // A row's height before its paragraphs are laid out.
    int estimated_height(const Row&) const;
    std::vector<int> heights_;
    std::vector<double> positions_;
    double offset_ = 0, target_ = 0;
    int layout_width_ = 0;
    wxTimer wheel_timer_;
    wxTimer layout_timer_;
    void* native_scroll_ = nullptr;
    const CorpusDb& corpus_;
    std::vector<Source> source_catalog_;
    Reading reading_{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}};
    Settings settings_;
    std::function<void(const Reading&)> bookmarked_, listen_;
    Passage displayed_{"Ps", {23, 1}, {23, 6}};
    std::string base_source_ = "sv1917", frame_ = "grc-lxx";
    const CanonBook* canon_ = nullptr;
    std::string canon_code() const {
        return canon_ ? canon_->code : displayed_.book;
    }
    std::vector<std::string> shown_sources(std::size_t row) const;
    wxString source_label(const std::string&, std::size_t heading) const;
    std::string language_of(const std::string&) const;
    wxRect source_selector_rect(std::size_t heading, std::size_t pane) const;
    void position_source_selectors();
    void source_menu(std::size_t heading, std::size_t pane, wxWindow* button);
    std::function<void(bool)> source_changed_;
    mutable std::map<int, std::vector<std::string>> available_sources_;
    std::map<std::pair<std::string, int>, std::array<std::string, 2>> chapter_overrides_;
    std::map<std::pair<std::size_t, std::size_t>, wxWindow*> source_selectors_;
    // Verses of an edition without a Septuagint counterpart, after the framing verse they follow.
    using Orphans = std::map<VerseRef, std::vector<std::pair<std::string, VerseRef>>>;
    const Orphans& orphans(const std::string& source) const;
    mutable std::map<std::string, Orphans> orphans_;
    // The Swedish edition of the chapter a heading row opens: the 1917 Bible, the 1921 apocrypha, or both.
    // Empty where the chapter has no Swedish text.
    wxString swedish_edition(std::size_t heading) const;
    mutable std::map<std::size_t, wxString> swedish_editions_;
    std::vector<Row> rows_;
    mutable std::map<std::size_t, Layout> cache_;
    mutable std::list<std::size_t> order_;
    static constexpr std::size_t cache_limit = 192;
    // Eight lines per transparent tile. Keep native pixels at the destination's
    // scale, and bound the cache by bytes rather than paragraph count.
    using TileKey = std::tuple<std::size_t, std::size_t, std::size_t>;
    struct TextTile {
        wxGraphicsBitmap bitmap;
        int width, height;
        std::size_t bytes;
        std::list<TileKey>::iterator recency;
    };
    mutable std::map<TileKey, TextTile> tiles_;
    mutable std::list<TileKey> tile_order_;
    mutable double tile_scale_ = 0;
    mutable wxRealPoint tile_dpi_;
    mutable wxColour tile_ink_, tile_muted_, tile_verse_;
    mutable RenderStats render_stats_;
    static constexpr std::size_t tile_budget = std::size_t{32} * 1024 * 1024;
    static constexpr std::size_t tile_lines = 8;
};
} // namespace ortho
