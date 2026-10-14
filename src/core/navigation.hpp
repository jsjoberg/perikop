#pragma once
#include <cstddef>
#include <optional>
#include <vector>

namespace ortho {
enum class Screen { Day, Reader, Bible, Plans, About };
constexpr bool auxiliary_screen(Screen screen) {
    return screen != Screen::Day && screen != Screen::Reader;
}

// Navigation owns the return history; view IDs let the UI retain the corresponding windows.
class NavigationState {
public:
    using ViewId = std::size_t;
    struct View {
        Screen screen;
        ViewId id;
        bool operator==(const View&) const = default;
    };
    View current() const {
        return pages_.empty() ? View{root_, 0} : pages_.back();
    }
    bool reader() const {
        return current().screen == Screen::Reader;
    }
    bool auxiliary() const {
        return !pages_.empty();
    }
    std::size_t history_size() const {
        return pages_.empty() ? 0 : pages_.size() - 1;
    }
    void show_root(Screen);
    View push(Screen);
    std::optional<View> pop();

private:
    Screen root_ = Screen::Day;
    ViewId next_id_ = 1;
    std::vector<View> pages_;
};
} // namespace ortho
