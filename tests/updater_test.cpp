#include "core/updater.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using namespace ortho;
using State = UpdateStatus::State;
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
// The UI thread: functions the worker posts wait here until the test runs them.
class Queue {
public:
    Updater::Post post() {
        return [this](std::function<void()> function) {
            const std::lock_guard lock(mutex_);
            functions_.push_back(std::move(function));
            ready_.notify_all();
        };
    }
    // Runs posted functions until the condition holds.
    void run_until(const std::function<bool()>& done) {
        while (!done()) {
            std::unique_lock lock(mutex_);
            if (!ready_.wait_for(lock, std::chrono::seconds(10), [this] {
                    return !functions_.empty();
                }))
                throw std::runtime_error("Timed out waiting for the update worker");
            auto function = std::move(functions_.front());
            functions_.pop_front();
            lock.unlock();
            function();
        }
    }
    std::size_t waiting() {
        const std::lock_guard lock(mutex_);
        return functions_.size();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> functions_;
};
// Each step waits for the test to release it, so the test controls when work finishes.
class Fake final : public UpdateBackend {
public:
    std::string reason;
    std::optional<UpdateOffer> offer = UpdateOffer{"0.1.1", 1000, false};
    bool fail = false;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    int checks = 0, downloads = 0, applies = 0;
    std::string unavailable() const override {
        return reason;
    }
    std::optional<UpdateOffer> check(UpdateChannel) override {
        released.wait();
        ++checks;
        if (fail)
            throw std::runtime_error("Ingen anslutning");
        return offer;
    }
    void download(const std::function<void(int)>& progress) override {
        released.wait();
        ++downloads;
        progress(50);
        progress(100);
    }
    void apply() override {
        ++applies;
    }
};
struct Setup {
    Queue queue;
    Fake* fake;
    int changes = 0;
    std::unique_ptr<Updater> updater;
    explicit Setup(std::unique_ptr<Fake> backend = std::make_unique<Fake>()) : fake(backend.get()) {
        updater = std::make_unique<Updater>(std::move(backend), queue.post(), [this] {
            ++changes;
        });
    }
    State state() const {
        return updater->status().state;
    }
};

void check_download_and_apply() {
    Setup setup;
    setup.fake->release.set_value();
    setup.updater->check(UpdateChannel::Nightly);
    check(setup.state() == State::Checking, "checking is shown at once");
    setup.queue.run_until([&] {
        return setup.state() != State::Checking;
    });
    check(setup.state() == State::Available && setup.updater->status().offer.version == "0.1.1",
          "the offered version is shown");
    setup.updater->download();
    int highest = 0;
    setup.queue.run_until([&] {
        highest = std::max(highest, setup.updater->status().progress);
        return setup.state() != State::Downloading;
    });
    check(setup.state() == State::Ready && highest == 100, "progress reaches 100 and the download is ready");
    check(setup.updater->apply() && setup.fake->applies == 1, "apply starts the installation");
}
void check_current_and_failure() {
    auto current = std::make_unique<Fake>();
    current->offer.reset();
    Setup up_to_date(std::move(current));
    up_to_date.fake->release.set_value();
    up_to_date.updater->check(UpdateChannel::Releases);
    up_to_date.queue.run_until([&] {
        return up_to_date.state() != State::Checking;
    });
    check(up_to_date.state() == State::Current, "no offer means this version is current");
    up_to_date.updater->download();
    check(up_to_date.state() == State::Current, "nothing downloads without an offer");

    auto failing = std::make_unique<Fake>();
    failing->fail = true;
    Setup failed(std::move(failing));
    failed.fake->release.set_value();
    failed.updater->check(UpdateChannel::Releases);
    failed.queue.run_until([&] {
        return failed.state() != State::Checking;
    });
    check(failed.state() == State::Failed && failed.updater->status().message == "Ingen anslutning",
          "a failure is reported with its reason");
    check(!failed.updater->apply(), "nothing can be applied after a failure");
}
void check_unavailable() {
    auto installed = std::make_unique<Fake>();
    installed->reason = "Inte installerad";
    Setup setup(std::move(installed));
    setup.updater->check(UpdateChannel::Releases);
    check(setup.state() == State::Unavailable && setup.updater->status().message == "Inte installerad" &&
              setup.queue.waiting() == 0,
          "an uninstalled copy never starts a check");
}
void check_cancel() {
    Setup setup;
    setup.fake->release.set_value();
    setup.updater->check(UpdateChannel::Releases);
    setup.queue.run_until([&] {
        return setup.state() == State::Available;
    });
    // A new gate holds the download until the test has cancelled it.
    setup.fake->release = {};
    setup.fake->released = setup.fake->release.get_future().share();
    setup.updater->download();
    setup.updater->cancel();
    check(setup.state() == State::Cancelling, "cancelling is shown while the request finishes");
    setup.updater->check(UpdateChannel::Releases);
    check(setup.state() == State::Cancelling, "no other step starts until the request has finished");
    setup.fake->release.set_value();
    setup.queue.run_until([&] {
        return setup.state() != State::Cancelling;
    });
    check(setup.state() == State::Available && setup.fake->downloads == 1,
          "a cancelled download leaves the offer, and its result is discarded");
    setup.updater->reset();
    check(setup.state() == State::Idle, "a channel change forgets the offer");
}
void check_destroyed_while_working() {
    auto backend = std::make_unique<Fake>();
    auto* fake = backend.get();
    Queue queue;
    auto updater = std::make_unique<Updater>(std::move(backend), queue.post(), [] {});
    updater->check(UpdateChannel::Releases);
    updater.reset();
    fake->release.set_value();
    // The worker still owns the backend; give it time to finish and post nothing.
    for (int i = 0; i < 100 && queue.waiting() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    check(queue.waiting() == 0, "a worker that outlives the updater delivers nothing");
}
} // namespace

int main() {
    try {
        check_download_and_apply();
        check_current_and_failure();
        check_unavailable();
        check_cancel();
        check_destroyed_while_working();
        std::cout << "Update steps passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
