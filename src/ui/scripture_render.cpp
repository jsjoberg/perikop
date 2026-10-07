#include "ui/controls.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <limits>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
namespace ortho {
void ScriptureView::draw(wxDC& dc, wxSize size, std::size_t begin, std::size_t end, double top) const {
    const auto colors = palette(settings_.theme);
    dc.SetBackground(wxBrush(colors.paper));
    dc.Clear();
    const int margin = outside_margin(), stride = column_stride();
    const bool stacked = stacked_columns();
    std::unique_ptr<wxGraphicsContext> gc(
        wxGraphicsRenderer::GetDefaultRenderer()->CreateContextFromUnknownDC(dc));
    int y = int(std::lround(top)) + FromDIP(12);
    for (std::size_t index = begin; index < end && y < size.y; ++index) {
        const auto& row = rows_[index];
        const auto& layout = row_layout(index);
        if (row.heading) {
            dc.SetTextForeground(colors.muted);
            dc.SetFont(ui_font(11));
            auto name = ui::utf8(corpus_.book_name(canon_code())).Upper();
            const auto extent = dc.GetTextExtent(name);
            dc.DrawText(name, (size.x - extent.x) / 2, y + FromDIP(15));
            dc.SetFont(body_font(28));
            dc.SetTextForeground(colors.ink);
            const wxString title = wxString::Format("%d", row.ref.chapter - (canon_ ? canon_->offset() : 0));
            dc.DrawText(title, (size.x - dc.GetTextExtent(title).x) / 2, y + FromDIP(42));
            dc.SetFont(ui_font(9));
            dc.SetTextForeground(colors.muted);
            wxString labels;
            for (const auto& language : languages()) {
                if (!labels.empty())
                    labels += "     ·     ";
                const auto source = source_of(language);
                labels += language == "sv"   ? "SVENSKA 1917"
                          : language == "en" ? (source == "en-web" ? "WORLD ENGLISH BIBLE" : "KING JAMES")
                                             : "ΕΛΛΗΝΙΚΑ";
            }
            dc.DrawText(labels, (size.x - dc.GetTextExtent(labels).x) / 2, y + FromDIP(93));
        } else {
            if (columns_count() > 1 && !stacked) {
                dc.SetPen(wxPen(colors.rule, 1));
                for (int c = 1; c < columns_count(); ++c) {
                    const int gutter = margin + c * stride - FromDIP(24);
                    dc.DrawLine(gutter, y, gutter, y + layout.height);
                }
            }
            int stacked_y = y;
            for (std::size_t c = 0; c < layout.columns.size(); ++c) {
                const auto& column = layout.columns[c];
                const int x = margin + (stacked ? 0 : int(c) * stride);
                int text_y = stacked ? stacked_y : y;
                if (stacked && layout.columns.size() > 1) {
                    dc.SetFont(ui_font(8));
                    dc.SetTextForeground(colors.muted);
                    dc.DrawText(column.language == "sv"   ? "SVENSKA"
                                : column.language == "el" ? "ΕΛΛΗΝΙΚΑ"
                                                          : "ENGLISH",
                                x + FromDIP(30), text_y);
                    text_y += FromDIP(24);
                }
                dc.SetFont(body_font(settings_.font_size));
                dc.SetTextForeground(colors.ink);
                // The margin marker, and the tinted spoken line unless the reader turned it off.
                if (gc && guide_ && guide_->row == index && guide_->column == c && guide_alpha_ > 0.01) {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    for (const auto& span : guide_->spans) {
                        if (!settings_.speech_highlight)
                            break;
                        const double center = positions_[index] + text_y - y + FromDIP(12) +
                                              (span.line + 0.5) * column.text.line_height;
                        const double emphasis =
                            std::clamp(1 - std::abs(center - guide_y_) / column.text.line_height, 0.0, 1.0);
                        gc->SetBrush(
                            wxBrush(with_alpha(colors.accent, int((16 + 20 * emphasis) * guide_alpha_))));
                        gc->DrawRoundedRectangle(x + FromDIP(30) + span.first - FromDIP(5),
                                                 text_y + span.line * column.text.line_height + FromDIP(1),
                                                 span.last - span.first + FromDIP(10),
                                                 column.text.line_height - FromDIP(2), FromDIP(4));
                    }
                    const double marker_y = guide_y_ - offset_ - guide_->height * 0.34;
                    gc->SetBrush(wxBrush(with_alpha(colors.accent, int(230 * guide_alpha_))));
                    gc->DrawRoundedRectangle(x + FromDIP(16), marker_y, FromDIP(4), guide_->height * 0.68,
                                             FromDIP(2));
                }
                if (gc && c == 0 && selection_) {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(with_alpha(colors.accent, 56)));
                    for (std::size_t line = 0; line < column.text.lines.size(); ++line) {
                        double first = std::numeric_limits<double>::max(), last = 0;
                        for (const auto& run : column.text.lines[line].runs) {
                            if (!column.main_text(run.tag))
                                continue;
                            const auto& verse = column.verses[run.tag].first;
                            if (verse < selection_->first || selection_->second < verse)
                                continue;
                            first = std::min(first, run.x);
                            last = std::max(last, run.x + run.width);
                        }
                        if (last > first)
                            gc->DrawRectangle(x + FromDIP(30) + first - FromDIP(3),
                                              text_y + line * column.text.line_height,
                                              last - first + FromDIP(6), column.text.line_height);
                    }
                }
                // The word shown in the Ordstudium panel.
                if (gc && c == 0 && highlighted_ && highlighted_->book == displayed_.book) {
                    const auto target = wxString::FromUTF8(highlighted_->text).Lower();
                    std::map<wxString, int> seen;
                    for (const auto& word : words(column)) {
                        if (size_t(word.tag) >= column.verses.size() ||
                            column.verses[word.tag].first != highlighted_->verse)
                            continue;
                        const auto lower = word.text.Lower();
                        const int occurrence = seen[lower]++;
                        if (lower != target || occurrence != highlighted_->occurrence)
                            continue;
                        gc->SetPen(*wxTRANSPARENT_PEN);
                        gc->SetBrush(wxBrush(with_alpha(colors.accent, 72)));
                        for (const auto& [line, index] : word.runs) {
                            const auto& run = column.text.lines[line].runs[index];
                            gc->DrawRoundedRectangle(x + FromDIP(30) + run.x - FromDIP(3),
                                                     text_y + line * column.text.line_height + FromDIP(2),
                                                     run.width + FromDIP(6),
                                                     column.text.line_height - FromDIP(4), FromDIP(4));
                        }
                    }
                }
                draw_paragraph(dc, column.text, x + FromDIP(30), text_y,
                               [&](const TextRun& run) -> std::optional<wxColour> {
                                   if (run.tag >= 0 && size_t(run.tag) < column.faint.size() &&
                                       column.faint[run.tag])
                                       return colors.muted;
                                   return std::nullopt;
                               });
                // Mark only the lines containing the prescribed verse range.
                if (c == 0) {
                    dc.SetPen(wxPen(colors.accent, FromDIP(3)));
                    for (std::size_t line = 0; line < column.text.lines.size(); ++line) {
                        const auto& runs = column.text.lines[line].runs;
                        if (std::any_of(runs.begin(), runs.end(), [&](const auto& run) {
                                return run.tag >= 0 && std::size_t(run.tag) < column.prescribed.size() &&
                                       column.prescribed[run.tag];
                            })) {
                            const int ly = text_y + int(line) * column.text.line_height;
                            dc.DrawLine(margin - FromDIP(17), ly, margin - FromDIP(17),
                                        ly + column.text.line_height);
                        }
                    }
                }
                text_y += column.text.height();
                if (stacked)
                    stacked_y = text_y;
            }
        }
        y += layout.height;
    }
}
void ScriptureView::paint(wxPaintEvent&) {
    prepare_visible();
    scrollbar();
    wxAutoBufferedPaintDC dc(this);
    const auto begin = first_visible();
    draw(dc, GetClientSize(), begin, rows_.size(), positions_.empty() ? 0 : positions_[begin] - offset_);
    draw_return(dc);
    // The passage's last paragraph has been seen once its end is on screen.
    const double bottom = offset_ + GetClientSize().y;
    for (auto i = begin; !end_seen_ && i < rows_.size() && positions_[i] < bottom; ++i)
        if (!rows_[i].heading && rows_[i].ref <= displayed_.last && displayed_.last <= rows_[i].last)
            end_seen_ = positions_[i + 1] <= bottom;
}
void ScriptureView::return_button(const wxString& label, bool always) {
    if (return_label_ == label && return_always_ == always)
        return;
    return_label_ = label;
    return_always_ = always;
    Refresh(false);
}
void ScriptureView::hover_return(bool hover) {
    if (return_hover_ == hover)
        return;
    return_hover_ = hover;
    SetCursor(hover ? wxCursor(wxCURSOR_HAND) : wxNullCursor);
    Refresh(false);
}
void ScriptureView::draw_return(wxDC& dc) {
    return_rect_ = {};
    if (return_label_.empty() || rows_.empty() || (!return_always_ && passage_in_view())) {
        if (return_hover_)
            hover_return(false);
        return;
    }
    std::unique_ptr<wxGraphicsContext> gc(
        wxGraphicsRenderer::GetDefaultRenderer()->CreateContextFromUnknownDC(dc));
    if (!gc)
        return;
    const auto colors = palette(settings_.theme);
    const auto font = ui_font(13);
    gc->SetFont(font, colors.ink);
    double width = 0, height = 0;
    gc->GetTextExtent(return_label_, &width, &height);
    const int pad_x = FromDIP(26), pad_y = FromDIP(12);
    const auto size = GetClientSize();
    return_rect_ = wxRect(int((size.x - width) / 2) - pad_x, size.y - FromDIP(30) - int(height) - 2 * pad_y,
                          int(width) + 2 * pad_x, int(height) + 2 * pad_y);
    // Mostly see-through until the pointer reaches it.
    const bool hover = return_hover_;
    gc->SetBrush(wxBrush(with_alpha(colors.paper, hover ? 245 : 185)));
    gc->SetPen(
        gc->CreatePen(wxGraphicsPenInfo(with_alpha(hover ? colors.accent : colors.muted, hover ? 200 : 70))
                          .Width(FromDIP(100) / 100.0)));
    gc->DrawRoundedRectangle(return_rect_.x, return_rect_.y, return_rect_.width, return_rect_.height,
                             return_rect_.height / 2.0);
    gc->SetFont(font, with_alpha(colors.ink, hover ? 255 : 165));
    gc->DrawText(return_label_, return_rect_.x + pad_x, return_rect_.y + (return_rect_.height - height) / 2);
}
void ScriptureView::render_to(wxDC& dc, wxSize size) {
    prepare_visible();
    const auto begin = first_visible();
    draw(dc, size, begin, rows_.size(), positions_.empty() ? 0 : positions_[begin] - offset_);
}
} // namespace ortho
