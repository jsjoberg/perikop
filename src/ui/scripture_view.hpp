#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "typesetting/paragraph_layout.hpp"
#include "ui/theme.hpp"
#include <list>
#include <map>
#include <memory>
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
    void render_to(wxDC&, wxSize size);
    void scroll_by(double pixels);
    double scroll_position() const {
        return offset_;
    }
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
    bool verse_selected(VerseRef) const;
    // The left pane's edition, and the edition whose numbering frames the text.
    const std::string& base_source() const {
        return base_source_;
    }
    const std::string& frame() const {
        return frame_;
    }
    void clear_selection();
    void select_verses(VerseRef first, VerseRef last, bool append = false);
    void on_selection(std::function<void()> callback) {
        selection_changed_ = std::move(callback);
    }
    // The left-pane verse under a point in client coordinates.
    std::optional<VerseRef> verse_at(wxPoint) const;
    // A left-pane word: its framing book and verse, and which occurrence of
    // the same word in that verse it is, counting from 0.
    struct Word {
        std::string book, text;
        VerseRef verse;
        int occurrence = 0;
    };
    std::optional<Word> word_at(wxPoint) const;
    // A plain click on a word, when no verses are marked.
    void on_word(std::function<void(const Word&)> callback) {
        word_clicked_ = std::move(callback);
    }
    void highlight_word(std::optional<Word>);
    // A translucent button floating over the text's lower edge; an empty
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
        std::size_t line;
        int run;
        double distance;
    };
    std::optional<Hit> hit(wxPoint) const;
    std::optional<std::pair<WordRuns, int>> word_of(const Column&, int tag, std::size_t line, int run) const;
    void set_position(double);
    double content_height() const;
    double max_offset() const;
    void rebuild_positions();
    void prepare_visible();
    std::size_t first_visible() const;
    void scrollbar();
    const Layout& row_layout(std::size_t row) const;
    void paint(wxPaintEvent&);
    bool passage_in_view() const;
    void draw_return(wxDC&);
    void hover_return(bool);
    void draw(wxDC&, wxSize, std::size_t begin, std::size_t end, double top) const;
    void invalidate();
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
    SpeechPlayback playback_;
    std::optional<SpeechCue> located_cue_;
    std::optional<size_t> speech_row_;
    std::pair<VerseRef, VerseRef> speech_range_; // the spoken verses in framing numbers
    double guide_y_ = 0, guide_alpha_ = 0, follow_target_ = 0;
    bool following_ = false;
    std::function<void()> release_follow_, selection_changed_;
    std::function<void(const Word&)> word_clicked_;
    std::function<void()> returned_;
    wxString return_label_;
    wxRect return_rect_; // empty while the button is hidden
    bool return_always_ = false, return_hover_ = false, return_pressed_ = false;
    bool end_seen_ = false;
    std::optional<Word> highlighted_;
    std::optional<VerseRef> drag_anchor_;
    std::vector<std::pair<VerseRef, VerseRef>> selections_, drag_ranges_;
    bool dragged_ = false, append_drag_ = false;
    wxTimer follow_timer_;
    std::vector<std::string> languages() const;
    int columns_count() const;
    int outside_margin() const;
    int column_width() const;
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
    void* native_scroll_ = nullptr;
    const CorpusDb& corpus_;
    Reading reading_{ReadingKind::MorningPsalm, {"Ps", {23, 1}, {23, 6}}, "Psalm 23"};
    Settings settings_;
    Passage displayed_{"Ps", {23, 1}, {23, 6}};
    std::string base_source_ = "sv1917", frame_ = "grc-lxx";
    const CanonBook* canon_ = nullptr;
    std::string canon_code() const {
        return canon_ ? canon_->code : displayed_.book;
    }
    std::string source_of(const std::string& language) const;
    // Verses of an edition without a Septuagint counterpart, after the framing verse they follow.
    using Orphans = std::map<VerseRef, std::vector<std::pair<std::string, VerseRef>>>;
    const Orphans& orphans(const std::string& source) const;
    mutable std::map<std::string, Orphans> orphans_;
    std::vector<Row> rows_;
    mutable std::map<std::size_t, Layout> cache_;
    mutable std::list<std::size_t> order_;
    static constexpr std::size_t cache_limit = 192;
};
} // namespace ortho
