#pragma once
#include <mutex>
#include <vector>
#include <queue>

namespace BurpTUI {

/// Non-blocking thread-safe queue for producer-consumer between networking and TUI threads.
template<typename T>
class TsQueue {
public:
    void push(T item) {
        std::scoped_lock lk(mtx_);
        q_.push(std::move(item));
    }

    /// Drain all items. Never blocks. Returns empty vector if nothing available.
    std::vector<T> tryPopAll() {
        std::scoped_lock lk(mtx_);
        std::vector<T> out;
        out.reserve(q_.size());
        while (!q_.empty()) {
            out.push_back(std::move(q_.front()));
            q_.pop();
        }
        return out;
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lk(mtx_);
        return q_.empty();
    }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock lk(mtx_);
        return q_.size();
    }

private:
    mutable std::mutex mtx_;
    std::queue<T> q_;
};

} // namespace BurpTUI
