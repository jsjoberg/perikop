#pragma once
#include "core/model.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ortho {
// A version the update channel offers.
struct UpdateOffer {
    std::string version;
    std::uint64_t size = 0; // bytes to download
    bool downgrade = false;
};
// The platform's update system. check, download, and apply run on a worker thread, one at a time;
// the backend keeps the offer from check through download and apply.
class UpdateBackend {
public:
    virtual ~UpdateBackend() = default;
    // Why updates cannot run here, such as in a copy that was not installed; empty when they can.
    virtual std::string unavailable() const = 0;
    // The newest version on the channel, or none when this one is current. Throws on failure.
    virtual std::optional<UpdateOffer> check(UpdateChannel) = 0;
    // Downloads the offer, reporting percent. Throws on failure.
    virtual void download(const std::function<void(int)>& progress) = 0;
    // Installs the downloaded offer once the program has exited, and starts it again. Throws on failure.
    virtual void apply() = 0;
};
struct UpdateStatus {
    enum class State {
        Unavailable,
        Idle,
        Checking,
        Current,
        Available,
        Downloading,
        Cancelling,
        Ready,
        Failed
    };
    State state = State::Idle;
    UpdateOffer offer;   // Available, Downloading, Ready
    int progress = 0;    // Downloading
    std::string message; // Unavailable, Failed
};
// The update steps the user starts: check, download, apply. Nothing runs on its own.
// Work runs off the UI thread; post must run its function on the UI thread, where every change is reported.
class Updater {
public:
    using Post = std::function<void(std::function<void()>)>;
    Updater(std::unique_ptr<UpdateBackend> backend, Post post, std::function<void()> changed);
    ~Updater();
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;
    const UpdateStatus& status() const {
        return status_;
    }
    void check(UpdateChannel);
    void download();
    // Stops waiting. The SDK cannot stop a request, so it finishes unseen before another step can start.
    void cancel();
    // Forgets the offer, such as when the channel changes.
    void reset();
    // Starts the installation; the caller then closes the program. False if it could not start.
    bool apply();

private:
    struct Shared;
    using Result = std::function<void(UpdateStatus&)>;
    using Work = std::function<Result(UpdateBackend&, const std::function<void(int)>& progress)>;
    static void deliver(const std::shared_ptr<Shared>& shared, std::function<void()> function);
    void start(UpdateStatus::State during, UpdateStatus::State cancelled, Work work);
    std::shared_ptr<UpdateBackend> backend_;
    std::shared_ptr<Shared> shared_;
    std::function<void()> changed_;
    UpdateStatus status_;
    UpdateStatus::State cancelled_ = UpdateStatus::State::Idle;
    unsigned generation_ = 0;
    bool busy_ = false;
};
} // namespace ortho
