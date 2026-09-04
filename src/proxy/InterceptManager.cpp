#include "proxy/InterceptManager.hpp"
#include <algorithm>

namespace BurpTUI {

InterceptManager& InterceptManager::instance() {
    static InterceptManager instance;
    return instance;
}

void InterceptManager::setInterceptEnabled(bool enabled) {
    interceptEnabled_.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        forwardAll();
    } else {
        std::function<void()> notify;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            notify = notifyCallback_;
        }
        if (notify) notify();
    }
}

void InterceptManager::interceptRequest(
    int id,
    bool isHttps,
    std::string host,
    std::uint16_t port,
    std::shared_ptr<HttpRequest> req,
    std::function<void(InterceptAction)> onDecision)
{
    auto item = std::make_shared<InterceptedRequest>();
    item->id = id;
    item->isHttps = isHttps;
    item->host = std::move(host);
    item->port = port;
    item->request = std::move(req);
    item->onDecision = std::move(onDecision);

    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
        notify = notifyCallback_;
    }
    if (notify) notify();
}

bool InterceptManager::hasPending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
}

std::size_t InterceptManager::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::shared_ptr<InterceptedRequest> InterceptManager::peekCurrent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return nullptr;
    return queue_.front();
}

bool InterceptManager::forwardCurrent() {
    std::shared_ptr<InterceptedRequest> item;
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        notify = notifyCallback_;
    }
    if (item && item->onDecision) {
        item->onDecision(InterceptAction::Forward);
    }
    if (notify) notify();
    return true;
}

bool InterceptManager::dropCurrent() {
    std::shared_ptr<InterceptedRequest> item;
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        notify = notifyCallback_;
    }
    if (item && item->onDecision) {
        item->onDecision(InterceptAction::Drop);
    }
    if (notify) notify();
    return true;
}

void InterceptManager::forwardAll() {
    std::deque<std::shared_ptr<InterceptedRequest>> toForward;
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        toForward = std::move(queue_);
        queue_.clear();
        notify = notifyCallback_;
    }
    for (auto& item : toForward) {
        if (item && item->onDecision) {
            item->onDecision(InterceptAction::Forward);
        }
    }
    if (notify) notify();
}

void InterceptManager::dropAll() {
    std::deque<std::shared_ptr<InterceptedRequest>> toDrop;
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        toDrop = std::move(queue_);
        queue_.clear();
        notify = notifyCallback_;
    }
    for (auto& item : toDrop) {
        if (item && item->onDecision) {
            item->onDecision(InterceptAction::Drop);
        }
    }
    if (notify) notify();
}

void InterceptManager::removeById(int id) {
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(queue_.begin(), queue_.end(), [id](const auto& item) {
            return item && item->id == id;
        });
        if (it != queue_.end()) {
            queue_.erase(it, queue_.end());
            notify = notifyCallback_;
        }
    }
    if (notify) notify();
}

void InterceptManager::setNotifyCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    notifyCallback_ = std::move(callback);
}

} // namespace BurpTUI
