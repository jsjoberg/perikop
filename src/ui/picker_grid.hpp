#pragma once

#include "ui/theme.hpp"
#include <functional>
#include <optional>
#include <vector>
#include <wx/control.h>

namespace ortho {
// The Bible picker's books, chapters, and verses, drawn in the theme's colours
// like the month calendar. An open cell unfolds its contents directly beneath
// its own row. A click or Return chooses a cell; arrows move between cells.
class PickerGrid final : public wxControl {
public:
    enum class State { Plain, Marked, Selected };
    struct Cell {
        wxString label, tip;
        bool enabled = true;
        State state = State::Plain;
        std::function<void()> action;
    };
    struct Block {
        // A small capital heading; a title with an optional action beside it; or a grid of cells.
        enum class Kind { Heading, Title, Cells } kind;
        wxString text{}, action_label{};
        std::function<void()> action{};
        std::vector<Cell> cells{};
        int columns = 10;
        // The cell whose row the drawer follows, and the drawer's contents.
        std::optional<std::size_t> open{};
        std::vector<Block> drawer{};
    };
    PickerGrid(wxWindow* parent, Theme);
    void set(std::vector<Block>);
    // Lays the cells out for this width; the height follows.
    void set_width(int);
    // The open cell's row and its drawer at a depth from 1, or an empty rectangle.
    wxRect unfolded(int depth) const;
    // Called with the cell the keyboard moves to, so that it can be scrolled into view.
    void on_focus(std::function<void(wxRect)> callback) {
        focused_ = std::move(callback);
    }

protected:
    wxSize DoGetBestClientSize() const override;

private:
    struct Item {
        wxRect rect;
        const Block* block;
        const Cell* cell = nullptr; // a cell, else the block's title or action
        bool action = false;
    };
    struct Drawer {
        wxRect rect, opener;
        int depth;
    };
    int place(const std::vector<Block>&, int x, int width, int y, int depth);
    void layout();
    void paint(wxPaintEvent&);
    void key(wxKeyEvent&);
    std::optional<std::size_t> item_at(wxPoint) const;
    bool active(std::size_t) const;
    void hover(std::optional<std::size_t>);
    void activate(std::size_t);
    Theme theme_;
    std::vector<Block> blocks_;
    std::vector<Item> items_;
    std::vector<Drawer> drawers_;
    std::function<void(wxRect)> focused_;
    int width_ = 0, height_ = 0;
    std::optional<std::size_t> hover_, focus_;
    // The focus ring shows once the keyboard is used, not after a click.
    bool keyboard_ = false;
};
} // namespace ortho
