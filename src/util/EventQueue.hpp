#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

namespace BurpTUI {

/// Thread-safe, bounded MPSC event queue.
template<typename T>
class EventQueue {
public:
    explicit EventQueue(std::size_t maxSize = 4096) : maxSize_(maxSize) {}

    void push(T item) {
        std::unique_lock lk(mtx_);
        cv_.wait(lk, [&]{ return q_.size() < maxSize_; });
        q_.push(std::move(item));
        lk.unlock();
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock lk(mtx_);
        cv_.wait(lk, [&]{ return !q_.empty(); });
        T item = std::move(q_.front());
        q_.pop();
        lk.unlock();
        cv_.notify_one();
        return item;
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lk(mtx_);
        return q_.empty();
    }

private:
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::queue<T>           q_;
    std::size_t             maxSize_;
};

} // namespace BurpTUI
