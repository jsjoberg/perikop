#include "ui/scripture_view.hpp"
#include <algorithm>
#include <cmath>
#include <wx/image.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>

#include <gdiplus.h>
#endif

namespace ortho {
void ScriptureView::clear_tiles() const {
    tiles_.clear();
    tile_order_.clear();
    render_stats_.tile_bytes = 0;
}

void ScriptureView::draw_text(wxGraphicsContext& gc, wxDC& dc, const Column& column, std::size_t row,
                              std::size_t col, int x, int y, int bottom) const {
    const auto colors = palette(settings_.theme);
    const double scale = dc.GetContentScaleFactor();
    // Layout uses window metrics. A memory DC can report the system DPI even
    // when this window is on a different monitor.
    const auto window_dpi = GetDPI();
    const wxRealPoint dpi(window_dpi.x, window_dpi.y);
    if (tile_scale_ != scale || tile_dpi_ != dpi || tile_ink_ != colors.ink || tile_muted_ != colors.muted) {
        clear_tiles();
        tile_scale_ = scale;
        tile_dpi_ = dpi;
        tile_ink_ = colors.ink;
        tile_muted_ = colors.muted;
    }
    const auto& text = column.text;
    const int pad = FromDIP(8), line_height = text.line_height;
    const auto first = std::size_t(std::max(0, (-y - pad) / line_height)) / tile_lines;
    const auto count = (text.lines.size() + tile_lines - 1) / tile_lines;
    const auto colour = [&](const TextRun& run) -> std::optional<wxColour> {
        if (run.tag >= 0 && std::size_t(run.tag) < column.faint.size() && column.faint[run.tag])
            return colors.muted;
        return std::nullopt;
    };
    for (auto tile = first; tile < count; ++tile) {
        const auto begin = tile * tile_lines, end = std::min(begin + tile_lines, text.lines.size());
        const int top = int(begin) * line_height;
        if (y + top - pad >= bottom)
            break;
        const TileKey key{row, col, tile};
        auto found = tiles_.find(key);
        if (found == tiles_.end()) {
            double extent = column_width();
            for (auto line = begin; line < end; ++line)
                for (const auto& run : text.lines[line].runs)
                    extent = std::max(extent, run.x + run.width);
            const int width = int(std::ceil(extent)) + 2 * pad;
            const int height = int(end - begin) * line_height + 2 * pad;
            const int pixels_x = int(std::ceil(width * scale)), pixels_y = int(std::ceil(height * scale));
            const auto bytes = std::size_t(pixels_x) * std::size_t(pixels_y) * 4;
            // Extreme window/font sizes still draw correctly without an unbounded allocation.
            if (bytes > tile_budget) {
                draw_paragraph(gc, text, dc.GetFont(), dpi, colors.ink, x, y, begin, end, colour);
                continue;
            }
            wxImage image(pixels_x, pixels_y);
            image.InitAlpha();
            std::fill_n(image.GetAlpha(), std::size_t(pixels_x) * std::size_t(pixels_y), 0);
            std::unique_ptr<wxGraphicsContext> raster(gc.GetRenderer()->CreateContextFromImage(image));
            if (!raster) {
                draw_paragraph(gc, text, dc.GetFont(), dpi, colors.ink, x, y, begin, end, colour);
                continue;
            }
            raster->Scale(scale, scale);
            // Grayscale coverage remains correct over any highlight/background colour.
            raster->SetAntialiasMode(wxANTIALIAS_DEFAULT);
#ifdef __WXMSW__
            // ClearType's RGB coverage assumes an opaque background. Tiles need
            // grayscale alpha so selections can be drawn underneath them.
            static_cast<Gdiplus::Graphics*>(raster->GetNativeContext())
                ->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
#endif
            draw_paragraph(*raster, text, dc.GetFont(), dpi, colors.ink, pad, pad - top, begin, end, colour);
            raster.reset(); // Image contexts finish writing their pixels on destruction.
            auto bitmap = gc.CreateBitmapFromImage(image);
            if (bitmap.IsNull()) {
                draw_paragraph(gc, text, dc.GetFont(), dpi, colors.ink, x, y, begin, end, colour);
                continue;
            }
            while (render_stats_.tile_bytes + bytes > tile_budget && !tile_order_.empty()) {
                const auto old = tiles_.find(tile_order_.front());
                render_stats_.tile_bytes -= old->second.bytes;
                tiles_.erase(old);
                tile_order_.pop_front();
            }
            tile_order_.push_back(key);
            found = tiles_.emplace(key, TextTile{bitmap, width, height, bytes, std::prev(tile_order_.end())})
                        .first;
            render_stats_.tile_bytes += bytes;
            ++render_stats_.tile_builds;
        } else {
            ++render_stats_.tile_hits;
            tile_order_.splice(tile_order_.end(), tile_order_, found->second.recency);
        }
        const auto& cached = found->second;
        // Ceil can add a partial pixel; preserve a one-to-one physical-pixel mapping.
        gc.DrawBitmap(cached.bitmap, x - pad, y + top - pad, std::ceil(cached.width * scale) / scale,
                      std::ceil(cached.height * scale) / scale);
    }
}
} // namespace ortho
