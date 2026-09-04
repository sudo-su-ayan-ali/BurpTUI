#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <atomic>
#include <functional>
#include <deque>
#include <mutex>
#include "http/HttpRequest.hpp"

namespace BurpTUI {

enum class InterceptAction {
    Forward,
    Drop
};

struct InterceptedRequest {
    int id{0};
    bool isHttps{false};
    std::string host;
    std::uint16_t port{80};
    std::shared_ptr<HttpRequest> request;
    std::function<void(InterceptAction)> onDecision;
};

class InterceptManager {
public:
    static InterceptManager& instance();

    bool isInterceptEnabled() const {
        return interceptEnabled_.load(std::memory_order_relaxed);
    }

    void setInterceptEnabled(bool enabled);

    /// Intercept a request and pause its session until forward or drop decision.
    void interceptRequest(
        int id,
        bool isHttps,
        std::string host,
        std::uint16_t port,
        std::shared_ptr<HttpRequest> req,
        std::function<void(InterceptAction)> onDecision);

    /// Check if there are requests waiting in the queue.
    bool hasPending() const;

    /// Number of queued requests.
    std::size_t pendingCount() const;

    /// Returns a copy of the front intercepted request (for UI display), or nullptr if empty.
    std::shared_ptr<InterceptedRequest> peekCurrent() const;

    /// Forward the front request in the queue.
    bool forwardCurrent();

    /// Drop the front request in the queue.
    bool dropCurrent();

    /// Forward all pending requests (e.g. when turning intercept OFF).
    void forwardAll();

    /// Drop all pending requests.
    void dropAll();

    /// Remove a request by transaction ID (e.g. if client socket disconnected).
    void removeById(int id);

    /// Set UI notification callback to trigger FTXUI redraw.
    void setNotifyCallback(std::function<void()> callback);

private:
    InterceptManager() = default;
    ~InterceptManager() = default;

    InterceptManager(const InterceptManager&) = delete;
    InterceptManager& operator=(const InterceptManager&) = delete;

    std::atomic<bool> interceptEnabled_{false};
    mutable std::mutex mutex_;
    std::deque<std::shared_ptr<InterceptedRequest>> queue_;
    std::function<void()> notifyCallback_;
};

} // namespace BurpTUI
