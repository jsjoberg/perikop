#include "ui/picker_grid.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

namespace ortho {
namespace {
// Layout on a grid of device-independent pixels.
constexpr int cell_width = 58, cell_height = 30, gap = 4, inset = 14, notch = 7;
} // namespace

PickerGrid::PickerGrid(wxWindow* parent, Theme theme)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      theme_(theme) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &PickerGrid::paint, this);
    Bind(wxEVT_KEY_DOWN, &PickerGrid::key, this);
    for (const auto type : {wxEVT_SET_FOCUS, wxEVT_KILL_FOCUS})
        Bind(type, [this](wxFocusEvent& e) {
            Refresh(false);
            e.Skip();
        });
    Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
        pointer_ = e.GetPosition();
        hover(item_at(*pointer_));
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
        pointer_.reset();
        hover(std::nullopt);
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        keyboard_ = false;
        SetFocus();
        e.Skip();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (const auto index = item_at(e.GetPosition())) {
            focus_ = index;
            activate(*index);
        }
    });
}

void PickerGrid::set(std::vector<Block> blocks) {
    // The chosen cell sits above anything it unfolds, so its position identifies it again.
    const auto focused = focus_ ? std::optional(items_[*focus_].rect.GetTopLeft()) : std::nullopt;
    const auto label = focus_ && items_[*focus_].cell ? items_[*focus_].cell->label : wxString();
    blocks_ = std::move(blocks);
    layout();
    focus_.reset();
    for (std::size_t i = 0; focused && i < items_.size(); ++i)
        if (items_[i].rect.GetTopLeft() == *focused && active(i))
            focus_ = i;
    // A drawer above it may have closed and moved it up: then the cell with the same label,
    // preferring the one now open, nearest its old place.
    if (!focus_ && focused && !label.empty()) {
        // (not open, distance): the lowest wins; every real cell beats the starting rank.
        std::pair best{true, std::numeric_limits<int>::max()};
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const auto& item = items_[i];
            if (!active(i) || !item.cell || item.cell->label != label)
                continue;
            const bool open = item.block->open && &item.block->cells[*item.block->open] == item.cell;
            const std::pair rank{!open, std::abs(item.rect.y - focused->y)};
            if (rank < best) {
                best = rank;
                focus_ = i;
            }
        }
    }
    refresh_hover();
}

void PickerGrid::refresh_hover() {
    hover_.reset();
    UnsetToolTip();
    SetCursor(wxNullCursor);
    // Wayland gives no global pointer position, and GTK crashes asking without a pointer device,
    // so use the last position this window saw.
    hover(pointer_ ? item_at(*pointer_) : std::nullopt);
}

void PickerGrid::set_width(int width) {
    if (width == width_)
        return;
    width_ = width;
    const auto focused = focus_ ? std::optional(items_[*focus_]) : std::nullopt;
    layout();
    // The same cell keeps the focus in its new place.
    focus_.reset();
    for (std::size_t i = 0; focused && i < items_.size(); ++i)
        if (items_[i].block == focused->block && items_[i].cell == focused->cell &&
            items_[i].action == focused->action)
            focus_ = i;
    refresh_hover();
}

wxRect PickerGrid::unfolded(int depth) const {
    for (const auto& drawer : drawers_)
        if (drawer.depth == depth)
            return {drawer.rect.x, drawer.opener.y, drawer.rect.width,
                    drawer.rect.GetBottom() + 1 - drawer.opener.y};
    return {};
}

wxSize PickerGrid::DoGetBestClientSize() const {
    return {width_, height_};
}

