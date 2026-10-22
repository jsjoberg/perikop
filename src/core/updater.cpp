#include "core/updater.hpp"
#include <exception>
#include <mutex>
#include <thread>

namespace ortho {
using State = UpdateStatus::State;
// Outlives the Updater while a worker runs, so late results are dropped instead of reaching a destroyed one.
struct Updater::Shared {
    std::mutex mutex;
    bool alive = true;
    Post post;
};
void Updater::deliver(const std::shared_ptr<Shared>& shared, std::function<void()> function) {
    const std::lock_guard lock(shared->mutex);
    if (shared->alive)
        shared->post([shared, function = std::move(function)] {
            // alive changes only on the UI thread, where this runs.
            if (shared->alive)
                function();
        });
}

Updater::Updater(std::unique_ptr<UpdateBackend> backend, Post post, std::function<void()> changed)
    : backend_(std::move(backend)), shared_(std::make_shared<Shared>()), changed_(std::move(changed)) {
    shared_->post = std::move(post);
    status_.message = backend_->unavailable();
    if (!status_.message.empty())
        status_.state = State::Unavailable;
}
Updater::~Updater() {
    const std::lock_guard lock(shared_->mutex);
    shared_->alive = false;
}
void Updater::start(State during, State cancelled, Work work) {
    busy_ = true;
    cancelled_ = cancelled;
    status_.state = during;
    status_.progress = 0;
    status_.message.clear();
    changed_();
    const auto generation = ++generation_;
    std::thread([this, shared = shared_, backend = backend_, work = std::move(work), generation] {
        const auto progress = [&](int percent) {
            deliver(shared, [this, generation, percent] {
                if (generation_ == generation && status_.state == State::Downloading) {
                    status_.progress = percent;
                    changed_();
                }
            });
        };
        Result result;
        try {
            result = work(*backend, progress);
        } catch (const std::exception& error) {
            result = [message = std::string(error.what())](UpdateStatus& status) {
                status.state = State::Failed;
                status.message = message.empty() ? "Okänt fel" : message;
            };
        } catch (...) {
            // Anything else would end the program from this detached thread and leave busy_ set.
            result = [](UpdateStatus& status) {
                status.state = State::Failed;
                status.message = "Okänt fel";
            };
        }
        deliver(shared, [this, generation, result = std::move(result)] {
            busy_ = false;
            if (generation_ == generation)
                result(status_);
            else if (status_.state == State::Cancelling)
                status_.state = cancelled_;
            changed_();
        });
    }).detach();
}
void Updater::check(UpdateChannel channel) {
    if (busy_ || status_.state == State::Unavailable || status_.state == State::Ready)
        return;
    start(State::Checking, State::Idle, [channel](UpdateBackend& backend, const auto&) -> Result {
        auto offer = backend.check(channel);
        return [offer = std::move(offer)](UpdateStatus& status) {
            status.state = offer ? State::Available : State::Current;
            status.offer = offer.value_or(UpdateOffer{});
        };
    });
}
void Updater::download() {
    if (busy_ || status_.state != State::Available)
        return;
    start(State::Downloading, State::Available, [](UpdateBackend& backend, const auto& progress) -> Result {
        backend.download(progress);
        return [](UpdateStatus& status) {
            status.state = State::Ready;
        };
    });
}
void Updater::cancel() {
    if (status_.state != State::Checking && status_.state != State::Downloading)
        return;
    ++generation_;
    status_.state = State::Cancelling;
    changed_();
}
void Updater::reset() {
    if (status_.state == State::Unavailable)
        return;
    if (busy_) {
        cancel();
        cancelled_ = State::Idle;
        return;
    }
    status_ = {};
    changed_();
}
bool Updater::apply() {
    if (busy_ || status_.state != State::Ready)
        return false;
    try {
        backend_->apply();
        return true;
    } catch (const std::exception& error) {
        status_.state = State::Failed;
        status_.message = error.what();
        changed_();
        return false;
    }
}
} // namespace ortho
