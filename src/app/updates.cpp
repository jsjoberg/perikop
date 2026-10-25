#include "app/updates.hpp"
#include <iostream>
#include <stdexcept>
#ifdef PERIKOP_UPDATES
#include <Velopack.hpp>
#endif

namespace ortho {
namespace {
const char* const not_installed = "Den här kopian uppdateras inte inifrån programmet. Nya versioner finns på "
                                  "github.com/jsjoberg/perikop/releases.";
#ifdef PERIKOP_UPDATES
const char* const repository = "https://github.com/jsjoberg/perikop";
// Linux feeds include the glibc baseline so older clients cannot receive incompatible AppImages.
#if defined(_WIN32) && defined(__aarch64__)
const char* const platform = "win-arm64";
#elif defined(_WIN32)
const char* const platform = "win-x64";
#elif defined(__APPLE__)
const char* const platform = "osx-arm64";
#elif defined(__aarch64__)
const char* const platform = "linux-arm64-glibc239";
#else
const char* const platform = "linux-x64-glibc239";
#endif

// The manager finds an installed program from its files alone, with no network; it throws when there is none.
bool installed() {
    try {
        Velopack::UpdateManager manager(std::make_unique<Velopack::GithubSource>(repository));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

class VelopackBackend final : public UpdateBackend {
public:
    // Where releases come from: GitHub for the program, a local directory for the update probe.
    using Source = std::function<std::unique_ptr<Velopack::IUpdateSourcePointer>(bool nightly)>;
    VelopackBackend(Source source, bool restart) : source_(std::move(source)), restart_(restart) {
        if (!installed())
            reason_ = not_installed;
    }
    std::string installed_version() const {
        return manager_ ? manager_->GetCurrentVersion() : std::string();
    }
    std::string unavailable() const override {
        return reason_;
    }
    std::optional<UpdateOffer> check(UpdateChannel channel) override {
        const bool nightly = channel == UpdateChannel::Nightly;
        Velopack::UpdateOptions options{};
        options.ExplicitChannel = std::string(platform) + (nightly ? "-nightly" : "-releases");
        // Switching channel may offer an older version; the user confirms it before it downloads.
        options.AllowVersionDowngrade = true;
        options.MaximumDeltasBeforeFallback = 10;
        manager_ = std::make_unique<Velopack::UpdateManager>(source_(nightly), &options);
        update_ = manager_->CheckForUpdates();
        if (!update_)
            return std::nullopt;
        std::uint64_t size = update_->TargetFullRelease.Size;
        if (!update_->DeltasToTarget.empty()) {
            size = 0;
            for (const auto& delta : update_->DeltasToTarget)
                size += delta.Size;
        }
        return UpdateOffer{update_->TargetFullRelease.Version, size, update_->IsDowngrade};
    }
    void download(const std::function<void(int)>& progress) override {
        if (!manager_ || !update_)
            throw std::runtime_error("Ingen version är vald");
        manager_->DownloadUpdates(
            *update_,
            [](void* report, size_t percent) {
                (*static_cast<const std::function<void(int)>*>(report))(int(percent));
            },
            const_cast<std::function<void(int)>*>(&progress));
    }
    void apply() override {
        if (!manager_ || !update_)
            throw std::runtime_error("Ingen version är vald");
        manager_->WaitExitThenApplyUpdates(*update_, !restart_, restart_);
    }

private:
    Source source_;
    bool restart_;
    std::string reason_;
    // The selected package stays with the manager that found it, from check through download and apply.
    std::unique_ptr<Velopack::UpdateManager> manager_;
    std::optional<Velopack::UpdateInfo> update_;
};
#else
class UnavailableBackend final : public UpdateBackend {
public:
    std::string unavailable() const override {
        return not_installed;
    }
    std::optional<UpdateOffer> check(UpdateChannel) override {
        return std::nullopt;
    }
    void download(const std::function<void(int)>&) override {}
    void apply() override {}
};
#endif
} // namespace

void start_updates() {
#ifdef PERIKOP_UPDATES
    // Run also starts Velopack's log file, so a copy that Velopack did not install writes nothing.
    if (installed())
        Velopack::VelopackApp::Build().SetAutoApplyOnStartup(false).Run();
#endif
}
std::unique_ptr<UpdateBackend> make_update_backend() {
#ifdef PERIKOP_UPDATES
    return std::make_unique<VelopackBackend>(
        [](bool nightly) {
            // The GitHub source sends no identifier; nightlies are prereleases.
            return std::make_unique<Velopack::GithubSource>(repository, "", nightly);
        },
        true);
#else
    return std::make_unique<UnavailableBackend>();
#endif
}
int update_probe(const std::string& feed, UpdateChannel channel) {
#ifdef PERIKOP_UPDATES
    VelopackBackend backend(
        [feed](bool) {
            return std::make_unique<Velopack::FileSource>(feed);
        },
        false);
    if (!backend.unavailable().empty()) {
        std::cerr << backend.unavailable() << '\n';
        return 1;
    }
    try {
        const auto offer = backend.check(channel);
        std::cout << "VERSION " << backend.installed_version() << '\n';
        if (!offer) {
            std::cout << "NO_UPDATE\n";
            return 0;
        }
        std::cout << "AVAILABLE " << offer->version << '\n';
        backend.download([](int) {});
        backend.apply();
        std::cout << "APPLYING\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
#else
    (void)feed;
    (void)channel;
    std::cerr << not_installed << '\n';
    return 1;
#endif
}
} // namespace ortho
