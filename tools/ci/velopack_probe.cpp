// Manual Linux compatibility probe, kept separate from Perikop's production startup.
#include <Velopack.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef PROBE_VERSION
#error PROBE_VERSION must identify the packaged test version
#endif

int main(int argc, char** argv) {
    // Initialisation must neither request updates nor apply previously downloaded packages.
    Velopack::VelopackApp::Build().SetAutoApplyOnStartup(false).Run();
    try {
        if (argc == 2 && std::string(argv[1]) == "version") {
            std::cout << "VERSION " << PROBE_VERSION << '\n';
            return 0;
        }
        if (argc != 4)
            throw std::runtime_error("Usage: probe version | check/stage/session SOURCE CHANNEL");
        const std::string command = argv[1];
        if (command != "check" && command != "stage" && command != "session")
            throw std::runtime_error("Unknown probe command");
        Velopack::UpdateOptions options{};
        options.ExplicitChannel = argv[3];
        options.AllowVersionDowngrade = true; // Only these explicit test commands can switch channels.
        options.MaximumDeltasBeforeFallback = 10;
        Velopack::UpdateManager manager(argv[2], &options);
        const auto update = manager.CheckForUpdates();
        if (!update) {
            std::cout << "NO_UPDATE\n";
            return 0;
        }
        std::cout << "AVAILABLE " << update->TargetFullRelease.Version << " DOWNGRADE " << update->IsDowngrade
                  << '\n';
        if (command == "stage") {
            manager.DownloadUpdates(*update);
            std::cout << "STAGED\n";
        }
        if (command == "session") {
            // Keep the selected asset from check through stage and apply, as the UI must do.
            // SDK startup cleans up older cached packages even with auto-apply disabled, so a
            // staged downgrade cannot be assumed to survive an intervening app launch.
            bool staged = false;
            std::string action;
            while (std::getline(std::cin, action)) {
                if (action == "stage") {
                    manager.DownloadUpdates(*update);
                    staged = true;
                    std::cout << "STAGED\n";
                } else if (action == "version")
                    std::cout << "VERSION " << PROBE_VERSION << '\n';
                else if (action == "apply") {
                    if (!staged)
                        throw std::runtime_error("Stage the selected target before applying it");
                    manager.WaitExitThenApplyUpdates(*update, true, false);
                    std::cout << "APPLY_REQUESTED\n";
                    return 0;
                } else
                    throw std::runtime_error("Unknown session action");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
