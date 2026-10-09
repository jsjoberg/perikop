#include "ui/main_frame.hpp"
#include "ui/scripture_view.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <wx/dcmemory.h>

namespace ortho {
bool MainFrame::render_benchmark() {
    using Clock = std::chrono::steady_clock;
    bool ok = true;
    settings_.primary = "sv";
    settings_.theme = Theme::Light;
    settings_.font_size = 19;
    settings_.word_study = false;
    std::cout << std::fixed << std::setprecision(3);
    for (const auto* parallel : {"", "el", "en"}) {
        settings_.parallel = parallel;
        apply_settings(false);
        open_reading({ReadingKind::Gospel, {"John", {1, 1}, {1, 18}}});
        Layout();
        const auto size = scripture_->GetClientSize();
        const double scale = scripture_->GetContentScaleFactor();
        wxBitmap bitmap;
        if (!bitmap.CreateWithLogicalSize(size, scale))
            return false;
        wxMemoryDC dc(bitmap);
        std::cout << "Reader benchmark: sv" << (*parallel ? "/" : "") << parallel << ", viewport=" << size.x
                  << 'x' << size.y << ", scale=" << scale << ", dpi=" << scripture_->GetDPI().x << '\n';
        const auto report = [](const char* label, std::vector<double> samples) {
            std::sort(samples.begin(), samples.end());
            std::cout << label << ": median_ms=" << samples[samples.size() / 2]
                      << ", p99_ms=" << samples[(samples.size() * 99 - 1) / 100] << '\n';
        };
        const auto frame = [&](bool cached, double delta) {
            const auto begin = Clock::now();
            if (delta != 0)
                scripture_->scroll_by(delta);
            scripture_->render_to(dc, size, cached);
            return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        };
        // Prime both positions so the warm measurements contain no tile/layout construction.
        frame(true, 0);
        frame(true, 4);
        frame(true, -4);
        const auto before = scripture_->render_stats();
        for (const bool cached : {false, true}) {
            std::vector<double> samples;
            for (int i = 0; i < 240; ++i)
                samples.push_back(frame(cached, i % 2 == 0 ? 4 : -4));
            report(cached ? "Warm cached scroll" : "Warm direct scroll", std::move(samples));
        }
        const auto warm = scripture_->render_stats();
        if (warm.layout_builds != before.layout_builds || warm.tile_builds != before.tile_builds ||
            warm.tile_hits <= before.tile_hits)
            ok = false;
        std::vector<double> cold;
        for (int i = 0; i < 120; ++i)
            cold.push_back(frame(true, size.y / 3.0));
        report("First visit scroll (includes layout/raster misses)", std::move(cold));
        const auto after = scripture_->render_stats();
        std::cout << "Warm new layouts=" << warm.layout_builds - before.layout_builds
                  << ", warm new tiles=" << warm.tile_builds - before.tile_builds
                  << ", first visit new layouts=" << after.layout_builds - warm.layout_builds
                  << ", first visit new tiles=" << after.tile_builds - warm.tile_builds
                  << ", cached_MiB=" << after.tile_bytes / (1024.0 * 1024.0) << '\n';
        if (after.tile_bytes > std::size_t{32} * 1024 * 1024 || scripture_->cached_rows() > 192)
            ok = false;
    }
    // Exercise a quiet paused guide without loading or running a speech model.
    open_psalm();
    SpeechPlayback paused{SpeechState::Paused, SpeechCue{0, 0, "Ps", "sv1917", {23, 1}, {23, 1}, false}, 0,
                          0};
    scripture_->playback(paused);
    for (int i = 0; i < 120; ++i)
        scripture_->advance_playback(0.016);
    const auto settled = scripture_->render_stats();
    for (int i = 0; i < 120; ++i) {
        scripture_->playback(paused);
        scripture_->advance_playback(0.016);
    }
    const auto idle = scripture_->render_stats();
    const auto idle_repaints = idle.repaint_requests - settled.repaint_requests;
    std::cout << "Settled pause: repaint_requests=" << idle_repaints
              << ", animation_timer=" << scripture_->animating() << '\n';
    ok = ok && idle_repaints == 0 && !scripture_->animating();
    std::cout << (ok ? "Reader benchmark passed.\n" : "Reader benchmark failed.\n");
    return ok;
}
} // namespace ortho
