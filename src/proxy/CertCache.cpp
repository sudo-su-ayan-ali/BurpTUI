#include "proxy/CertCache.hpp"

namespace BurpTUI {

CertCache::CertCache(CertGenerator& generator, std::size_t maxEntries)
    : gen_(generator), maxEntries_(maxEntries)
{}

CertGenerator::CertKeyPair CertCache::get(const std::string& hostname) {
    std::scoped_lock lk(mtx_);
    auto it = cache_.find(hostname);
    if (it != cache_.end()) return it->second;
    // Evict oldest entry when full (simple FIFO eviction for now)
    if (cache_.size() >= maxEntries_)
        cache_.erase(cache_.begin());
    auto pair = gen_.generate(hostname);
    cache_.emplace(hostname, pair);
    return pair;
}

} // namespace BurpTUI
