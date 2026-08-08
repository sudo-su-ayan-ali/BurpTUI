#pragma once
#include "Store.hpp"
#include <mutex>

namespace BurpTUI {

/// In-memory store, thread-safe. Useful for testing / volatile sessions.
class MemoryStore final : public Store {
public:
    void save(const ProxyEntry& entry) override;
    std::vector<ProxyEntry> list() const override;
    std::optional<ProxyEntry> get(std::uint64_t id) const override;
    void clear() override;

private:
    mutable std::mutex      mtx_;
    std::vector<ProxyEntry> entries_;
    std::uint64_t           nextId_ = 1;
};

} // namespace BurpTUI