int PickerGrid::place(const std::vector<Block>& blocks, int x, int width, int y, int depth) {
    int w = 0, h = 0;
    for (const auto& block : blocks) {
        switch (block.kind) {
        case Block::Kind::Heading: {
            const auto font = ui_font(9);
            GetTextExtent(block.text, &w, &h, nullptr, nullptr, &font);
            y += FromDIP(zoomed(12));
            items_.push_back({{x, y, w, h}, &block});
            y += h + FromDIP(zoomed(12));
            break;
        }
        case Block::Kind::Title: {
            const auto title_font = body_font(15), action_font = ui_font(10);
            GetTextExtent(block.text, &w, &h, nullptr, nullptr, &title_font);
            items_.push_back({{x, y, w, h}, &block});
            int bottom = y + h;
            if (!block.actions.empty()) {
                std::vector<wxSize> pills;
                int row = 0;
                for (const auto& action : block.actions) {
                    int aw = 0, ah = 0;
                    GetTextExtent(action.label, &aw, &ah, nullptr, nullptr, &action_font);
                    pills.emplace_back(aw + FromDIP(zoomed(24)),
                                       std::max(ah + FromDIP(zoomed(10)), FromDIP(zoomed(26))));
                    row += pills.back().x + (row ? FromDIP(zoomed(8)) : 0);
                }
                // A right-aligned row beside the title when both fit, else a row beneath it.
                const bool beside = w + FromDIP(zoomed(16)) + row <= width;
                wxPoint at = beside ? wxPoint(x + width - row, y) : wxPoint(x, y + h + FromDIP(zoomed(8)));
                for (std::size_t i = 0; i < pills.size(); ++i) {
                    const wxPoint pill_at(at.x, beside ? y + (h - pills[i].y) / 2 : at.y);
                    items_.push_back({{pill_at, pills[i]}, &block, nullptr, &block.actions[i]});
                    bottom = std::max(bottom, pill_at.y + pills[i].y);
                    at.x += pills[i].x + FromDIP(zoomed(8));
                }
            }
            y = bottom + FromDIP(zoomed(12));
            break;
        }
        case Block::Kind::Cells: {
            const int step = FromDIP(zoomed(cell_width + gap)), space = FromDIP(zoomed(gap));
            const int columns = std::clamp((width + space) / step, 1, block.columns);
            for (std::size_t first = 0; first < block.cells.size(); first += std::size_t(columns)) {
                const auto end = std::min(block.cells.size(), first + std::size_t(columns));
                for (auto i = first; i < end; ++i) {
                    // Spread the remainder so the row fills the width exactly.
                    const int column = int(i - first);
                    const int left = x + column * (width + space) / columns;
                    const int right = x + (column + 1) * (width + space) / columns - space;
                    items_.push_back(
                        {{left, y, right - left, FromDIP(zoomed(cell_height))}, &block, &block.cells[i]});
                }
                y += FromDIP(zoomed(cell_height));
                if (const auto& open = block.open; open && first <= *open && *open < end) {
                    const wxRect opener = items_[items_.size() - (end - *open)].rect;
                    y += FromDIP(zoomed(notch + 4));
                    const int top = y;
                    y = place(block.drawer, x + FromDIP(zoomed(inset)), width - 2 * FromDIP(zoomed(inset)),
                              y + FromDIP(zoomed(inset)), depth + 1) +
                        FromDIP(zoomed(inset - gap));
                    drawers_.push_back({{x, top, width, y - top}, opener, depth + 1});
                    y += FromDIP(zoomed(10));
                } else
                    y += space;
            }
            break;
        }
        }
    }
    return y;
}

void PickerGrid::layout() {
    items_.clear();
    drawers_.clear();
    height_ = place(blocks_, 0, width_, 0, 0);
    InvalidateBestSize();
    SetMinSize({width_, height_});
    Refresh(false);
}

std::optional<std::size_t> PickerGrid::item_at(wxPoint point) const {
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (active(i) && items_[i].rect.Contains(point))
            return i;
    return std::nullopt;
}

bool PickerGrid::active(std::size_t index) const {
    const auto& item = items_[index];
    return (item.action && item.action->enabled) || (item.cell && item.cell->enabled && item.cell->action);
}

void PickerGrid::hover(std::optional<std::size_t> index) {
    if (index == hover_)
        return;
    hover_ = index;
    const auto* cell = index ? items_[*index].cell : nullptr;
    const auto* action = index ? items_[*index].action : nullptr;
    const auto tip = cell ? cell->tip : action ? action->tip : wxString{};
    if (!tip.empty())
        SetToolTip(tip);
    else
        UnsetToolTip();
    SetCursor(index ? wxCursor(wxCURSOR_HAND) : wxNullCursor);
    Refresh(false);
}

void PickerGrid::activate(std::size_t index) {
    const auto& item = items_[index];
    // The action can replace these cells or close the picker, so it runs after this event.
    CallAfter(item.cell ? item.cell->action : item.action->run);
}

