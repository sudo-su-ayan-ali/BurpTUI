#include "storage/MemoryStore.hpp"
#include <algorithm>

namespace BurpTUI {

void MemoryStore::save(const ProxyEntry& entry) {
    std::scoped_lock lk(mtx_);
    ProxyEntry e = entry;
    if (e.id == 0) e.id = nextId_++;
    entries_.push_back(std::move(e));
}

std::vector<ProxyEntry> MemoryStore::list() const {
    std::scoped_lock lk(mtx_);
    return entries_;
}

std::optional<ProxyEntry> MemoryStore::get(std::uint64_t id) const {
    std::scoped_lock lk(mtx_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [id](const ProxyEntry& e){ return e.id == id; });
    if (it == entries_.end()) return std::nullopt;
    return *it;
}

void MemoryStore::clear() {
    std::scoped_lock lk(mtx_);
    entries_.clear();
    nextId_ = 1;
}

} // namespace BurpTUI
