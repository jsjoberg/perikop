#include "core/navigation.hpp"
#include <stdexcept>

namespace ortho {
void NavigationState::show_root(Screen screen) {
    if (auxiliary_screen(screen) || auxiliary())
        throw std::logic_error("Close auxiliary pages before selecting a root screen");
    root_ = screen;
}
NavigationState::View NavigationState::push(Screen screen) {
    if (!auxiliary_screen(screen))
        throw std::logic_error("A root screen cannot be pushed as an auxiliary page");
    const View view{screen, next_id_++};
    pages_.push_back(view);
    return view;
}
std::optional<NavigationState::View> NavigationState::pop() {
    if (pages_.empty())
        return {};
    const auto view = pages_.back();
    pages_.pop_back();
    return view;
}
} // namespace ortho
