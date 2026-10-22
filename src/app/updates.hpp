#pragma once
#include "core/updater.hpp"
#include <memory>
#include <string>

namespace ortho {
// Lets Velopack finish an installation or update step that started the program. It never checks for,
// downloads, or applies an update; those wait for the user.
void start_updates();
// Updates from this repository's GitHub releases, or a backend that explains why this copy cannot update.
std::unique_ptr<UpdateBackend> make_update_backend();
// Diagnostic: updates an installed copy from a local feed directory on the given channel, as the
// update controls do, without restarting it. Prints the steps and returns the exit status.
int update_probe(const std::string& feed, UpdateChannel);
} // namespace ortho
