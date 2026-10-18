#include "app/updates.hpp"
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
// Channels are <os>-<arch>-<track>; see docs/updating.md.
#if defined(__APPLE__) && defined(__aarch64__)
const char* const platform = "osx-arm64";
#elif defined(__APPLE__)
const char* const platform = "osx-x64";
#elif defined(__aarch64__)
const char* const platform = "linux-arm64";
#else
const char* const platform = "linux-x64";
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
    VelopackBackend() {
        if (!installed())
            reason_ = not_installed;
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
        // The GitHub source sends no identifier; nightlies are prereleases.
        manager_ = std::make_unique<Velopack::UpdateManager>(
            std::make_unique<Velopack::GithubSource>(repository, "", nightly), &options);
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
        manager_->WaitExitThenApplyUpdates(*update_, false, true);
    }

private:
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
    return std::make_unique<VelopackBackend>();
#else
    return std::make_unique<UnavailableBackend>();
#endif
}
} // namespace ortho
