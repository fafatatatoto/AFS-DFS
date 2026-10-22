// Server side of the async callback (long poll).
// Clients send CallbackList and keep running (inotify, other RPCs).
// Store/Delete bump `epoch_` and wake waiters. Writers never wait on this CV.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

class ChangeLog {
public:
    int epoch() const {
        std::lock_guard<std::mutex> g(mu_);
        return epoch_;
    }

    void notify_changed() {
        std::lock_guard<std::mutex> g(mu_);
        ++epoch_;
        cv_.notify_all();
    }

    // Returns true if a change arrived, false on timeout.
    // `client_epoch` is the last version this client has already seen.
    bool wait_until_newer(int client_epoch, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mu_);
        return cv_.wait_for(lock, timeout, [&] { return epoch_ != client_epoch; });
    }

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    int epoch_ = 0;
};
