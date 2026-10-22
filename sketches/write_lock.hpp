// Exclusive whole-file writer lock.
// One holder at a time. Re-acquire by the same client is a no-op (retries).
// Not a lease: a crashed holder can pin the file. That is a known gap.

#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class WriteLockTable {
public:
    enum class Result { Granted, Busy };

    Result acquire(const std::string& file, const std::string& client) {
        std::lock_guard<std::mutex> g(mu_);
        auto& slot = slots_[file];
        if (!slot.held) {
            slot.held = true;
            slot.owner = client;
            return Result::Granted;
        }
        if (slot.owner == client) {
            return Result::Granted;  // idempotent for the current holder
        }
        return Result::Busy;
    }

    void release(const std::string& file, const std::string& client) {
        std::lock_guard<std::mutex> g(mu_);
        auto it = slots_.find(file);
        if (it == slots_.end()) return;
        if (it->second.owner == client) {
            it->second.held = false;
            it->second.owner.clear();
        }
    }

private:
    struct Slot {
        bool held = false;
        std::string owner;
    };

    std::mutex mu_;
    std::unordered_map<std::string, Slot> slots_;
};
