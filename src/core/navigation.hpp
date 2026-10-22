#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace ortho {
enum class Screen { Day, Reader, Bible, Plans, Settings, About };
constexpr bool auxiliary_screen(Screen screen) {
    return screen != Screen::Day && screen != Screen::Reader;
}

// Embedded page stack; stable IDs retain the corresponding windows across session navigation.
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
    void restore(const NavigationState& saved) {
        root_ = saved.root_;
        pages_ = saved.pages_;
        next_id_ = std::max(next_id_, saved.next_id_);
    }
    const std::vector<View>& pages() const {
        return pages_;
    }

private:
    Screen root_ = Screen::Day;
    ViewId next_id_ = 1;
    std::vector<View> pages_;
};

// A bounded path through this session, with no persisted browsing history.
class NavigationHistory {
public:
    using Id = std::size_t;
    Id current() const {
        return visits_[cursor_];
    }
    bool can_back() const {
        return cursor_ > 0;
    }
    bool can_forward() const {
        return cursor_ + 1 < visits_.size();
    }
    bool back() {
        if (!can_back())
            return false;
        --cursor_;
        return true;
    }
    bool forward() {
        if (!can_forward())
            return false;
        ++cursor_;
        return true;
    }
    Id visit() {
        visits_.resize(cursor_ + 1);
        visits_.push_back(next_++);
        if (visits_.size() > 64)
            visits_.erase(visits_.begin());
        cursor_ = visits_.size() - 1;
        return current();
    }
    bool contains(Id id) const {
        return std::find(visits_.begin(), visits_.end(), id) != visits_.end();
    }

private:
    std::vector<Id> visits_{0};
    std::size_t cursor_ = 0;
    Id next_ = 1;
};
} // namespace ortho