void PickerGrid::key(wxKeyEvent& e) {
    const int code = e.GetKeyCode();
    if ((code == WXK_RETURN || code == WXK_NUMPAD_ENTER || code == WXK_SPACE) && focus_) {
        activate(*focus_);
        return;
    }
    if (code != WXK_LEFT && code != WXK_RIGHT && code != WXK_UP && code != WXK_DOWN) {
        e.Skip();
        return;
    }
    keyboard_ = true;
    std::optional<std::size_t> next;
    if (!focus_) {
        for (std::size_t i = 0; i < items_.size() && !next; ++i)
            if (active(i))
                next = i;
    } else if (code == WXK_LEFT || code == WXK_RIGHT) {
        // Reading order runs through a drawer before the rows below it.
        for (auto i = *focus_; !next;) {
            if (code == WXK_LEFT ? i == 0 : i + 1 == items_.size())
                break;
            i = code == WXK_LEFT ? i - 1 : i + 1;
            if (active(i))
                next = i;
        }
    } else {
        // The nearest row above or below, then the cell closest across.
        const auto from = items_[*focus_].rect;
        const bool up = code == WXK_UP;
        std::optional<std::pair<int, int>> best;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const auto& r = items_[i].rect;
            if (!active(i) || (up ? r.GetBottom() >= from.y : r.y <= from.GetBottom()))
                continue;
            const int distance = up ? from.y - r.GetBottom() : r.y - from.GetBottom();
            const int across = std::abs(r.x + r.width / 2 - (from.x + from.width / 2));
            if (const std::pair candidate{distance, across}; !best || candidate < *best) {
                best = candidate;
                next = i;
            }
        }
    }
    if (next) {
        focus_ = next;
        if (focused_)
            focused_(items_[*next].rect);
    }
    Refresh(false);
}

void PickerGrid::paint(wxPaintEvent&) {
    const auto colors = palette(theme_);
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(colors.paper));
    dc.Clear();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const double scale = FromDIP(zoomed(100)) / 100.0, radius = 6 * scale;
    // Drawers from the outermost in, each with a notch that points at the cell it belongs to.
    auto drawers = drawers_;
    std::ranges::sort(drawers, {}, &Drawer::depth);
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(with_alpha(colors.accent, 22)));
    for (const auto& drawer : drawers) {
        const auto& r = drawer.rect;
        gc->DrawRoundedRectangle(r.x, r.y, r.width, r.height, 2 * radius);
        const double centre = drawer.opener.x + drawer.opener.width / 2.0, tip = FromDIP(zoomed(notch));
        auto path = gc->CreatePath();
        path.MoveToPoint(centre - tip, r.y);
        path.AddLineToPoint(centre, r.y - tip);
        path.AddLineToPoint(centre + tip, r.y);
        path.CloseSubpath();
        gc->FillPath(path);
    }
    double width = 0, height = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        const auto& r = item.rect;
        const bool hovered = hover_ == i, focused = focus_ == i && keyboard_ && HasFocus();
        gc->SetPen(*wxTRANSPARENT_PEN);
        if (!item.cell && !item.action) {
            const bool heading = item.block->kind == Block::Kind::Heading;
            gc->SetFont(heading ? ui_font(9) : body_font(15), heading ? colors.muted : colors.ink);
            gc->DrawText(item.block->text, r.x, r.y);
            continue;
        }
        if (item.action) {
            const auto ink = item.action->enabled ? colors.accent : with_alpha(colors.muted, 110);
            if (hovered && item.action->enabled) {
                gc->SetBrush(wxBrush(with_alpha(colors.accent, 36)));
                gc->DrawRoundedRectangle(r.x, r.y, r.width, r.height, r.height / 2.0);
            }
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(ink).Width(1.1 * scale)));
            gc->DrawRoundedRectangle(r.x + 0.5, r.y + 0.5, r.width - 1, r.height - 1, (r.height - 1) / 2.0);
            gc->SetFont(ui_font(10), ink);
            gc->GetTextExtent(item.action->label, &width, &height);
            gc->DrawText(item.action->label, r.x + (r.width - width) / 2, r.y + (r.height - height) / 2);
        } else {
            const auto& cell = *item.cell;
            const bool selected = cell.state == State::Selected;
            if (cell.enabled) {
                gc->SetBrush(wxBrush(selected ? with_alpha(colors.accent, hovered ? 90 : 70)
                                     : cell.state == State::Marked
                                         ? with_alpha(colors.accent, hovered ? 54 : 38)
                                     : hovered ? colors.overlay
                                               : colors.surface));
                gc->DrawRoundedRectangle(r.x, r.y, r.width, r.height, radius);
            }
            auto font = ui_font(11);
            if (selected)
                font.SetWeight(wxFONTWEIGHT_MEDIUM);
            gc->SetFont(font, !cell.enabled ? with_alpha(colors.muted, 110)
                              : selected    ? colors.accent
                                            : colors.ink);
            gc->GetTextExtent(cell.label, &width, &height);
            gc->DrawText(cell.label, r.x + (r.width - width) / 2, r.y + (r.height - height) / 2);
        }
        if (focused) {
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(colors.ink).Width(1.6 * scale)));
            const double corner = item.action ? r.height / 2.0 : radius;
            gc->DrawRoundedRectangle(r.x + 1, r.y + 1, r.width - 2, r.height - 2, corner);
        }
    }
}
} // namespace ortho
