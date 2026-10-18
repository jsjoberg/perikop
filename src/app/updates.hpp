#pragma once
#include "core/updater.hpp"
#include <memory>

namespace ortho {
// Lets Velopack finish an installation or update step that started the program. It never checks for,
// downloads, or applies an update; those wait for the user.
void start_updates();
// Updates from this repository's GitHub releases, or a backend that explains why this copy cannot update.
std::unique_ptr<UpdateBackend> make_update_backend();
} // namespace ortho
